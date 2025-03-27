#include "common/imgwriter.h"

#include <log/log.h>

#include <algorithm>
#include <cstring>
#include <ctime>
#include <fstream>
#include <vector>

namespace Common::ImageWriter
{

int parse_image_type(std::string type, int default_)
{
	std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c){ return std::tolower(c); });
	if (type == "bmp" || type == ".bmp" || type == "bitmap")
	{
		return IMAGE_TYPE_BMP;
	}
	return default_;
}

fs::path image_extension(int image_type)
{
	if (image_type == IMAGE_TYPE_BMP) return fs::path(".bmp");
	return fs::path("");
}

fs::path make_unique_name(std::string prefix, std::string suffix)
{
	static unsigned int unique_number = 1;

	std::time_t timestamp = std::time(nullptr);
	char timestamp_buffer[20];
	strftime(timestamp_buffer, sizeof(timestamp_buffer), "%Y%m%d_%H%M%S", std::localtime(&timestamp));

	return prefix + timestamp_buffer + "_" + std::to_string(unique_number++) + suffix;
}

bool write_bmp(fs::path path, uint32_t width, uint32_t height, uint32_t data[], bool transparent)
{
	std::ofstream bmp_file(path, std::ios::binary);
	if (!bmp_file.is_open()) return false;

	//BMP header
	const char* SIGNATURE = "BM";
	bmp_file.write(SIGNATURE, 2);

	uint32_t head_size = 14;
	uint32_t info_size = transparent ? 108 : 40;
	uint32_t data_size_per_row = transparent ? (width * 4) : (width * 3);
	uint32_t padding_per_row = (4 - (data_size_per_row % 4)) % 4; 
	uint32_t data_size = (data_size_per_row + padding_per_row) * height;

	uint32_t file_size = head_size + info_size + data_size;
	bmp_file.write((char*)&file_size, 4);

	uint32_t reserved = 0;
	bmp_file.write((char*)&reserved, 4);

	uint32_t data_offs = head_size + info_size;
	bmp_file.write((char*)&data_offs, 4);

	//DIB header
	bmp_file.write((char*)&info_size, 4);

	bmp_file.write((char*)&width, 4);
	bmp_file.write((char*)&height, 4);

	uint16_t planes = 1;
	bmp_file.write((char*)&planes, 2);

	uint16_t bpp = transparent ? 32 : 24;
	bmp_file.write((char*)&bpp, 2);

	uint32_t compression = transparent ? 3 : 0; // BI_BITFIELDS or BI_RGB
	bmp_file.write((char*)&compression, 4);

	bmp_file.write((char*)&data_size, 4);

	uint32_t pixels_per_metre = 2835; // 72DPI
	bmp_file.write((char*)&pixels_per_metre, 4);
	bmp_file.write((char*)&pixels_per_metre, 4);

	uint32_t num_colors = 0;
	bmp_file.write((char*)&num_colors, 4);
	bmp_file.write((char*)&num_colors, 4);

	if (transparent)
	{
		uint32_t bitmask_r = 0xFF << 16;
		uint32_t bitmask_g = 0xFF << 8;
		uint32_t bitmask_b = 0xFF;
		uint32_t bitmask_a = 0xFF << 24;
		bmp_file.write((char*)&bitmask_r, 4);
		bmp_file.write((char*)&bitmask_g, 4);
		bmp_file.write((char*)&bitmask_b, 4);
		bmp_file.write((char*)&bitmask_a, 4);

		const char* color_space = "sRGB";
		bmp_file.write((char*)&color_space, 4);
		bmp_file.write((char*)&reserved, 4);
		bmp_file.write((char*)&reserved, 4);
		bmp_file.write((char*)&reserved, 4);
		bmp_file.write((char*)&reserved, 4);
		bmp_file.write((char*)&reserved, 4);
		bmp_file.write((char*)&reserved, 4);
		bmp_file.write((char*)&reserved, 4);
		bmp_file.write((char*)&reserved, 4);
		bmp_file.write((char*)&reserved, 4);
		bmp_file.write((char*)&reserved, 4);
		bmp_file.write((char*)&reserved, 4);
		bmp_file.write((char*)&reserved, 4);
	}

	for (int y = 0; y < height; y++)
	{
		int flipped_y = height - y - 1;
		for (int x = 0; x < width; x++)
		{
			//Write B,G,R,A or B,G,R (assumes system is little-endian!)
			uint32_t pixel_argb = data[flipped_y * width + x];
			bmp_file.write((char*)&pixel_argb, transparent ? 4 : 3);
		}
		if (padding_per_row > 0)
		{
			bmp_file.write((char*)&reserved, padding_per_row);
		}
	}

	bmp_file.close();
	return true;
}

bool write_image(int image_type, fs::path path, uint32_t width, uint32_t height, uint32_t data[], bool transparent)
{
	if (image_type == IMAGE_TYPE_BMP) return write_bmp(path, width, height, data, transparent);
	return false;
}

static inline uint32_t color_16bpp_to_argb(uint16_t c)
{
	uint8_t r = ((c >> 10) & 31) * 255 / 31;
	uint8_t g = ((c >> 5) & 31) * 255 / 31;
	uint8_t b = (c & 31) * 255 / 31;
	uint8_t a = (c >> 15) * 255;
	return (a << 24) | (r << 16) | (g << 8) | b;
}

bool save_image_16bpp(int image_type, fs::path path, uint32_t width, uint32_t height, uint16_t data[], bool transparent)
{
	unsigned int num_pixels = width * height;
	uint32_t data_argb[num_pixels];

	uint16_t alpha_set = transparent ? 0 : 0x8000;

	for (int i = 0; i < num_pixels; i++)
	{
		data_argb[i] = color_16bpp_to_argb(data[i] | alpha_set);
	}

	return write_image(image_type, path, width, height, data_argb, transparent);
}

bool save_image_8bpp(int image_type, fs::path path, uint32_t width, uint32_t height, uint8_t data[], uint32_t num_colors, uint16_t palette[], bool transparent)
{
	unsigned int num_pixels = width * height;
	uint16_t data_16bpp[num_pixels];

	for (int i = 0; i < num_pixels; i++)
	{
		uint8_t pixel = data[i];
		if (pixel >= num_colors) pixel = num_colors - 1;
		data_16bpp[i] = palette[pixel];
	}

	return save_image_16bpp(image_type, path, width, height, data_16bpp, transparent);
}

}
