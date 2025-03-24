#include <core/loopy_io.h>
#include <log/log.h>

#include <algorithm>
#include <cassert>
#include <cstdio>

namespace LoopyIO
{

struct State
{
	uint16_t pad_buttons;
	uint16_t latched_sensors;
	uint16_t print_temp;
};

static State state;

void initialize()
{
	state = {};
}

void shutdown()
{
	//nop
}

uint8_t reg_read8(uint32_t addr)
{
	assert(0);
	return 0;
}

uint16_t reg_read16(uint32_t addr)
{
	addr &= 0xFFF;
	switch (addr)
	{
	case 0x000:
		return state.print_temp;
	case 0x010:
		return (state.pad_buttons & 0xF) | (((state.pad_buttons >> 4) & 0xF) << 8);
	case 0x012:
		return state.pad_buttons >> 8;
	case 0x014:
		return 0;
	case 0x030:
		return state.latched_sensors;
	default:
		Log::warn("[IO] unmapped read16 %08X", addr);
		return 0;
	}
}

uint32_t reg_read32(uint32_t addr)
{
	assert(0);
	return 0;
}

void reg_write8(uint32_t addr, uint8_t value)
{
	assert(0);
}

void reg_write16(uint32_t addr, uint16_t value)
{
	addr &= 0xFFF;
	switch (addr)
	{
	case 0x030:
		state.latched_sensors = (state.latched_sensors & ~0x0100) | (value & 0x0100);
	default:
		Log::warn("[IO] unmapped write16 %08X: %04X", addr, value);
	}
}

void reg_write32(uint32_t addr, uint32_t value)
{
	assert(0);
}

void update_pad(int key_info, bool pressed)
{
	if (pressed)
	{
		state.pad_buttons |= key_info;
	}
	else
	{
		state.pad_buttons &= ~key_info;
	}
}

void update_print_temp()
{
	float temp = 22.f;
	int bits_temp = std::clamp((int)(temp * 16), 0, 0x3FF);
	state.print_temp = bits_temp << 6;
}

void update_sensors()
{
	//Stock mainboard is always configured for NTSC
	constexpr int region_jumper = 1;

	//BIOS hooks allow simulated printing of XS-11 type seals
	constexpr bool seal_cartridge_present = true;
	constexpr int seal_cartridge_type = 1;

	//Set sensors for appropriate idle state
	int print_mech_sensors = seal_cartridge_present ? 0b100 : 0b011;

	state.latched_sensors = (state.latched_sensors & 0x0100) | ((seal_cartridge_type & 7) << 4) | ((print_mech_sensors & 7) << 1) | (region_jumper & 1);
}

}  // namespace LoopyIO