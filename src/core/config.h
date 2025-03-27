#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

namespace Config
{

struct CartInfo
{
	std::vector<uint8_t> rom;
	std::vector<uint8_t> sram;
	std::string sram_file_path;
	std::string rom_path;

	bool is_loaded()
	{
		return !rom.empty();
	}
};

struct EmulatorOpts
{
	fs::path image_save_directory;
	int screenshot_image_type;
	int printer_image_type;
	std::string printer_view_command;
};

struct SystemInfo
{
	CartInfo cart;
	EmulatorOpts emulator;
	std::vector<uint8_t> bios_rom;
	std::vector<uint8_t> sound_rom;
};

}  // namespace Config