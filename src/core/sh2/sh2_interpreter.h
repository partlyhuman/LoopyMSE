#pragma once
#include <cstdint>

namespace SH2::Interpreter
{

void run(uint16_t instr, uint32_t src_addr);

}