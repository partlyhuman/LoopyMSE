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

struct SystemInfo
{
	CartInfo cart;
	std::vector<uint8_t> bios_rom;
	std::vector<uint8_t> sound_rom;

	fs::path image_save_directory;
};

}  // namespace Config