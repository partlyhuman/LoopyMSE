#include "core/sh2/peripherals/sh2_serial.h"

#include <log/log.h>
#include <sound/sound.h>

#include <cassert>
#include <cstdio>
#include <functional>

#include "core/sh2/peripherals/sh2_dmac.h"
#include "core/timing.h"

namespace SH2::OCPM::Serial
{

constexpr static int PORT_COUNT = 2;

static Timing::FuncHandle tx_ev_func;

struct Port
{
	Timing::EventHandle tx_ev;
	DMAC::DREQ rx_dreq_id, tx_dreq_id;

	int id;
	int bit_factor;
	int cycles_per_bit;

	struct Mode
	{
		int clock_factor;
		int mp_enable;
		int stop_bit_length;
		int parity_mode;
		int parity_enable;
		int seven_bit_mode;
		int sync_mode;
	};

	Mode mode;

	struct Ctrl
	{
		int clock_mode;
		int tx_end_intr_enable;
		int mp_intr_enable;
		int rx_enable;
		int tx_enable;
		int rx_intr_enable;
		int tx_intr_enable;
	};

	Ctrl ctrl;

	struct Status
	{
		int tx_empty;
	};

	Status status;

	int tx_bits_left;
	uint8_t tx_shift_reg;
	uint8_t tx_buffer;
	uint8_t tx_prepared_data;

	std::function<void(uint8_t)> tx_callback;

	void calc_cycles_per_bit()
	{
		assert(!mode.sync_mode);
		cycles_per_bit = (32 << (mode.clock_factor * 2)) * (bit_factor + 1);
	}

	void tx_start(uint8_t value)
	{
		tx_bits_left = 8;
		tx_shift_reg = value;
		status.tx_empty = true;
		sched_tx_ev();
	}

	void sched_tx_ev()
	{
		Timing::UnitCycle sched_cycles = Timing::convert_cpu(cycles_per_bit);
		tx_ev = Timing::add_event(tx_ev_func, sched_cycles, (uint64_t)this, Timing::CPU_TIMER);
	}
};

struct State
{
	Port ports[PORT_COUNT];
};

static State state;

static void check_tx_dreqs()
{
	for (auto& port : state.ports)
	{
		if (port.status.tx_empty && port.ctrl.tx_enable)
		{
			DMAC::send_dreq(port.tx_dreq_id);
		}
	}
}

static void tx_event(uint64_t param, int cycles_late)
{
	assert(!cycles_late);
	Port* port = (Port*)param;

	bool bit = port->tx_shift_reg & 0x1;
	port->tx_shift_reg >>= 1;
	port->tx_prepared_data >>= 1;
	port->tx_prepared_data |= bit << 7;
	port->tx_bits_left--;

	if (!port->tx_bits_left)
	{
		Log::debug("[Serial] port%d tx %02X", port->id, port->tx_prepared_data);

		if (port->tx_callback != nullptr)
		{
			port->tx_callback(port->tx_prepared_data);
		}

		if (!port->status.tx_empty)
		{
			port->tx_start(port->tx_buffer);
			check_tx_dreqs();
		}
		else
		{
			//TODO: can this trigger an interrupt?
			Log::debug("[Serial] port%d finished tx", port->id);
		}
	}
	else
	{
		port->sched_tx_ev();
	}
}

void initialize()
{
	state = {};

	tx_ev_func = Timing::register_func("Serial::tx_event", tx_event);

	for (int i = 0; i < PORT_COUNT; i++)
	{
		state.ports[i].id = i;
		state.ports[i].status.tx_empty = true;
		state.ports[i].calc_cycles_per_bit();
	}

	state.ports[0].rx_dreq_id = DMAC::DREQ::RXI0;
	state.ports[1].rx_dreq_id = DMAC::DREQ::RXI1;
	state.ports[0].tx_dreq_id = DMAC::DREQ::TXI0;
	state.ports[1].tx_dreq_id = DMAC::DREQ::TXI1;
}

uint8_t read8(uint32_t addr)
{
	addr &= 0xF;
	Port* port = &state.ports[addr >> 3];
	int reg = addr & 0x7;

	uint8_t value = 0;
	switch (reg)
	{
	case 0x04:
		//TODO implement other status bits
		value = port->status.tx_empty << 7;
		break;
	default:
		Log::debug("[Serial] read port%d reg%d: %02X", port->id, reg, value);
		break;
	}
	return value;
}

void write8(uint32_t addr, uint8_t value)
{
	addr &= 0xF;
	Port* port = &state.ports[addr >> 3];
	int reg = addr & 0x7;

	switch (reg)
	{
	case 0x00:
		Log::debug("[Serial] write port%d mode: %02X", port->id, value);
		port->mode.clock_factor = value & 0x3;
		port->mode.mp_enable = (value >> 2) & 0x1;
		port->mode.stop_bit_length = (value >> 3) & 0x1;
		port->mode.parity_mode = (value >> 4) & 0x1;
		port->mode.parity_enable = (value >> 5) & 0x1;
		port->mode.seven_bit_mode = (value >> 6) & 0x1;
		port->mode.sync_mode = (value >> 7) & 0x1;
		assert(!(value & ~0x3));
		break;
	case 0x01:
		Log::debug("[Serial] write port%d bitrate factor: %02X", port->id, value);
		port->bit_factor = value;
		port->calc_cycles_per_bit();
		Log::debug("[Serial] set port%d baudrate: %d bit/s", port->id, Timing::F_CPU / port->cycles_per_bit);
		break;
	case 0x02:
		Log::debug("[Serial] write port%d ctrl: %02X", port->id, value);
		port->ctrl.clock_mode = value & 0x3;
		port->ctrl.tx_end_intr_enable = (value >> 2) & 0x1;
		port->ctrl.mp_intr_enable = (value >> 3) & 0x1;
		port->ctrl.rx_enable = (value >> 4) & 0x1;
		port->ctrl.tx_enable = (value >> 5) & 0x1;
		port->ctrl.rx_intr_enable = (value >> 6) & 0x1;
		port->ctrl.tx_intr_enable = (value >> 7) & 0x1;

		if (!port->ctrl.tx_enable)
		{
			port->status.tx_empty = true;
		}

		check_tx_dreqs();
		break;
	case 0x03:
		Log::debug("[Serial] write port%d data: %02X", port->id, value);
		port->tx_buffer = value;

		if (!(port->status.tx_empty && port->ctrl.tx_enable))
		{
			break;
		}

		// Data written from DMA automatically sets the tx_empty bit
		if (DMAC::is_dma_access())
		{
			port->status.tx_empty = false;
			if (!port->tx_bits_left)
			{
				//Space is available, start the timed transfer
				port->tx_start(port->tx_buffer);
			}
			else
			{
				//Byte transfer is in progress, clear DREQ
				DMAC::clear_dreq(port->tx_dreq_id);
			}
		}
		break;
	case 0x04:
	{
		//TODO implement other status bits
		Log::debug("[Serial] write port%d status: %02X", port->id, value);
		bool new_empty = (value >> 7) & 0x1;
		if(port->status.tx_empty && !new_empty)
		{
			port->status.tx_empty &= new_empty;
			if (!port->tx_bits_left)
			{
				//Space is available, start the timed transfer
				port->tx_start(port->tx_buffer);
			}
		}
		break;
	}
	default:
		assert(0);
	}
}

void set_tx_callback(int port, std::function<void(uint8_t)> callback)
{
	assert(port >= 0 && port < PORT_COUNT);
	state.ports[port].tx_callback = callback;
}

}  // namespace SH2::OCPM::Serial