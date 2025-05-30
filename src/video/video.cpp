#include <common/bswp.h>
#include <common/imgwriter.h>
#include <common/wordops.h>
#include <core/loopy_io.h>
#include <core/memory.h>
#include <core/sh2/peripherals/sh2_intc.h>
#include <core/timing.h>
#include <log/log.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>

#include "video/render.h"
#include "video/vdp_local.h"

namespace imagew = Common::ImageWriter;

namespace Video
{

static Timing::FuncHandle vcount_func, hsync_func;
static Timing::EventHandle vcount_ev, hsync_ev;

VDP vdp;

constexpr static int LINES_PER_FRAME = 263;

struct DumpHeader
{
	uint32_t addr;
	uint32_t length;
	uint32_t data_width;
};

void dump_all_bmps(int image_type, fs::path base_path)
{
	fs::path image_ext = imagew::image_extension(image_type);

	for (int i = 0; i < 4; i++)
	{
		fs::path bitmap_name = "output_bitmap";
		bitmap_name += std::to_string(i);
		bitmap_name += image_ext;
		imagew::save_image_16bpp(
			image_type, base_path / bitmap_name, DISPLAY_WIDTH, DISPLAY_HEIGHT, vdp.bitmap_output[i].get(), true
		);
	}

	for (int i = 0; i < 2; i++)
	{
		fs::path bg_name = "output_bg";
		bg_name += std::to_string(i);
		bg_name += image_ext;
		imagew::save_image_16bpp(
			image_type, base_path / bg_name, DISPLAY_WIDTH, DISPLAY_HEIGHT, vdp.bg_output[i].get(), true
		);

		fs::path screen_name = "output_screen_";
		screen_name += (i == 1) ? 'B' : 'A';
		screen_name += image_ext;
		imagew::save_image_16bpp(
			image_type, base_path / screen_name, DISPLAY_WIDTH, DISPLAY_HEIGHT, vdp.screen_output[i].get(), true
		);

		fs::path obj_name = "output_obj";
		obj_name += std::to_string(i);
		obj_name += image_ext;
		imagew::save_image_16bpp(
			image_type, base_path / obj_name, DISPLAY_WIDTH, DISPLAY_HEIGHT, vdp.obj_output[i].get(), true
		);
	}

	fs::path display_name = "output_display";
	display_name += image_ext;
	imagew::save_image_16bpp(
		image_type, base_path / display_name, DISPLAY_WIDTH, DISPLAY_HEIGHT, vdp.display_output.get(), false
	);
}

static void start_hsync(uint64_t param, int cycles_late)
{
	vdp.hcount |= 0x100;
	//IRQ0 is triggered every line and uses hcmp/vcmp
	//For now hcmp is not emulated but we just assume it happens at the same time as HSYNC
	if (vdp.cmp_irq_ctrl.irq0_enable && vdp.cmp_irq_ctrl.irq0_enable2)
	{
		if (!vdp.cmp_irq_ctrl.use_vcmp || vdp.vcount == vdp.irq0_vcmp)
		{
			auto irq_id = SH2::OCPM::INTC::IRQ::IRQ0;
			SH2::OCPM::INTC::assert_irq(irq_id, 0);
			SH2::OCPM::INTC::deassert_irq(irq_id);
		}
	}
	//IRQ1 is triggered on visible lines when in HSYNC mode
	if (vdp.sync_irq_ctrl.irq1_enable && vdp.sync_irq_ctrl.irq1_source == 1)
	{
		if (vdp.vcount < vdp.visible_scanlines)
		{
			auto irq_id = SH2::OCPM::INTC::IRQ::IRQ1;
			SH2::OCPM::INTC::assert_irq(irq_id, 0);
			SH2::OCPM::INTC::deassert_irq(irq_id);
		}
	}
}

static void vsync_start()
{
	Log::debug("[Video] VSYNC start");

	//This is kinda weird, but when the VDP enters VSYNC, the total number of scanlines is subtracted from VCOUNT
	//Think of the VSYNC lines as being negative
	vdp.vcount = (vdp.vcount - LINES_PER_FRAME) & 0x1FF;
	vdp.frame_ended = true;

	//NMI is triggered on VSYNC
	if (vdp.cmp_irq_ctrl.nmi_enable)
	{
		//TODO: is there a cleaner way to do this?
		auto irq_id = SH2::OCPM::INTC::IRQ::NMI;
		SH2::OCPM::INTC::assert_irq(irq_id, 0);
		SH2::OCPM::INTC::deassert_irq(irq_id);
	}

	//IRQ1 is triggered on VSYNC when in VSYNC mode
	if (vdp.sync_irq_ctrl.irq1_enable && (vdp.sync_irq_ctrl.irq1_source == 0))
	{
		auto irq_id = SH2::OCPM::INTC::IRQ::IRQ1;
		SH2::OCPM::INTC::assert_irq(irq_id, 0);
		SH2::OCPM::INTC::deassert_irq(irq_id);
	}

	//dump_bmp("output_display", vdp.display_output);
	//dump_all_bmps();
	//dump_for_serial();
}

static void inc_vcount(uint64_t param, int cycles_late)
{
	//Leave HSYNC
	vdp.hcount &= ~0x100;
	if (vdp.vcount < vdp.visible_scanlines)
	{
		Renderer::draw_scanline(vdp.vcount);
	}

	vdp.vcount++;

	//Once we go past the visible region, enter VSYNC
	if (vdp.vcount == vdp.visible_scanlines)
	{
		vsync_start();
	}

	//At the end of VSYNC, wrap around to the start of the visible region
	constexpr static int VSYNC_END = 0x200;

	if (vdp.vcount == VSYNC_END)
	{
		Log::debug("[Video] VSYNC end");
		vdp.vcount = 0;

		//Draw the background color outside the active area
		if (!vdp.mode.extra_scanlines)
		{
			//SDL will be sampling only the active area rect, but during blending, AA will sample from the very next row,
			//so fill it with the background instead of 0x0.
			//The top, left, and right are not subject to this because the source rect touches the edges of the texture so it's not sampled beyond that
			Renderer::draw_border_scanline(0xE0);
		}
	}

	constexpr static int CYCLES_PER_FRAME = Timing::F_CPU / 60;
	constexpr static int CYCLES_PER_LINE = CYCLES_PER_FRAME / LINES_PER_FRAME;
	constexpr static int CYCLES_UNTIL_HSYNC = (CYCLES_PER_LINE * 256.0f) / 341.25f;

	Timing::UnitCycle scanline_cycles = Timing::convert_cpu(CYCLES_PER_LINE - cycles_late);
	vcount_ev = Timing::add_event(vcount_func, scanline_cycles, 0, Timing::CPU_TIMER);

	Timing::UnitCycle hsync_cycles = Timing::convert_cpu(CYCLES_UNTIL_HSYNC - cycles_late);
	hsync_ev = Timing::add_event(hsync_func, hsync_cycles, 0, Timing::CPU_TIMER);
}

static void dump_serial_region(std::ofstream& dump, uint8_t* mem, uint32_t addr, uint32_t length)
{
	DumpHeader header;
	header.addr = Common::bswp32(addr | (1 << 27));	 //Make sure the address is 16-bit for the CPU
	header.length = Common::bswp32(length);
	header.data_width = Common::bswp32(2);

	dump.write((char*)&header, sizeof(header));
	dump.write((char*)mem, length);
}

void initialize()
{
	vdp = {};

	vdp.visible_scanlines = 0xE0;

	//Initialize output buffers
	for (int i = 0; i < 2; i++)
	{
		vdp.bg_output[i] = std::make_unique<uint16_t[]>(DISPLAY_WIDTH * DISPLAY_HEIGHT);
		vdp.obj_output[i] = std::make_unique<uint16_t[]>(DISPLAY_WIDTH * DISPLAY_HEIGHT);
		vdp.screen_output[i] = std::make_unique<uint16_t[]>(DISPLAY_WIDTH * DISPLAY_HEIGHT);
	}

	//Set all OBJs to invisible
	for (int i = 0; i < OAM_SIZE; i += 4)
	{
		oam_write32(i, 0x200);
	}

	for (int i = 0; i < 4; i++)
	{
		vdp.bitmap_output[i] = std::make_unique<uint16_t[]>(DISPLAY_WIDTH * DISPLAY_HEIGHT);
	}

	vdp.display_output = std::make_unique<uint16_t[]>(DISPLAY_WIDTH * DISPLAY_HEIGHT);

	//Map VRAM to the CPU
	//Bitmap VRAM is mirrored
	Memory::map_sh2_pagetable(vdp.bitmap, BITMAP_VRAM_START, BITMAP_VRAM_SIZE);
	Memory::map_sh2_pagetable(vdp.bitmap, BITMAP_VRAM_START + BITMAP_VRAM_SIZE, BITMAP_VRAM_SIZE);
	Memory::map_sh2_pagetable(vdp.tile, TILE_VRAM_START, TILE_VRAM_SIZE);

	vcount_func = Timing::register_func("Video::inc_vcount", inc_vcount);
	hsync_func = Timing::register_func("Video::start_hsync", start_hsync);

	//Kickstart the VCOUNT event
	inc_vcount(0, 0);
}

void shutdown()
{
	// nop
}

void start_frame()
{
	vdp.frame_ended = false;

	constexpr static int BUFFER_SIZE = DISPLAY_WIDTH * DISPLAY_HEIGHT;

	//Clear the output buffers
	for (int i = 0; i < 2; i++)
	{
		memset(vdp.bg_output[i].get(), 0, BUFFER_SIZE);
		memset(vdp.obj_output[i].get(), 0, BUFFER_SIZE);
		memset(vdp.bitmap_output[i].get(), 0, BUFFER_SIZE);
		memset(vdp.bitmap_output[i + 2].get(), 0, BUFFER_SIZE);
		memset(vdp.screen_output[i].get(), 0, BUFFER_SIZE);
	}

	memset(vdp.display_output.get(), 0, BUFFER_SIZE);
}

bool check_frame_end()
{
	return vdp.frame_ended;
}

uint16_t get_background_color()
{
	return vdp.backdrops[0];
}

int get_display_scanlines()
{
	return vdp.visible_scanlines;
}

uint16_t* get_display_output()
{
	return vdp.display_output.get();
}

void dump_current_frame(int image_type, fs::path bmp_path)
{
	// Do we need to wait for vsync? This happens on input processing loop
	bool status =
		imagew::save_image_16bpp(image_type, bmp_path, DISPLAY_WIDTH, vdp.visible_scanlines, vdp.display_output.get());
}

void dump_for_serial()
{
	std::ofstream dump("emudump.bin", std::ios::binary);
	const char* MAGIC = "LPSTATE\0";

	dump.write(MAGIC, 8);

	dump_serial_region(dump, vdp.bitmap, BITMAP_VRAM_START, BITMAP_VRAM_SIZE);
	dump_serial_region(dump, vdp.tile, TILE_VRAM_START, TILE_VRAM_SIZE);
	dump_serial_region(dump, vdp.palette, PALETTE_START, PALETTE_SIZE);
	dump_serial_region(dump, vdp.oam, OAM_START, OAM_SIZE);

	//TODO: dump MMIO
}

uint8_t palette_read8(uint32_t addr)
{
	return vdp.palette[addr & 0x1FF];
}

uint16_t palette_read16(uint32_t addr)
{
	uint16_t value;
	memcpy(&value, &vdp.palette[addr & 0x1FE], 2);
	return Common::bswp16(value);
}

uint32_t palette_read32(uint32_t addr)
{
	uint32_t value;
	memcpy(&value, &vdp.palette[addr & 0x1FE], 4);
	return Common::bswp32(value);
}

void palette_write8(uint32_t addr, uint8_t value)
{
	vdp.palette[addr & 0x1FF] = value;
}

void palette_write16(uint32_t addr, uint16_t value)
{
	value = Common::bswp16(value);
	memcpy(&vdp.palette[addr & 0x1FE], &value, 2);
}

void palette_write32(uint32_t addr, uint32_t value)
{
	value = Common::bswp32(value);
	memcpy(&vdp.palette[addr & 0x1FE], &value, 4);
}

uint8_t oam_read8(uint32_t addr)
{
	return vdp.oam[addr & 0x1FF];
}

uint16_t oam_read16(uint32_t addr)
{
	uint16_t value;
	memcpy(&value, &vdp.oam[addr & 0x1FE], 2);
	return Common::bswp16(value);
}

uint32_t oam_read32(uint32_t addr)
{
	uint32_t value;
	memcpy(&value, &vdp.oam[addr & 0x1FE], 4);
	return Common::bswp32(value);
}

void oam_write8(uint32_t addr, uint8_t value)
{
	vdp.oam[addr & 0x1FF] = value;
}

void oam_write16(uint32_t addr, uint16_t value)
{
	value = Common::bswp16(value);
	memcpy(&vdp.oam[addr & 0x1FE], &value, 2);
}

void oam_write32(uint32_t addr, uint32_t value)
{
	value = Common::bswp32(value);
	memcpy(&vdp.oam[addr & 0x1FE], &value, 4);
}

uint8_t capture_read8(uint32_t addr)
{
	return vdp.capture_buffer[addr & 0x1FF];
}

uint16_t capture_read16(uint32_t addr)
{
	uint16_t value;
	memcpy(&value, &vdp.capture_buffer[addr & 0x1FE], 2);
	return Common::bswp16(value);
}

uint32_t capture_read32(uint32_t addr)
{
	uint32_t value;
	memcpy(&value, &vdp.capture_buffer[addr & 0x1FE], 4);
	return Common::bswp32(value);
}

void capture_write8(uint32_t addr, uint8_t value)
{
	assert(0);
}

void capture_write16(uint32_t addr, uint16_t value)
{
	assert(0);
}

void capture_write32(uint32_t addr, uint32_t value)
{
	assert(0);
}

uint8_t bitmap_reg_read8(uint32_t addr)
{
	READ_HALFWORD(bitmap_reg, addr);
}

uint16_t bitmap_reg_read16(uint32_t addr)
{
	addr &= 0xFFE;

	int index = (addr >> 1) & 0x3;
	auto layer = &vdp.bitmap_regs[index];
	int reg = addr & ~0x7;

	switch (reg)
	{
	case 0x000:
		return layer->scrollx;
	case 0x008:
		return layer->scrolly;
	case 0x010:
		return layer->screenx;
	case 0x018:
		return layer->screeny;
	case 0x020:
		return layer->w | (layer->clipx << 8);
	case 0x028:
		return layer->h;
	case 0x030:
		return vdp.bitmap_ctrl;
	case 0x040:
		return vdp.bitmap_palsel;
	case 0x050:
		return layer->buffer_ctrl;
	default:
		assert(0);
		return 0;
	}
}

uint32_t bitmap_reg_read32(uint32_t addr)
{
	READ_DOUBLEWORD(bitmap_reg, addr);
}

void bitmap_reg_write8(uint32_t addr, uint8_t value)
{
	WRITE_HALFWORD(bitmap_reg, addr, value);
}

void bitmap_reg_write16(uint32_t addr, uint16_t value)
{
	addr &= 0xFFE;

	int index = (addr >> 1) & 0x3;
	auto layer = &vdp.bitmap_regs[index];
	int reg = addr & ~0x7;

	switch (reg)
	{
	case 0x000:
		Log::debug("[Video] write BM%d_SCROLLX: %04X", index, value);
		layer->scrollx = value & 0x1FF;
		break;
	case 0x008:
		Log::debug("[Video] write BM%d_SCROLLY: %04X", index, value);
		layer->scrolly = value & 0x1FF;
		break;
	case 0x010:
		Log::debug("[Video] write BM%d_SCREENX: %04X", index, value);
		layer->screenx = value & 0x1FF;
		break;
	case 0x018:
		Log::debug("[Video] write BM%d_SCREENY: %04X", index, value);
		layer->screeny = value & 0x1FF;
		break;
	case 0x020:
		Log::debug("[Video] write BM%d_CLIPWIDTH: %04X", index, value);
		layer->w = value & 0xFF;
		layer->clipx = value >> 8;
		break;
	case 0x028:
		Log::debug("[Video] write BM%d_HEIGHT: %04X", index, value);
		layer->h = value & 0xFF;
		break;
	case 0x030:
		Log::debug("[Video] write BM_CTRL: %04X", value);
		vdp.bitmap_ctrl = value;
		break;
	case 0x040:
		Log::debug("[Video] write BM_PALSEL: %04X", value);
		vdp.bitmap_palsel = value;
		break;
	case 0x050:
		Log::debug("[Video] write BM%d_BUFFER_CTRL: %04X", index, value);
		layer->buffer_ctrl = value;
		break;
	default:
		assert(0);
	}
}

void bitmap_reg_write32(uint32_t addr, uint32_t value)
{
	WRITE_DOUBLEWORD(bitmap_reg, addr, value);
}

uint8_t ctrl_read8(uint32_t addr)
{
	READ_HALFWORD(ctrl, addr);
}

uint16_t ctrl_read16(uint32_t addr)
{
	addr &= 0xFFE;
	switch (addr)
	{
	case 0x000:
	{
		uint16_t result = vdp.mode.use_pal;
		result |= vdp.mode.extra_scanlines << 1;
		result |= vdp.mode.unk << 2;
		result |= vdp.mode.mouse_scan << 3;
		result |= vdp.mode.pad_scan << 4;
		result |= vdp.mode.unk2 << 5;
		return result;
	}
	case 0x002:
		//FIXME: This only reflects HSYNC status, it doesn't actually return the horizontal counter
		return vdp.hcount;
	case 0x004:
		return vdp.vcount;
	default:
		assert(0);
		return 0;
	}
}

uint32_t ctrl_read32(uint32_t addr)
{
	READ_DOUBLEWORD(ctrl, addr);
}

void ctrl_write8(uint32_t addr, uint8_t value)
{
	WRITE_HALFWORD(ctrl, addr, value);
}

void ctrl_write16(uint32_t addr, uint16_t value)
{
	addr &= 0xFFE;
	switch (addr)
	{
	case 0x000:
		Log::debug("[Video] write MODE: %04X", value);
		vdp.mode.use_pal = value & 0x1;
		vdp.mode.extra_scanlines = (value >> 1) & 0x1;
		vdp.mode.unk = (value >> 2) & 0x1;
		vdp.mode.mouse_scan = (value >> 3) & 0x1;
		vdp.mode.pad_scan = (value >> 4) & 0x1;
		vdp.mode.unk2 = (value >> 5) & 0x1;
		assert(!vdp.mode.use_pal);

		vdp.visible_scanlines = (vdp.mode.extra_scanlines) ? 0xF0 : 0xE0;
		LoopyIO::set_controller_scan_mode(vdp.mode.pad_scan, vdp.mode.mouse_scan);
		break;
	case 0x006:
		if (value & 0x01)
		{
			vdp.capture_enable = true;
		}

		if (value & 0x02)
		{
			LoopyIO::update_print_temp();
		}

		if (value & 0x04)
		{
			LoopyIO::update_sensors();
		}

		//Only log writes to unimplemented bits
		if ((value & ~0x0007) != 0x01)
		{
			Log::debug("[Video] write ctrl 006: %04X", value);
		}
		break;
	case 0x008:
		Log::debug("[Video] write SYNC_IRQ_CTRL: %04X", value);
		vdp.sync_irq_ctrl.irq1_enable = value & 0x1;
		vdp.sync_irq_ctrl.irq1_source = (value >> 1) & 0x1;
		break;
	default:
		assert(0);
	}
}

void ctrl_write32(uint32_t addr, uint32_t value)
{
	WRITE_DOUBLEWORD(ctrl, addr, value);
}

uint8_t bgobj_read8(uint32_t addr)
{
	READ_HALFWORD(bgobj, addr);
}

uint16_t bgobj_read16(uint32_t addr)
{
	addr &= 0xFFE;
	switch (addr)
	{
	case 0x000:
	{
		uint16_t result = vdp.bg_ctrl.shared_maps;
		result |= vdp.bg_ctrl.map_size << 1;
		result |= vdp.bg_ctrl.bg0_8bit << 3;
		result |= vdp.bg_ctrl.tile_size1 << 4;
		result |= vdp.bg_ctrl.tile_size0 << 6;
		return result;
	}
	case 0x002:
		return vdp.bg_scrollx[0];
	case 0x004:
		return vdp.bg_scrolly[0];
	case 0x006:
		return vdp.bg_scrollx[1];
	case 0x008:
		return vdp.bg_scrolly[1];
	case 0x00A:
		return vdp.bg_palsel[0];
	case 0x00C:
		return vdp.bg_palsel[1];
	case 0x010:
	{
		uint16_t result = vdp.obj_ctrl.id_offs;
		result |= vdp.obj_ctrl.tile_index_offs[1] << 8;
		result |= vdp.obj_ctrl.tile_index_offs[0] << 11;
		result |= vdp.obj_ctrl.is_8bit << 14;
		return result;
	}
	case 0x012:
		return vdp.obj_palsel[0];
	case 0x014:
		return vdp.obj_palsel[1];
	case 0x020:
		return vdp.tilebase;
	default:
		assert(0);
		return 0;
	}
}

uint32_t bgobj_read32(uint32_t addr)
{
	READ_DOUBLEWORD(bgobj, addr);
}

void bgobj_write8(uint32_t addr, uint8_t value)
{
	WRITE_HALFWORD(bgobj, addr, value);
}

void bgobj_write16(uint32_t addr, uint16_t value)
{
	addr &= 0xFFE;
	switch (addr)
	{
	case 0x000:
		Log::debug("[Video] write BG_CTRL: %04X", value);
		vdp.bg_ctrl.shared_maps = value & 0x1;
		vdp.bg_ctrl.map_size = (value >> 1) & 0x3;
		vdp.bg_ctrl.bg0_8bit = (value >> 3) & 0x1;

		//Note the reversed order!
		vdp.bg_ctrl.tile_size1 = (value >> 4) & 0x3;
		vdp.bg_ctrl.tile_size0 = (value >> 6) & 0x3;
		break;
	case 0x002:
	case 0x006:
	{
		int index = (addr - 0x002) >> 2;
		Log::debug("[Video] write BG%d_SCROLLX: %04X", index, value);
		vdp.bg_scrollx[index] = value & 0xFFF;
		break;
	}
	case 0x004:
	case 0x008:
	{
		int index = (addr - 0x004) >> 2;
		Log::debug("[Video] write BG%d_SCROLLY: %04X", index, value);
		vdp.bg_scrolly[index] = value & 0xFFF;
		break;
	}
	case 0x00A:
	case 0x00C:
	{
		int index = (addr - 0x00A) >> 1;
		Log::debug("[Video] write BG%d_PALSEL: %04X", index, value);
		vdp.bg_palsel[index] = value;
		break;
	}
	case 0x010:
		Log::debug("[Video] write OBJ_CTRL: %04X", value);
		vdp.obj_ctrl.id_offs = value & 0xFF;

		//Note the reversed order!
		vdp.obj_ctrl.tile_index_offs[1] = (value >> 8) & 0x7;
		vdp.obj_ctrl.tile_index_offs[0] = (value >> 11) & 0x7;
		vdp.obj_ctrl.is_8bit = (value >> 14) & 0x1;
		break;
	case 0x012:
	case 0x014:
	{
		int index = (addr - 0x012) >> 1;
		Log::debug("[Video] write OBJ%d_PALSEL: %04X", index, value);
		vdp.obj_palsel[index] = value;
		break;
	}
	case 0x020:
		Log::debug("[Video] write TILEBASE: %04X", value);
		vdp.tilebase = value & 0xFF;
		break;
	default:
		assert(0);
	}
}

void bgobj_write32(uint32_t addr, uint32_t value)
{
	WRITE_DOUBLEWORD(bgobj, addr, value);
}

uint8_t display_read8(uint32_t addr)
{
	READ_HALFWORD(display, addr);
}

uint16_t display_read16(uint32_t addr)
{
	addr &= 0xFFE;
	switch (addr)
	{
	case 0x000:
		return vdp.dispmode;
	case 0x002:
	{
		uint16_t result = 0;

		for (int i = 0; i < 2; i++)
		{
			result |= vdp.layer_ctrl.bg_enable[i] << i;
			result |= vdp.layer_ctrl.obj_enable[i] << (i + 6);
		}

		for (int i = 0; i < 4; i++)
		{
			result |= vdp.layer_ctrl.bitmap_enable[i] << (i + 2);
		}

		result |= vdp.layer_ctrl.bitmap_screen_mode[0] << 8;
		result |= vdp.layer_ctrl.bitmap_screen_mode[1] << 10;
		result |= vdp.layer_ctrl.obj_screen_mode[0] << 12;
		result |= vdp.layer_ctrl.obj_screen_mode[1] << 14;
		return result;
	}
	case 0x004:
	{
		uint16_t result = vdp.color_prio.prio_mode;
		result |= vdp.color_prio.screen_b_backdrop_only << 4;
		result |= vdp.color_prio.output_screen_b << 5;
		result |= vdp.color_prio.output_screen_a << 6;
		result |= vdp.color_prio.blend_mode << 7;
		return result;
	}
	case 0x006:
		//Note the reversed order!
		return vdp.backdrops[1];
	case 0x008:
		return vdp.backdrops[0];
	default:
		assert(0);
		return 0;
	}
}

uint32_t display_read32(uint32_t addr)
{
	READ_DOUBLEWORD(display, addr);
}

void display_write8(uint32_t addr, uint8_t value)
{
	WRITE_HALFWORD(display, addr, value);
}

void display_write16(uint32_t addr, uint16_t value)
{
	addr &= 0xFFE;
	switch (addr)
	{
	case 0x000:
		vdp.dispmode = value & 0x7;
		Log::debug("[Video] write DISPMODE: %04X", value);
		break;
	case 0x002:
		for (int i = 0; i < 2; i++)
		{
			vdp.layer_ctrl.bg_enable[i] = (value >> i) & 0x1;
			vdp.layer_ctrl.obj_enable[i] = (value >> (i + 6)) & 0x1;
		}

		for (int i = 0; i < 4; i++)
		{
			vdp.layer_ctrl.bitmap_enable[i] = (value >> (i + 2)) & 0x1;
		}

		vdp.layer_ctrl.bitmap_screen_mode[0] = (value >> 8) & 0x3;
		vdp.layer_ctrl.bitmap_screen_mode[1] = (value >> 10) & 0x3;
		vdp.layer_ctrl.obj_screen_mode[0] = (value >> 12) & 0x3;
		vdp.layer_ctrl.obj_screen_mode[1] = value >> 14;
		Log::debug("[Video] write LAYER_CTRL: %04X", value);
		break;
	case 0x004:
		vdp.color_prio.prio_mode = value & 0xF;
		vdp.color_prio.screen_b_backdrop_only = (value >> 4) & 0x1;
		vdp.color_prio.output_screen_b = (value >> 5) & 0x1;
		vdp.color_prio.output_screen_a = (value >> 6) & 0x1;
		vdp.color_prio.blend_mode = (value >> 7) & 0x1;
		Log::debug("[Video] write COLORPRIO: %04X", value);
		break;
	case 0x006:
		//Note the reversed order!
		vdp.backdrops[1] = value;
		break;
	case 0x008:
		vdp.backdrops[0] = value;
		break;
	case 0x00A:
		vdp.capture_ctrl.scanline = value & 0xFF;
		vdp.capture_ctrl.format = (value >> 8) & 0x3;
		break;
	default:
		assert(0);
	}
}

void display_write32(uint32_t addr, uint32_t value)
{
	WRITE_DOUBLEWORD(display, addr, value);
}

uint8_t irq_read8(uint32_t addr)
{
	READ_HALFWORD(irq, addr);
}

uint16_t irq_read16(uint32_t addr)
{
	addr &= 0xFFE;

	switch (addr)
	{
	case 0x002:
		return vdp.irq0_hcmp;
	case 0x004:
		return vdp.irq0_vcmp;
	default:
		assert(0);
		return 0;
	}
}

uint32_t irq_read32(uint32_t addr)
{
	READ_DOUBLEWORD(irq, addr);
}

void irq_write8(uint32_t addr, uint8_t value)
{
	WRITE_HALFWORD(irq, addr, value);
}

void irq_write16(uint32_t addr, uint16_t value)
{
	addr &= 0xFFE;

	switch (addr)
	{
	case 0x000:
		//TODO: emulate IRQ0, a screen position compare interrupt (no game uses it, but homebrew might)
		vdp.cmp_irq_ctrl.irq0_enable = (value >> 1) & 0x1;
		vdp.cmp_irq_ctrl.nmi_enable = (value >> 2) & 0x1;
		vdp.cmp_irq_ctrl.use_vcmp = (value >> 5) & 0x1;
		vdp.cmp_irq_ctrl.irq0_enable2 = (value >> 7) & 0x1;
		Log::debug("[VDP] write CMP_IRQ_CTRL: %04X", value);
		break;
	case 0x002:
		vdp.irq0_hcmp = value & 0x1FF;
		break;
	case 0x004:
		vdp.irq0_vcmp = value & 0x1FF;
		break;
	}
}

void irq_write32(uint32_t addr, uint32_t value)
{
	WRITE_DOUBLEWORD(irq, addr, value);
}

uint8_t dma_ctrl_read8(uint32_t addr)
{
	READ_HALFWORD(dma_ctrl, addr);
}

uint16_t dma_ctrl_read16(uint32_t addr)
{
	addr &= 0xFFE;
	switch (addr)
	{
	case 0x002:
		return vdp.dma_mask;
	case 0x004:
		return vdp.dma_value;
	default:
		assert(0);
		return 0;
	}
}

uint32_t dma_ctrl_read32(uint32_t addr)
{
	READ_DOUBLEWORD(dma_ctrl, addr);
}

void dma_ctrl_write8(uint32_t addr, uint8_t value)
{
	WRITE_HALFWORD(dma_ctrl, addr, value);
}

void dma_ctrl_write16(uint32_t addr, uint16_t value)
{
	addr &= 0xFFE;
	switch (addr)
	{
	case 0x000:
		Log::debug("[Video] write dma ctrl 000: %04X", value);
		break;
	case 0x002:
		//TODO: what does bit 8 do? Seems to have no effect in HW tests at this time
		vdp.dma_mask = value & 0x1FF;
		break;
	case 0x004:
		vdp.dma_value = value & 0xFF;
		break;
	default:
		assert(0);
	}
}

void dma_ctrl_write32(uint32_t addr, uint32_t value)
{
	WRITE_DOUBLEWORD(dma_ctrl, addr, value);
}

uint8_t dma_read8(uint32_t addr)
{
	assert(0);
	return 0;
}

uint16_t dma_read16(uint32_t addr)
{
	assert(0);
	return 0;
}

uint32_t dma_read32(uint32_t addr)
{
	assert(0);
	return 0;
}

void dma_write8(uint32_t addr, uint8_t value)
{
	WRITE_HALFWORD(dma, addr, value);
}

void dma_write16(uint32_t addr, uint16_t value)
{
	//Value written doesn't matter, it always triggers this
	//TODO: how long does this take? Is the CPU stalled?
	addr &= 0x3FE;

	int y = addr >> 1;
	for (int x = 0; x < DISPLAY_WIDTH; x++)
	{
		uint32_t addr = x + (y * DISPLAY_WIDTH);
		uint8_t data = vdp.bitmap[addr];
		data &= ~vdp.dma_mask;
		data |= vdp.dma_value & vdp.dma_mask;
		vdp.bitmap[addr] = data;
	}
}

void dma_write32(uint32_t addr, uint32_t value)
{
	WRITE_DOUBLEWORD(dma, addr, value);
}

}  // namespace Video