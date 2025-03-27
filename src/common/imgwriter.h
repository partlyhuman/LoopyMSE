#pragma once

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace Common::ImageWriter
{

constexpr static int IMAGE_TYPE_BMP = 1;
constexpr static int IMAGE_TYPE_DEFAULT = IMAGE_TYPE_BMP;

int parse_image_type(std::string type, int default_);

fs::path image_extension(int image_type);
fs::path make_unique_name(std::string prefix = "", std::string suffix = "");

bool save_image_16bpp(int image_type, fs::path path, uint32_t width, uint32_t height, uint16_t data[], bool transparent = false);
bool save_image_8bpp(int image_type, fs::path path, uint32_t width, uint32_t height, uint8_t data[], uint32_t num_colors, uint16_t palette[], bool transparent = false);

}