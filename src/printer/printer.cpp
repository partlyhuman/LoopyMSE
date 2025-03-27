#include "printer/printer.h"

#include <vector>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <cmath>

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

using namespace SH2;

void show_print_file(fs::path print_path)
{
	std::stringstream cmd;
	bool has_command = false;

#ifdef _WIN32
	// Windows: system call: start "" <path>
	cmd << "start " << "\"\" " << print_path;
	has_command = true;
#elif __APPLE__ && __MACH__
	// OSX: system call: open <path>
	cmd << "open " << print_path;
	has_command = true;
#elif __linux__
	// Linux: system call: xdg-open <path>
	cmd << "xdg-open " << print_path;
	has_command = true;
#endif

	if (has_command)
	{
		Log::info("[Printer] trying to open print in system viewer...");	
		system(cmd.str().c_str());
	}
	else
	{
		Log::info("[Printer] can't open files on this platform");
	}
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

bool motor_move_hook(uint32_t src_addr, uint32_t dst_addr)
{
	//Hook slow moving printer function and skip it for faster boot
	Log::info("[Printer] skipping motor move...");
	return true;
}

bool print_hook(uint32_t src_addr, uint32_t dst_addr)
{
	//Hook the BIOS print function
	Log::debug("[Printer] function %04X called from %08X", dst_addr, src_addr);
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

	Log::info("[Printer] size=%dx%d, pixel_format=%d, pixel_double=%d", width, height, pixel_double, pixel_format, pixel_double);

	if ((pixel_double == 0 || pixel_double == 1) && (pixel_format == 1 || pixel_format == 3))
	{
		int print_image_type = imagew::get_default_image_type();
		fs::path print_path = fs::absolute(output_dir) / imagew::make_unique_name("print_");
		print_path += imagew::image_extension(print_image_type);

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
				print_success = imagew::save_image_8bpp(print_image_type, print_path, width*2, height*2, &data_doubled[0], 256, palette);
			}
			else
			{
				print_success = imagew::save_image_8bpp(print_image_type, print_path, width, height, &data[0], 256, palette);
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
				print_success = imagew::save_image_16bpp(print_image_type, print_path, width*2, height*2, &data_doubled[0]);
			}
			else
			{
				print_success = imagew::save_image_16bpp(print_image_type, print_path, width, height, &data[0]);
			}
		}

		if (print_success)
		{
			Log::info("[Printer] saved print to %s", print_path.string().c_str());
			show_print_file(print_path);
			//show_print_messagebox(print_path.string());
		}
		else
		{
			Log::warn("[Printer] failed to open %s", print_path.string().c_str());
		}
	}
	else
	{
		Log::warn("[Printer] unknown mode, aborting");
		print_success = false;
	}

	//Return from print function with success or paper-out status
	sh2.gpr[0] = print_success ? PRINT_STATUS_SUCCESS : PRINT_STATUS_GENERAL_FAILURE;
	return true;
}

void initialize(Config::SystemInfo& config)
{
	output_dir = config.emulator.image_save_directory;
	
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