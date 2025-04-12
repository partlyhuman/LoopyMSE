#include <log/log.h>

#include <cassert>
#include "core/sh2/peripherals/sh2_pfc.h"
#include "core/sh2/sh2_bus.h"

namespace SH2::OCPM::PFC
{

struct GpioState
{
	uint16_t output[2];
	uint16_t direction[2];
};

static GpioState gpio;

uint16_t read_gpio_inputs(int port)
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

uint16_t read16(uint32_t addr)
{
	addr &= 0x3F;

	int gpio_port;

	switch(addr)
	{
	case 0x00:
	case 0x02:
		gpio_port = (addr >> 1) & 1;
		{
			uint16_t input = read_gpio_inputs(gpio_port);
			uint16_t output = gpio.output[gpio_port];
			uint16_t direction = gpio.direction[gpio_port];
			return (output & direction) | (input & ~direction);
		}
	case 0x04:
	case 0x06:
		gpio_port = (addr >> 1) & 1;
		return gpio.direction[gpio_port];
	default:
		Log::warn("[PFC] unmapped read %08X", addr);
		return 0;
	}
}

void write16(uint32_t addr, uint16_t value)
{
	addr &= 0x3F;

	int gpio_port;

	switch(addr)
	{
	case 0x00:
	case 0x02:
		gpio_port = (addr >> 1) & 1;
		gpio.output[gpio_port] = value;
		Log::debug("[PFC] GPIO write P%sDR: %04X", gpio_port ? "B" : "A", value);
		break;
	case 0x04:
	case 0x06:
		gpio_port = (addr >> 1) & 1;
		gpio.direction[gpio_port] = value;
		Log::debug("[PFC] GPIO write P%sIOR: %04X", gpio_port ? "B" : "A", value);
		break;
	case 0x08:
	case 0x0C:
		gpio_port = (addr >> 2) & 1;
		Log::debug("[PFC] GPIO write P%sCR1: %04X", gpio_port ? "B" : "A", value);
		break;
	case 0x0A:
	case 0x0E:
		gpio_port = (addr >> 2) & 1;
		Log::debug("[PFC] GPIO write P%sCR2: %04X", gpio_port ? "B" : "A", value);
		break;
	default:
		Log::warn("[PFC] unmapped write %08X: %04X", addr, value);
	}
}

void initialize()
{
	gpio = {};
}

}  // namespace SH2::OCPM::PFC