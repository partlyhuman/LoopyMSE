#include "printer/printer.h"

#include <algorithm> 
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <thread>
#include <vector>

#include <common/imgwriter.h>
#include <core/sh2/sh2_bus.h>
#include <core/sh2/sh2_local.h>
#include <log/log.h>

namespace fs = std::filesystem;
namespace imagew = Common::ImageWriter;

namespace Printer
{

constexpr static uint32_t ADDR_MOTOR_MOVE = 0x00001B76;
constexpr static uint32_t ADDR_PRINT = 0x000006D4;

constexpr static int PRINT_STATUS_SUCCESS = 0;
constexpr static int PRINT_STATUS_GENERAL_FAILURE = 1;
constexpr static int PRINT_STATUS_NO_SEAL_CART = 2;
constexpr static int PRINT_STATUS_CANCELLED = 3;
constexpr static int PRINT_STATUS_PAPER_JAM = 4;
constexpr static int PRINT_STATUS_OVERHEAT = 5;

static fs::path output_dir;
static int output_type;
static std::string view_command;

using namespace SH2;

void system_threadwrapper(std::string command)
{
	system(command.c_str());
}

void show_print_file(fs::path print_path)
{
	if (view_command.empty() || print_path.empty()) return;

	std::stringstream cmd;

	if (view_command == "(OPEN)")
	{
		bool has_command = false;
#ifdef _WIN32
		// Windows: system call: start "" <path>
		cmd << "start " << "\"\" " << print_path;
		has_command = true;
#elif __APPLE__ && __MACH__
		// OSX: system call: open <path> &
		cmd << "open " << print_path << " &";
		has_command = true;
#elif __linux__
		// Linux: system call: xdg-open <path> &
		cmd << "xdg-open " << print_path << " &";
		has_command = true;
#endif
		if (has_command)
		{
			Log::info("[Printer] trying to open print in default viewer...");
			std::thread t1(system_threadwrapper, cmd.str());
			t1.detach();
		}
		else
		{
			Log::info("[Printer] default viewer not supported on this platform");
		}
		return;
	}

	std::string file_placeholder = "$FILE";

	std::stringstream print_path_quoted_tmp;
	print_path_quoted_tmp << print_path;
	std::string print_path_quoted = print_path_quoted_tmp.str();

	std::string view_command_sub = view_command;
	bool has_placeholder = false;

	// Replace placeholder with quoted path
	size_t placeholder_pos = view_command_sub.find(file_placeholder);
	while (placeholder_pos != std::string::npos)
	{
		has_placeholder = true;
		view_command_sub.replace(placeholder_pos, file_placeholder.length(), print_path_quoted);
		placeholder_pos = view_command_sub.find(file_placeholder, placeholder_pos + print_path_quoted.length());
	}

	cmd << view_command_sub;
	if (!has_placeholder)
	{
		cmd << " " << print_path_quoted;
	}

	Log::info("[Printer] trying to open print with your specified view command...");	
	std::thread t1(system_threadwrapper, cmd.str());
	t1.detach();
}

template <typename T>
std::vector<T> double_pixel_data(std::vector<T> data, uint32_t width, uint32_t height)
{
	std::vector<T> data_doubled(width * height * 4);
	for (int y = 0; y < height*2; y++)
	{
		for (int x = 0; x < width*2; x++)
		{
			data_doubled[y * (width * 2) + x] = data[(y/2) * width + (x/2)];
		}
	}
	return data_doubled;
}

bool motor_move_hook(uint32_t addr)
{
	if (addr != ADDR_MOTOR_MOVE) return false;
	//Hook slow moving printer function and skip it for faster boot

	//We're at 1B7A executing 1B76; jump to 15FA to exit function immediately
	Log::info("[Printer] skipping motor move...");
	sh2.pc = 0x15FA;
	sh2.pipeline_valid = false;
	return true;
}

bool print_hook(uint32_t addr)
{
	if (addr != ADDR_PRINT) return false;
	//Hook the BIOS print function

	uint32_t sp = sh2.gpr[15];
	uint32_t p1_data    = Bus::read32(sh2.gpr[4]);
	uint32_t p2_palette = Bus::read32(sh2.gpr[5]);
	uint32_t p3_dims    = Bus::read32(sh2.gpr[6]);
	uint32_t p4_unk     = sh2.gpr[7];
	uint32_t p5_unk     = Bus::read32(sp);
	uint32_t p6_format  = Bus::read8(Bus::read32(sp+4));
	uint32_t p7_unk     = Bus::read32(sp+8);
	uint32_t p8_first   = Bus::read32(sp+12);
	Log::debug("[Printer] data=%08X, palette=%08X, dims=%08X, unkp4=%08X, unkp5=%08X, format=%02X, unkp7=%08X, first=%d",
		p1_data, p2_palette, p3_dims, p4_unk, p5_unk, p6_format, p7_unk, p8_first);
	
	if (output_dir.empty())
	{
		sh2.gpr[0] = PRINT_STATUS_NO_SEAL_CART;
		return true;
	}
	
	bool print_success;

	// Dump the data to be printed
	uint32_t width = p3_dims & 0xFFFF;
	uint32_t height = p3_dims >> 16;

	int pixel_double = p6_format >> 4;
	int pixel_format = p6_format & 15;

	//TODO: is there more complex logic to this?
	height = std::min(height, (uint32_t)(pixel_double == 1 ? 112 : 224));

	Log::info("[Printer] size=%dx%d, pixel_format=%d, pixel_double=%d", width, height, pixel_format, pixel_double);

	if ((pixel_double == 0 || pixel_double == 1) && (pixel_format == 1 || pixel_format == 3))
	{
		fs::path print_name = imagew::make_unique_name("print_");
		print_name += imagew::image_extension(output_type);
		fs::path print_path = fs::absolute(output_dir) / print_name;

		if (pixel_format == 3)
		{
			std::vector<uint8_t> data(width * height);
			uint16_t palette[256];

			for (int i = 0; i < (width * height); i++)
			{
				data[i] = Bus::read8(p1_data + i);
			}
			for (int p = 0; p < 256; p++)
			{
				palette[p] = Bus::read16(p2_palette + (p * 2));
			}

			if (pixel_double == 1)
			{
				std::vector<uint8_t> data_doubled = double_pixel_data<uint8_t>(data, width, height);
				print_success = imagew::save_image_8bpp(output_type, print_path, width*2, height*2, &data_doubled[0], 256, palette);
			}
			else
			{
				print_success = imagew::save_image_8bpp(output_type, print_path, width, height, &data[0], 256, palette);
			}
		}
		if (pixel_format == 1)
		{
			std::vector<uint16_t> data(width * height);

			for (int i = 0; i < (width * height); i++)
			{
				data[i] = Bus::read16(p1_data + (i * 2));
			}
			
			if (pixel_double == 1)
			{
				std::vector<uint16_t> data_doubled = double_pixel_data<uint16_t>(data, width, height);
				print_success = imagew::save_image_16bpp(output_type, print_path, width*2, height*2, &data_doubled[0]);
			}
			else
			{
				print_success = imagew::save_image_16bpp(output_type, print_path, width, height, &data[0]);
			}
		}

		if (print_success)
		{
			Log::info("[Printer] saved print to %s", print_name.string().c_str());
			show_print_file(print_path);
		}
		else
		{
			Log::warn("[Printer] failed to open %s", print_name.string().c_str());
		}
	}
	else
	{
		Log::warn("[Printer] unknown mode, aborting");
		print_success = false;
	}

	//Return from print function with success or paper-out status

	//We're at 6D8 executing 6D4; set return code and jump to FD2 to exit function immediately
	sh2.gpr[0] = print_success ? PRINT_STATUS_SUCCESS : PRINT_STATUS_GENERAL_FAILURE;
	sh2.pc = 0xFD2;
	sh2.pipeline_valid = false;
	return false;
}

void initialize(Config::SystemInfo& config)
{
	output_dir = config.emulator.image_save_directory;
	output_type = config.emulator.printer_image_type;

	view_command = config.emulator.printer_view_command;
	view_command.erase(view_command.begin(), std::find_if(view_command.begin(), view_command.end(), [](unsigned char ch) { return !std::isspace(ch); }));
	view_command.erase(std::find_if(view_command.rbegin(), view_command.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), view_command.end());

	SH2::add_hook(ADDR_MOTOR_MOVE, &motor_move_hook);
	SH2::add_hook(ADDR_PRINT, &print_hook);
	Log::debug("[Printer] registered hooks for print and motor-move BIOS calls");
}

void shutdown()
{
	output_dir.clear();

	SH2::remove_hook(ADDR_MOTOR_MOVE);
	SH2::remove_hook(ADDR_PRINT);
	Log::debug("[Printer] unregistered hooks");
}

}