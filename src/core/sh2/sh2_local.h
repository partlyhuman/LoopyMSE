#pragma once
#include <cstdint>
#include <unordered_map>

namespace SH2
{

typedef bool (*BranchHookFunc)(uint32_t, uint32_t);

struct CPU
{
	uint32_t gpr[16];
	uint32_t pc;
	uint32_t pr;
	uint32_t macl, mach;
	uint32_t gbr, vbr;
	uint32_t sr;

	int32_t cycles_left;

	int pending_exception_prio;
	int pending_exception_vector;

	uint8_t** pagetable;

	std::unordered_map<uint32_t, BranchHookFunc> branch_hooks;

	bool fetch_done;
	int fetch_cycles;

	uint16_t pipeline_instruction;
	bool pipeline_valid;

	bool in_delay_slot;
	bool in_nointerrupt_slot;
};

extern CPU sh2;

void assert_irq(int vector_id, int prio);
void raise_exception(int vector_id);
void set_pc(uint32_t new_pc);
void set_sr(uint32_t new_sr);

void add_hook(uint32_t address, BranchHookFunc hook);
void remove_hook(uint32_t address);

}