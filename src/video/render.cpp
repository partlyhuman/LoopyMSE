#include "video/render.h"

#include <common/bswp.h>

#include <algorithm>
#include <cassert>
#include <cstring>

#include "video/vdp_local.h"

namespace Video::Renderer
{

struct TilemapInfo
{
	int width;
	int height;
	uint32_t bg1_start;
	uint32_t data_start;
};

static uint16_t read_palette(uint8_t value)
{
	uint16_t color;
	memcpy(&color, vdp.palette + (value * 2), 2);
	return Common::bswp16(color);
}

static uint16_t read_screen(int index, int x)
{
	uint8_t pal_color = vdp.screens[index][x];
	if (!pal_color || (index == 1 && vdp.color_prio.screen_b_backdrop_only))
	{
		return vdp.backdrops[index];
	}

	return read_palette(pal_color);
}

static void write_screen(int index, int x, uint8_t value)
{
	x &= 0x1FF;
	if (x < DISPLAY_WIDTH)
	{
		vdp.screens[index][x] = value;
	}
}

static inline void write_color_raw(std::unique_ptr<uint16_t[]>& buffer, int x, int y, uint16_t value)
{
	x &= 0x1FF;
	if (x < DISPLAY_WIDTH)
	{
		buffer[x + (y * DISPLAY_WIDTH)] = value;
	}
}

static inline void write_color(std::unique_ptr<uint16_t[]>& buffer, int x, int y, uint16_t value)
{
	write_color_raw(buffer, x, y, value | 0x8000);
}

static void write_pal_color(std::unique_ptr<uint16_t[]>& buffer, int x, int y, uint8_t pal_index)
{
	uint16_t color = read_palette(pal_index);
	write_color(buffer, x, y, color);
}

static int get_bg_tile_size(int index)
{
	int tile_size = (index == 0) ? vdp.bg_ctrl.tile_size0 : vdp.bg_ctrl.tile_size1;
	switch (tile_size)
	{
	case 0x00:
		tile_size = 8;
		break;
	case 0x01:
		tile_size = 16;
		break;
	case 0x02:
		tile_size = 32;
		break;
	case 0x03:
		tile_size = 64;
		break;
	default:
		assert(0);
	}
	return tile_size;
}

static void get_tilemap_info(TilemapInfo& info)
{
	switch (vdp.bg_ctrl.map_size)
	{
	case 0x00:
		info.width = 64;
		info.height = 64;
		break;
	case 0x01:
		info.width = 64;
		info.height = 32;
		break;
	case 0x02:
		info.width = 32;
		info.height = 64;
		break;
	case 0x03:
		info.width = 32;
		info.height = 32;
		break;
	default:
		assert(0);
	}

	info.data_start = (info.width * info.height) << 1;
	if (vdp.bg_ctrl.shared_maps)
	{
		info.bg1_start = 0;
	}
	else
	{
		info.bg1_start = info.data_start;
		info.data_start <<= 1;
	}
}

static void draw_bg(int index, int screen_y)
{
	if (!vdp.layer_ctrl.bg_enable[index])
	{
		return;
	}

	bool is_8bit = index == 0 && vdp.bg_ctrl.bg0_8bit;
	int tile_size = get_bg_tile_size(index);
	int tile_size_mask = tile_size - 1;

	TilemapInfo tilemap;
	get_tilemap_info(tilemap);

	uint32_t map_start = (index == 1) ? tilemap.bg1_start : 0;

	for (int screen_x = 0; screen_x < DISPLAY_WIDTH; screen_x++)
	{
		int x = (screen_x + vdp.bg_scrollx[index]) & ((tilemap.width * tile_size) - 1);
		int y = (screen_y + vdp.bg_scrolly[index]) & ((tilemap.height * tile_size) - 1);

		uint16_t map_offs = (x / tile_size) + ((y / tile_size) * tilemap.width);

		uint16_t descriptor;
		memcpy(&descriptor, &vdp.tile[map_start + (map_offs << 1)], 2);
		descriptor = Common::bswp16(descriptor);

		uint16_t tile_index = descriptor & 0x7FF;
		int screen_index = (descriptor >> 11) & 0x1;
		int pal_descriptor = (descriptor >> 12) & 0x3;
		bool x_flip = (descriptor >> 14) & 0x1;
		bool y_flip = descriptor >> 15;

		int tile_x = x & tile_size_mask;
		if (x_flip)
		{
			tile_x = tile_size_mask - tile_x;
		}

		int tile_y = y & tile_size_mask;
		if (y_flip)
		{
			tile_y = tile_size_mask - tile_y;
		}

		tile_index += tile_y & ~0x7;
		tile_index += tile_x >> 3;
		uint32_t offs = (tile_x & 0x7) + ((tile_y & 0x7) * 0x08) + (tile_index << 6);

		uint8_t tile_data;
		if (is_8bit)
		{
			tile_data = vdp.tile[(tilemap.data_start + offs) & 0xFFFF];
		}
		else
		{
			offs >>= 1;
			offs += vdp.tilebase << 9;
			tile_data = vdp.tile[(tilemap.data_start + offs) & 0xFFFF];
			if (tile_x & 0x1)
			{
				tile_data &= 0xF;
			}
			else
			{
				tile_data >>= 4;
			}
		}

		//0 is transparent, no matter if it's 4-bit or 8-bit
		if (!tile_data)
		{
			write_color_raw(vdp.bg_output[index], screen_x, screen_y, 0);
			continue;
		}

		uint8_t output = tile_data;
		if (!is_8bit)
		{
			uint16_t palsel = vdp.bg_palsel[index];
			int pal = (palsel >> (pal_descriptor * 4)) & 0xF;
			output |= pal << 4;
		}

		write_pal_color(vdp.bg_output[index], screen_x, screen_y, output);
		write_screen(screen_index, screen_x, output);
	}
}

static void draw_bitmap(int index, int y)
{
	if (!vdp.layer_ctrl.bitmap_enable[index])
	{
		return;
	}

	VDP::BitmapRegs* regs = &vdp.bitmap_regs[index];

	//Skip drawing if the bitmap is off-screen verticaly
	if (((y - regs->screeny) & 0x1FF) > regs->h)
	{
		return;
	}

	int screenx = regs->screenx;
	if (screenx & 0x100)
	{
		screenx -= 0x200;
	}
	int visible_left = std::max(0, screenx + regs->clipx);
	int visible_right = std::min(255, screenx + regs->w);

	//Skip drawing if the bitmap is off-screen horizontally
	if (visible_left > 255 || visible_right < 0)
	{
		return;
	}

	bool is_8bit = false;
	bool split_x = false, split_y = false;
	int vram_width = 0, vram_height = 0;
	switch (vdp.bitmap_ctrl)
	{
	case 0x00:
		is_8bit = true;
		split_y = true;
		vram_width = 256;
		vram_height = 256;
		break;
	case 0x01:
		is_8bit = true;
		vram_width = 256;
		vram_height = 512;
		break;
	case 0x02:
		split_y = true;
		vram_width = 512;
		vram_height = 256;
		break;
	case 0x03:
		split_x = true;
		vram_width = 256;
		vram_height = 512;
		break;
	case 0x04:
		vram_width = 512;
		vram_height = 512;
		break;
	default:
		assert(0);
	}
	uint8_t subpalette_bits = ((vdp.bitmap_palsel >> ((3 - index) * 4)) & 0xF) << 4;
	bool use_color_buffer = (regs->buffer_ctrl & 0x100) != 0;

	int width_mask = vram_width - 1;
	int height_mask = vram_height - 1;

	int data_y = (y + regs->scrolly - regs->screeny) & height_mask;
	//If split_y is true, there are two separate maps at y=0 and y=256 that get scrolled independently
	if (split_y)
	{
		data_y |= regs->scrolly & 0x100;
	}

	//Fetch the appropriate line independent of screenx and process color buffering
	//TODO: this fetching should take place on the previous scanline (should subpalette mapping happen earlier too?)
	uint8_t bm_cache_line[256];
	int bm_cache_end = std::min(255, regs->w + 1); //HW bug: one extra pixel is processed unless full line
	for (int x = 0; x <= bm_cache_end; x++)
	{
		int data_x = (x + regs->scrollx) & width_mask;
		if (split_x)
		{
			data_x |= regs->scrollx & 0x100;
		}

		uint32_t addr;
		uint8_t data;
		if (is_8bit)
		{
			addr = data_x + (data_y * 256);
			data = vdp.bitmap[addr & 0x1FFFF];
		}
		else
		{
			addr = (data_x >> 1) + (data_y * 256);
			data = vdp.bitmap[addr & 0x1FFFF];
			if (data_x & 0x1)
			{
				data &= 0xF;
			}
			else
			{
				data >>= 4;
			}

			if (data > 0)
			{
				if (data == 0xF && use_color_buffer)
				{
					data = 0xFF;
				}
				else
				{
					data |= subpalette_bits;
				}
			}
		}

		if (use_color_buffer)
		{
			uint8_t threshold_mask = is_8bit ? 0xFF : 0x0F;
			if (data == 0xFF)
			{
				//HW bug: 0xFF fails to get replaced if x=0xFF
				if (x != 0xFF)
				{
					data = regs->buffered_color;
				}
			}
			else if ((data & threshold_mask) < (regs->buffer_ctrl & threshold_mask))
			{
				regs->buffered_color = data;
			}
		}

		//Now that the buffer control logic has been processed, store the pixel to the cache line
		bm_cache_line[x] = data;
	}

	//Now draw the appropriate part of the cache line to the screen according to screenx
	//For 4bit, subpalette lookups happen in this phase
	for(int x = visible_left; x <= visible_right; x++)
	{
		uint8_t data = bm_cache_line[(x - screenx) & 0xFF];
		if (data == 0)
		{
			continue;
		}

		uint8_t output = data;

		int pair_index = index >> 1;
		int output_mode = vdp.layer_ctrl.bitmap_screen_mode[pair_index];

		write_pal_color(vdp.bitmap_output[index], x, y, output);

		if (output_mode & 0x1)
		{
			write_screen(1, x, output);
		}

		if (output_mode & 0x2)
		{
			write_screen(0, x, output);
		}
	}
}

static void draw_obj(int index, int screen_y)
{
	if (!vdp.layer_ctrl.obj_enable[index])
	{
		return;
	}

	//TODO: limit the maximum number of sprites per scanline

	//Tilemap info is only useful here to get the start of tile data
	TilemapInfo tilemap;
	get_tilemap_info(tilemap);

	//OBJ #0 has highest priority, so the loop must be backwards
	for (int id = OBJ_COUNT - 1; id >= 0; id--)
	{
		int test_id = (id - vdp.obj_ctrl.id_offs) & 0xFF;
		if (index == 0 && test_id >= OBJ_COUNT)
		{
			continue;
		}

		if (index == 1 && test_id < OBJ_COUNT)
		{
			continue;
		}

		uint32_t descriptor;
		memcpy(&descriptor, vdp.oam + (id * 4), 4);
		descriptor = Common::bswp32(descriptor);

		int tile_size = (descriptor >> 10) & 0x3;

		int obj_width = 0, obj_height = 0;
		switch (tile_size)
		{
		case 0x00:
			obj_width = 8;
			obj_height = 8;
			break;
		case 0x01:
			obj_width = 16;
			obj_height = 16;
			break;
		case 0x02:
			obj_width = 16;
			obj_height = 32;
			break;
		case 0x03:
			obj_width = 32;
			obj_height = 32;
			break;
		default:
			assert(0);
		}

		int start_y = (descriptor >> 16) & 0xFF;
		bool high_y = (descriptor >> 9) & 0x1;

		start_y |= high_y << 8;

		int end_y = (start_y + obj_height) & 0x1FF;

		if (end_y > start_y)
		{
			if (screen_y < start_y || screen_y >= end_y)
			{
				continue;
			}
		}
		else
		{
			if (screen_y < start_y && screen_y >= end_y)
			{
				continue;
			}
		}

		int start_x = descriptor & 0x1FF;

		for (int screen_x = start_x; screen_x < start_x + obj_width; screen_x++)
		{
			if ((screen_x & 0x1FF) >= DISPLAY_WIDTH)
			{
				continue;
			}

			bool x_flip = (descriptor >> 14) & 0x1;
			bool y_flip = (descriptor >> 15) & 0x1;

			int tile_x = (screen_x - start_x) & (obj_width - 1);
			if (x_flip)
			{
				tile_x = obj_width - 1 - tile_x;
			}

			int tile_y = (screen_y - start_y) & (obj_height - 1);
			if (y_flip)
			{
				tile_y = obj_height - 1 - tile_y;
			}

			int tile_index = descriptor >> 24;
			tile_index += tile_y & ~0x7;
			tile_index += tile_x >> 3;
			tile_index += vdp.obj_ctrl.tile_index_offs[index] << 8;
			uint32_t offs = (tile_x & 0x7) + ((tile_y & 0x7) * 0x08) + (tile_index << 6);

			uint8_t tile_data;
			if (vdp.obj_ctrl.is_8bit)
			{
				tile_data = vdp.tile[(tilemap.data_start + offs) & 0xFFFF];
			}
			else
			{
				offs >>= 1;
				offs += vdp.tilebase << 9;
				tile_data = vdp.tile[(tilemap.data_start + offs) & 0xFFFF];
				if (tile_x & 0x1)
				{
					tile_data &= 0xF;
				}
				else
				{
					tile_data >>= 4;
				}
			}

			if (!tile_data)
			{
				continue;
			}

			uint8_t output = tile_data;
			if (!vdp.obj_ctrl.is_8bit)
			{
				uint16_t palsel = vdp.obj_palsel[index];
				int pal_descriptor = (descriptor >> 12) & 0x3;
				int pal = (palsel >> (pal_descriptor * 4)) & 0xF;
				output |= pal << 4;
			}

			write_pal_color(vdp.obj_output[index], screen_x, screen_y, output);
			int output_mode = vdp.layer_ctrl.obj_screen_mode[index];
			if (output_mode & 0x1)
			{
				write_screen(1, screen_x, output);
			}

			if (output_mode & 0x2)
			{
				write_screen(0, screen_x, output);
			}
		}
	}
}

static void draw_layers(int y)
{
	//Draw each layer
	//The order is important - each layer has a different priority, and lower priority layers are drawn first here
	int bitmap_prio = vdp.color_prio.prio_mode & 0x1;
	int bg0_prio = (vdp.color_prio.prio_mode >> 1) & 0x1;
	int obj0_prio = vdp.color_prio.prio_mode >> 2;

	int bitmap_low = (bitmap_prio == 1) ? 0 : 2;
	int bitmap_hi = (bitmap_low + 2) & 0x3;

	if (obj0_prio == 3)
	{
		draw_obj(0, y);
	}

	draw_bg(1, y);

	if (!bg0_prio)
	{
		draw_bg(0, y);
	}

	if (obj0_prio == 2)
	{
		draw_obj(0, y);
	}

	draw_bitmap(bitmap_low + 1, y);
	draw_bitmap(bitmap_low, y);

	if (obj0_prio == 1)
	{
		draw_obj(0, y);
	}

	draw_bitmap(bitmap_hi + 1, y);
	draw_bitmap(bitmap_hi, y);

	if (bg0_prio)
	{
		draw_bg(0, y);
	}

	draw_obj(1, y);

	if (obj0_prio == 0)
	{
		draw_obj(0, y);
	}
}

static void draw_color_math(int y, bool half)
{
	for (int x = 0; x < DISPLAY_WIDTH; x++)
	{
		uint16_t input_a = 0, input_b = 0;
		if (vdp.color_prio.output_screen_a)
		{
			input_a = read_screen(0, x);
		}

		if (vdp.color_prio.output_screen_b)
		{
			input_b = read_screen(1, x);
		}

		int a_r = (input_a >> 10) & 0x1F;
		int a_g = (input_a >> 5) & 0x1F;
		int a_b = input_a & 0x1F;

		int b_r = (input_b >> 10) & 0x1F;
		int b_g = (input_b >> 5) & 0x1F;
		int b_b = input_b & 0x1F;

		int out_r, out_g, out_b;

		if (vdp.color_prio.blend_mode)
		{
			//Subtractive blending
			out_r = a_r - b_r;
			out_g = a_g - b_g;
			out_b = a_b - b_b;
		}
		else
		{
			//Additive blending
			out_r = a_r + b_r;
			out_g = a_g + b_g;
			out_b = a_b + b_b;
		}

		if (half)
		{
			out_r >>= 1;
			out_g >>= 1;
			out_b >>= 1;
		}

		out_r = std::clamp(out_r, 0, 0x1F);
		out_g = std::clamp(out_g, 0, 0x1F);
		out_b = std::clamp(out_b, 0, 0x1F);

		uint16_t output = (out_r << 10) | (out_g << 5) | out_b;
		write_color(vdp.display_output, x, y, output);
	}
}

static void draw_screen_overlay(int y, bool screen_b_prio)
{
	for (int x = 0; x < DISPLAY_WIDTH; x++)
	{
		uint16_t input_a = 0, input_b = 0;
		if (vdp.color_prio.output_screen_a)
		{
			input_a = read_screen(0, x);
		}

		if (vdp.color_prio.output_screen_b)
		{
			input_b = read_screen(1, x);
		}

		uint16_t output = 0;
		if (screen_b_prio)
		{
			output = input_a;

			if (vdp.screens[1][x])
			{
				output = input_b;
			}
		}
		else
		{
			output = input_b;

			if (vdp.screens[0][x])
			{
				output = input_a;
			}
		}

		write_color(vdp.display_output, x, y, output);
	}
}

static void display_capture(int y)
{
	uint16_t* capture_buffer_15bpp = (uint16_t*)&vdp.capture_buffer[0];
	switch (vdp.capture_ctrl.format)
	{
	case 0:
		//Capture blended output in 15bpp
		memcpy(vdp.capture_buffer, vdp.display_output.get(), DISPLAY_WIDTH * sizeof(uint16_t));
	case 1:
		//Capture screen A in 15bpp via the palette/backdrop
		for (int x = 0; x < DISPLAY_WIDTH; x++)
		{
			capture_buffer_15bpp[x] = Common::bswp16(read_screen(0, x));
		}
		break;
	case 2:
	case 3:
		//Capture screen A in 8bpp
		memcpy(vdp.capture_buffer, vdp.screens[0], DISPLAY_WIDTH * sizeof(uint8_t));
		break;
	default:
		assert(0);
	}
}

void draw_scanline(int y)
{
	//Set both screens to the backdrop color
	memset(vdp.screens, 0, sizeof(vdp.screens));

	draw_layers(y);

	//Fetch the screen colors
	for (int x = 0; x < DISPLAY_WIDTH; x++)
	{
		uint16_t color = read_screen(0, x);
		write_color(vdp.screen_output[0], x, y, color);

		color = read_screen(1, x);
		write_color(vdp.screen_output[1], x, y, color);
	}

	//Draw the screens to the display output buffer
	switch (vdp.dispmode)
	{
	case 0x00:
		draw_color_math(y, false);
		break;
	case 0x01:
		draw_color_math(y, true);
		break;
	case 0x04:
		draw_screen_overlay(y, true);
		break;
	case 0x05:
		draw_screen_overlay(y, false);
		break;
	default:
		assert(0);
	}

	if (vdp.capture_enable && y == vdp.capture_ctrl.scanline)
	{
		display_capture(y);
		vdp.capture_enable = false;
	}
}

void draw_border_scanline(int y)
{
	//Draw backdrop A to the whole scanline
	//Note: y is relative to visible area!
	uint16_t border_color = vdp.backdrops[0] | 0x8000;
	for (int x = 0; x < DISPLAY_WIDTH; x++)
	{
		write_color_raw(vdp.display_output, x, y, border_color);
	}
}

}  // namespace Video::Renderer