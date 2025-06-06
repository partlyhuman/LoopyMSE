#include <log/log.h>

#include <cassert>
#include "core/sh2/peripherals/sh2_dmac.h"
#include "core/sh2/sh2_bus.h"

namespace SH2::OCPM::DMAC
{

struct Channel
{
	uint32_t src_addr;
	uint32_t dst_addr;
	uint32_t transfer_size;

	struct Ctrl
	{
		int enable;
		int finished;
		int irq_enable;
		int transfer_16bit;
		int is_burst;
		int unk_ack_bits;
		int mode;
		int src_step;
		int dst_step;
	};

	Ctrl ctrl;

	uint16_t get_ctrl()
	{
		uint16_t result = ctrl.enable;
		result |= ctrl.finished << 1;
		result |= ctrl.irq_enable << 2;
		result |= ctrl.transfer_16bit << 3;
		result |= ctrl.is_burst << 4;
		result |= ctrl.unk_ack_bits << 5;
		result |= ctrl.mode << 8;
		result |= ctrl.src_step << 12;
		result |= ctrl.dst_step << 14;
		return result;
	}

	void set_ctrl(uint16_t value)
	{
		ctrl.enable = value & 0x1;
		ctrl.finished &= (value >> 1) & 0x1;
		ctrl.irq_enable = (value >> 2) & 0x1;
		ctrl.transfer_16bit = (value >> 3) & 0x1;
		ctrl.is_burst = (value >> 4) & 0x1;
		ctrl.unk_ack_bits = (value >> 5) & 0x7;
		ctrl.mode = ((value >> 8) & 0xF);
		ctrl.src_step = (value >> 12) & 0x3;
		ctrl.dst_step = (value >> 14) & 0x3;
	}

	void start_transfer();
};

struct State
{
	Channel chan[4];
	int dreqs[(int)DREQ::NumDreq];
	uint16_t ctrl;
};

static State state;
static bool in_dma_state;

void Channel::start_transfer()
{
	//TODO: time these transfers instead of doing them all at once?
	assert(!ctrl.irq_enable);

	int src_step = 0;
	switch (ctrl.src_step)
	{
	case 1:
		src_step = 1;
		break;
	case 2:
		src_step = -1;
		break;
	}

	int dst_step = 0;
	switch (ctrl.dst_step)
	{
	case 1:
		dst_step = 1;
		break;
	case 2:
		dst_step = -1;
		break;
	}

	src_step <<= ctrl.transfer_16bit;
	dst_step <<= ctrl.transfer_16bit;

	//TODO: speed this up by doing memcpy if both addresses are in Memory
	if (ctrl.transfer_16bit)
	{
		Log::debug("[DMAC] start 16bit transfer src:%08X dst:%08X size:%08X sstep:%d dstep:%d", src_addr, dst_addr, transfer_size, src_step, dst_step);
		in_dma_state = true;
		while (transfer_size && state.dreqs[ctrl.mode])
		{
			uint16_t value = SH2::Bus::read16(src_addr);
			SH2::Bus::write16(dst_addr, value);

			src_addr += src_step;
			dst_addr += dst_step;
			transfer_size--;
		}
		in_dma_state = false;
	}
	else
	{
		Log::debug("[DMAC] start 8bit transfer src:%08X dst:%08X size:%08X sstep:%d dstep:%d", src_addr, dst_addr, transfer_size, src_step, dst_step);
		in_dma_state = true;
		while (transfer_size && state.dreqs[ctrl.mode])
		{
			uint8_t value = SH2::Bus::read8(src_addr);
			SH2::Bus::write8(dst_addr, value);

			src_addr += src_step;
			dst_addr += dst_step;
			transfer_size--;
		}
		in_dma_state = false;
	}

	if (!transfer_size)
	{
		ctrl.finished = true;
	}
}

static void check_activations()
{
	//TODO: check NMI and address error flags
	bool master_enable = state.ctrl & 0x1;
	if (!master_enable)
	{
		return;
	}

	for (auto& x : state.chan)
	{
		if (x.ctrl.enable && !x.ctrl.finished && state.dreqs[x.ctrl.mode])
		{
			x.start_transfer();
		}
	}
}

uint16_t read16(uint32_t addr)
{
	addr &= 0x3F;
	if (addr == 0x08)
	{
		return state.ctrl;
	}

	int reg = addr & 0x0F;
	Channel* chan = &state.chan[addr >> 4];
	switch (reg)
	{
	case 0x0A:
		return chan->transfer_size & 0xFFFF;
	case 0x0E:
		return chan->get_ctrl();
	default:
		assert(0);
		return 0;
	}
}

void write16(uint32_t addr, uint16_t value)
{
	addr &= 0x3F;

	if (addr == 0x08)
	{
		state.ctrl = value;
		return;
	}

	int reg = addr & 0x0F;
	Channel* chan = &state.chan[addr >> 4];
	switch (reg)
	{
	case 0x0A:
		chan->transfer_size = value;
		if (!chan->transfer_size)
		{
			chan->transfer_size = 0x10000;
		}
		break;
	case 0x0E:
		chan->set_ctrl(value);
		check_activations();
		break;
	default:
		assert(0);
	}
}

void write32(uint32_t addr, uint32_t value)
{
	addr &= 0x3F;

	int reg = addr & 0x0F;
	Channel* chan = &state.chan[addr >> 4];
	switch (reg)
	{
	case 0x00:
		chan->src_addr = value;
		break;
	case 0x04:
		chan->dst_addr = value;
		break;
	default:
		assert(0);
	}
}

void initialize()
{
	state = {};
	in_dma_state = false;

	//Auto mode should always go through
	send_dreq(DREQ::Auto);
}

void send_dreq(DREQ dreq)
{
	state.dreqs[(int)dreq] = true;
	check_activations();
}

void clear_dreq(DREQ dreq)
{
	state.dreqs[(int)dreq] = false;
}

bool is_dma_access()
{
	return in_dma_state;
}

}