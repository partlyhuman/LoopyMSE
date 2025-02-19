#include <algorithm>
#include <cassert>
#include <common/bswp.h>
#include "core/sh2/peripherals/sh2_dmac.h"
#include "core/sh2/peripherals/sh2_intc.h"
#include "core/sh2/peripherals/sh2_serial.h"
#include "core/sh2/peripherals/sh2_timers.h"
#include "core/sh2/sh2.h"
#include "core/sh2/sh2_bus.h"
#include "core/sh2/sh2_interpreter.h"
#include "core/sh2/sh2_local.h"
#include "core/memory.h"
#include "core/timing.h"

namespace SH2
{

CPU sh2;

static Timing::FuncHandle irq_func;
static Timing::EventHandle irq_ev;

static bool can_exec_irq(int prio)
{
	int imask = (sh2.sr >> 4) & 0xF;
	return prio > imask;
}

static void handle_irq(uint64_t param, int cycles_late)
{
	int prio = param & 0xFF;
	int vector = param >> 8;

	if (!can_exec_irq(prio))
	{
		return;
	}

	raise_exception(vector);

	int new_imask = std::clamp(prio, 0, 15);

	//Interrupt mask should only be modified after the above function so that the original value can be pushed onto the stack
	sh2.sr &= ~0xF0;
	sh2.sr |= new_imask << 4;
}

void initialize()
{
	sh2 = {};

	sh2.pagetable = Memory::get_sh2_pagetable();

	//TODO: make config option to skip BIOS boot?
	bool skip_bios_boot = false;
	if (skip_bios_boot)
	{
		set_pc(0x0E000480);
		sh2.gpr[15] = 0;
	}
	else
	{
		//The initial values of PC and SP are read from the vector table
		int boot_type = 0;
		uint8_t* boot_vectors = sh2.pagetable[0];
		uint32_t reset_pc, reset_sp;
		memcpy(&reset_pc, boot_vectors + boot_type*8 + 0, 4);
		memcpy(&reset_sp, boot_vectors + boot_type*8 + 4, 4);
		set_pc(Common::bswp32(reset_pc));
		sh2.gpr[15] = Common::bswp32(reset_sp);
	}

	//Next, VBR is cleared to zero and interrupt mask bits in SR are set to 1111
	sh2.vbr = 0;
	sh2.sr |= 0xF << 4;

	Timing::register_timer(Timing::CPU_TIMER, &sh2.cycles_left, run);

	irq_func = Timing::register_func("SH2::handle_irq", handle_irq);

	//Set up on-chip peripheral modules after CPU is done
	OCPM::DMAC::initialize();
	OCPM::INTC::initialize();
	OCPM::Serial::initialize();
	OCPM::Timer::initialize();
}

void shutdown()
{
	//nop
}

void run()
{
	while (sh2.cycles_left)
	{
		//Hack to make each instruction take 2-3 cycles, a lot closer to realistic speed
		//until we have a proper pipeline and memory-region-dependent fetch delays.
		if ((sh2.cycles_left % 5) < 2)
		{
			uint16_t instr = Bus::read16(sh2.pc - 4);
			SH2::Interpreter::run(instr);
			sh2.pc += 2;
		}
		sh2.cycles_left -= 1;
	}
}

void assert_irq(int vector_id, int prio)
{
	sh2.pending_irq_vector = vector_id;
	sh2.pending_irq_prio = prio;
	irq_check();
}

void irq_check()
{
	if (!can_exec_irq(sh2.pending_irq_prio))
	{
		return;
	}

	//Ensure the interrupt occurs after the CPU has executed
	uint64_t param = sh2.pending_irq_prio | (sh2.pending_irq_vector << 8);
	Timing::add_event(irq_func, Timing::convert_cpu(1), param, Timing::CPU_TIMER);
}

void raise_exception(int vector_id)
{
	assert(vector_id < 0x100);

	//Push SR and PC onto the stack
	sh2.gpr[15] -= 4;
	Bus::write32(sh2.gpr[15], sh2.sr);
	sh2.gpr[15] -= 4;
	Bus::write32(sh2.gpr[15], sh2.pc - 4);

	uint32_t vector_addr = sh2.vbr + (vector_id * 4);
	uint32_t new_pc = Bus::read32(vector_addr);

	set_pc(new_pc);
}

void set_pc(uint32_t new_pc)
{
	//Needs to be + 4 to account for pipelining
	sh2.pc = new_pc + 4;
}

void set_sr(uint32_t new_sr)
{
	sh2.sr = new_sr & 0x3F3;
	irq_check();
}

}