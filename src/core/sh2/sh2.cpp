#include <algorithm>
#include <cassert>
#include <cstring>

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

static bool can_accept_exception(int vector_id, int prio)
{
	int imask = (sh2.sr >> 4) & 0xF;
	if (imask == 0xF)
	{
		return false;
	}
	return prio > imask;
}

static bool can_execute_exception(int vector_id, int prio)
{
	//Some types not accepted after certain instructions (SH7021 datasheet tables 4.9 & 4.2)
	bool is_address_error = (vector_id >= 9 && vector_id <= 10);
	bool is_interrupt = (vector_id >= 11 && vector_id <= 12) || (vector_id >= 64);

	//Our implementation of the pipeline explodes if we allow any exception
	//right after the pipeline became invalid. This fixes it.
	if (!sh2.pipeline_valid)
	{
		return false;
	}
	if (sh2.in_delay_slot && (is_address_error || is_interrupt))
	{
		return false;
	}
	if (sh2.in_nointerrupt_slot && is_interrupt)
	{
		return false;
	}

	return true;
}

static bool handle_exception()
{
	if (sh2.pending_exception_vector)
	{
		int vector = sh2.pending_exception_vector;
		int prio = sh2.pending_exception_prio;
		if (can_execute_exception(vector, prio))
		{
			raise_exception(vector);
		
			int new_imask = std::clamp(prio, 0, 15);
		
			//Interrupt mask should only be modified after the above function so that the original value can be pushed onto the stack
			sh2.sr &= ~0xF0;
			sh2.sr |= new_imask << 4;

			sh2.pending_exception_vector = 0;
			return true;
		}
	}
	return false;
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

	//Initialize pipeline & execution state
	sh2.pipeline_valid = false;
	sh2.in_delay_slot = false;
	sh2.in_nointerrupt_slot = false;
	sh2.fetch_cycles = 1;

	Timing::register_timer(Timing::CPU_TIMER, &sh2.cycles_left, run);

	//Set up on-chip peripheral modules after CPU is done
	OCPM::DMAC::initialize();
	OCPM::INTC::initialize();
	OCPM::Serial::initialize();
	OCPM::Timer::initialize();
}

void shutdown()
{
	sh2.branch_hooks.clear();
}

void run()
{
	while (sh2.cycles_left)
	{
		bool last_instruction_done = true; //TODO: wait on longer instructions like multiply

		sh2.fetch_cycles -= 1;
		if (sh2.fetch_cycles <= 0)
		{
			sh2.fetch_cycles = 0;
			sh2.fetch_done = true;
		}

		bool pipeline_ready = sh2.fetch_done && last_instruction_done;
		
		if (pipeline_ready)
		{
			//Handle any pending exceptions first, this may change the following fetch
			handle_exception();

			//Start the next fetch with the current PC
			uint32_t fetch_src_addr = sh2.pc;
			uint16_t fetch_instruction = Bus::read16(fetch_src_addr);
			sh2.fetch_cycles = Bus::read_cycles(fetch_src_addr);
			sh2.fetch_done = false;
			
			//Advance the pipeline
			uint32_t execute_src_addr = sh2.pipeline_src_addr;
			uint16_t execute_instruction = sh2.pipeline_instruction;
			bool execute_valid = sh2.pipeline_valid;
			sh2.pipeline_src_addr = fetch_src_addr;
			sh2.pipeline_instruction = fetch_instruction;
			sh2.pipeline_valid = true;
			sh2.pc += 2;

			//Find and run the hook function at this address
			//TODO: split into smaller paged maps for performance
			if (sh2.branch_hooks.find(execute_src_addr) != sh2.branch_hooks.end())
			{
				SH2::BranchHookFunc hook = sh2.branch_hooks.at(execute_src_addr);
				
				//If hook returns true, the actual instruction is skipped
				if (hook(execute_src_addr))
				{
					execute_valid = false;
				}
			}

			//Execute whatever just came off the pipeline
			bool was_delay_slot = sh2.in_delay_slot;
			bool was_nointerrupt_slot = sh2.in_nointerrupt_slot;
			if (execute_valid)
			{
				SH2::Interpreter::run(execute_instruction, execute_src_addr);
			}
			//This should probably be done more directly in the interpreter
			if (was_delay_slot)
			{
				sh2.in_delay_slot = false;
			}
			if (was_nointerrupt_slot)
			{
				sh2.in_nointerrupt_slot = false;
			}
		}
		sh2.cycles_left -= 1;
	}
}

void assert_irq(int vector_id, int prio)
{
	if (!can_accept_exception(vector_id, prio))
	{
		return;
	}
	sh2.pending_exception_vector = vector_id;
	sh2.pending_exception_prio = prio;
}

void raise_exception(int vector_id)
{
	assert(vector_id < 0x100);

	//Push SR and PC onto the stack
	sh2.gpr[15] -= 4;
	Bus::write32(sh2.gpr[15], sh2.sr);
	sh2.gpr[15] -= 4;
	Bus::write32(sh2.gpr[15], sh2.pc - 2);

	uint32_t vector_addr = sh2.vbr + (vector_id * 4);
	uint32_t new_pc = Bus::read32(vector_addr);

	set_pc(new_pc);
	sh2.pipeline_valid = false;
}

void set_pc(uint32_t new_pc)
{
	sh2.pc = new_pc;
}

void set_sr(uint32_t new_sr)
{
	sh2.sr = new_sr & 0x3F3;
}

void add_hook(uint32_t address, BranchHookFunc hook)
{
	sh2.branch_hooks.emplace(address, hook);
}

void remove_hook(uint32_t address)
{
	sh2.branch_hooks.erase(address);
}

}