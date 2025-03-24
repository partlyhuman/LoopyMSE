#include "printer/printer.h"

#include <vector>
#include <filesystem>
#include <thread>

#include <common/imgwriter.h>
#include <core/sh2/sh2_bus.h>
#include <core/sh2/sh2_local.h>
#include <log/log.h>

#include <SDL2/SDL.h>

namespace fs = std::filesystem;
namespace imagew = Common::ImageWriter;

namespace Printer
{

constexpr static uint32_t ADDR_MOTOR_MOVE = 0x00001B76;
constexpr static uint32_t ADDR_PRINT = 0x000006D4;

constexpr static int PRINT_STATUS_SUCCESS = 0;
constexpr static int PRINT_STATUS_PAPER_OUT = 1;
constexpr static int PRINT_STATUS_NO_SEAL_CART = 2;
constexpr static int PRINT_STATUS_CANCEL = 3;
constexpr static int PRINT_STATUS_JAM = 4;
constexpr static int PRINT_STATUS_OVERHEAT = 4;
	
using namespace SH2;

void show_print_messagebox(std::string print_path)
{
	char message[512];
	snprintf(message, sizeof(message), "Your print has been saved to %s", print_path.c_str());
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "LoopyMSE", (const char*)message, NULL);
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
	uint32_t p1 = sh2.gpr[4];
	uint32_t p2 = sh2.gpr[5];
	uint32_t p3 = sh2.gpr[6];
	uint32_t p4 = sh2.gpr[7];
	uint32_t p5 = Bus::read32(sp);
	uint32_t p6 = Bus::read32(sp+4);
	uint32_t p7 = Bus::read32(sp+8);
	uint32_t p8_continue = Bus::read32(sp+12);
	uint32_t p1d_data = Bus::read32(p1);
	uint32_t p2d = Bus::read32(p2);
	uint32_t p3d_dims = Bus::read32(p3);
	uint8_t p6d_format = Bus::read8(p6);
	Log::debug("[Printer] &data=%08X, *p2=%08X, dims=%08X, p4=%08X, p5=%08X, format=%02X, p7=%08X, first=%d", p1d_data, p2d, p3d_dims, p4, p5, p6d_format, p7, p8_continue);
	
	bool print_success;

	// Dump the data to be printed
	uint32_t width = p3d_dims & 0xFFFF;
	uint32_t height = p3d_dims >> 16;

	int format_flags = p6d_format >> 4;
	int pixel_format = p6d_format & 15;
	Log::info("[Printer] size=%dx%d, format=%d:%d", width, height, format_flags, pixel_format);

	if (format_flags == 0 && (pixel_format == 1 || pixel_format == 3))
	{
		int print_image_type = imagew::get_default_image_type();
		fs::path print_path = imagew::make_unique_name("print_");
		print_path += imagew::image_extension(print_image_type);

		if (pixel_format == 3)
		{
			std::vector<uint8_t> data(width * height);
			uint16_t palette[256];

			for (int i = 0; i < (width * height); i++)
			{
				data[i] = Bus::read8(p1d_data + i);
			}
			for (int p = 0; p < 256; p++)
			{
				palette[p] = Bus::read16(p2d + (p * 2));
			}

			print_success = imagew::save_image_8bpp(print_image_type, print_path, width, height, &data[0], 256, palette);
		}
		if (pixel_format == 1)
		{
			std::vector<uint16_t> data(width * height);

			for (int i = 0; i < (width * height); i++)
			{
				data[i] = Bus::read16(p1d_data + (i * 2));
			}

			print_success = imagew::save_image_16bpp(print_image_type, print_path, width, height, &data[0]);
		}

		if (print_success)
		{
			Log::info("[Printer] saved print data to %s", print_path.string().c_str());
			//show_print_messagebox(print_path.string());
		}
		else
		{
			Log::info("[Printer] failed to open %s", print_path.string().c_str());
		}
	}
	else
	{
		Log::info("[Printer] unknown format %d:%d, aborting", format_flags, pixel_format);
		print_success = false;
	}

	//Return from print function with success or paper-out status
	sh2.gpr[0] = print_success ? PRINT_STATUS_SUCCESS : PRINT_STATUS_PAPER_OUT;
	return true;
}

void initialize()
{
	SH2::add_hook(ADDR_MOTOR_MOVE, &motor_move_hook);
	SH2::add_hook(ADDR_PRINT, &print_hook);
	Log::debug("[Printer] registered hooks for print and motor-move BIOS calls");
}

void shutdown()
{
	SH2::remove_hook(ADDR_MOTOR_MOVE);
	SH2::remove_hook(ADDR_PRINT);
	Log::debug("[Printer] unregistered hooks");
}

}