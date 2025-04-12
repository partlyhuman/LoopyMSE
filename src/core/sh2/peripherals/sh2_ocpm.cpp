#include "core/sh2/peripherals/sh2_ocpm.h"

#include <common/wordops.h>
#include <common/bswp.h>
#include <log/log.h>

#include <cstdio>
#include <cstring>

#include "core/sh2/peripherals/sh2_dmac.h"
#include "core/sh2/peripherals/sh2_intc.h"
#include "core/sh2/peripherals/sh2_serial.h"
#include "core/sh2/peripherals/sh2_timers.h"

namespace SH2::OCPM
{

constexpr static int SERIAL_START = 0xEC0;
constexpr static int SERIAL_END = 0xED0;

constexpr static int TIMER_START = 0xF00;
constexpr static int TIMER_END = 0xF40;

constexpr static int DMAC_START = 0xF40;
constexpr static int DMAC_END = 0xF80;

constexpr static int INTC_START = 0xF84;
constexpr static int INTC_END = 0xF90;

static uint8_t oram[0x400];

uint16_t read_gpio_port(int port);

uint8_t io_read8(uint32_t addr)
{
	addr = (addr & 0x1FF) + 0xE00;

	if (addr >= SERIAL_START && addr < SERIAL_END)
	{
		return Serial::read8(addr);
	}

	if (addr >= TIMER_START && addr < TIMER_END)
	{
		return Timer::read8(addr);
	}

	if (addr >= INTC_START && addr < INTC_END)
	{
		return INTC::read8(addr);
	}

	READ_HALFWORD(io, addr);
}

uint16_t io_read16(uint32_t addr)
{
	addr = (addr & 0x1FE) + 0xE00;

	if (addr >= TIMER_START && addr < TIMER_END)
	{
		return Timer::read16(addr);
	}

	if (addr >= DMAC_START && addr < DMAC_END)
	{
		return DMAC::read16(addr);
	}

	if (addr >= INTC_START && addr < INTC_END)
	{
		return INTC::read16(addr);
	}

	switch (addr)
	{
	case 0xFC0:
	case 0xFC2:
		return read_gpio_port((addr>>1) & 1);
	default:
		Log::warn("[OCPM] unmapped read %08X", addr);
		return 0;
	}
}

uint32_t io_read32(uint32_t addr)
{
	READ_DOUBLEWORD(io, addr);
}

void io_write8(uint32_t addr, uint8_t value)
{
	addr = (addr & 0x1FF) + 0xE00;

	if (addr >= SERIAL_START && addr < SERIAL_END)
	{
		Serial::write8(addr, value);
		return;
	}

	if (addr >= TIMER_START && addr < TIMER_END)
	{
		Timer::write8(addr, value);
		return;
	}

	if (addr >= INTC_START && addr < INTC_END)
	{
		INTC::write8(addr, value);
		return;
	}

	WRITE_HALFWORD(io, addr, value);
}

void io_write16(uint32_t addr, uint16_t value)
{
	addr = (addr & 0x1FE) + 0xE00;
	if (addr >= TIMER_START && addr < TIMER_END)
	{
		Timer::write16(addr, value);
		return;
	}

	if (addr >= DMAC_START && addr < DMAC_END)
	{
		DMAC::write16(addr, value);
		return;
	}

	if (addr >= INTC_START && addr < INTC_END)
	{
		INTC::write16(addr, value);
		return;
	}

	switch (addr)
	{
	case 0xFB8:
		return;
	default:
		Log::warn("[OCPM] unmapped write %08X: %04X", addr, value);
	}
}

void io_write32(uint32_t addr, uint32_t value)
{
	addr = (addr & 0x1FF) + 0xE00;
	if (addr >= DMAC_START && addr < DMAC_END)
	{
		DMAC::write32(addr, value);
		return;
	}

	WRITE_DOUBLEWORD(io, addr, value);
}

uint8_t oram_read8(uint32_t addr)
{
	return oram[addr & 0x3FF];
}

uint16_t oram_read16(uint32_t addr)
{
	uint16_t value;
	memcpy(&value, &oram[addr & 0x3FF], 2);
	return Common::bswp16(value);
}

uint32_t oram_read32(uint32_t addr)
{
	uint32_t value;
	memcpy(&value, &oram[addr & 0x3FF], 4);
	return Common::bswp32(value);
}

void oram_write8(uint32_t addr, uint8_t value)
{
	oram[addr & 0x3FF] = value;
}

void oram_write16(uint32_t addr, uint16_t value)
{
	value = Common::bswp16(value);
	memcpy(&oram[addr & 0x3FF], &value, 2);
}

void oram_write32(uint32_t addr, uint32_t value)
{
	value = Common::bswp32(value);
	memcpy(&oram[addr & 0x3FF], &value, 4);
}

uint16_t read_gpio_port(int port)
{
	if (port == 0)
	{
		// Port A
		constexpr int pa8_cart_present = 1;  //We don't run without a cartridge so this is always high
		constexpr int pa11_unk = 0;  //Tied hard low on all known boards, BIOS copies to an unknown VDP option
		return (pa11_unk << 11) | (pa8_cart_present << 8);
	}
	else if (port == 1)
	{
		// Port B
		constexpr int pb1_unk = 1;  //Pulled high in most cartridges
		constexpr int pb3_unk = 1;  //Pulled high in most cartridges
		return (pb3_unk << 3) | (pb1_unk << 1);
	}
	return 0;
}

}  // namespace SH2::OCPM