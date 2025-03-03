#pragma once
#include "core/config.h"

namespace Expansion
{

void initialize(Config::CartInfo& cart);
void shutdown();

// uint8_t unmapped_read8(uint32_t addr);
// uint16_t unmapped_read16(uint32_t addr);
// uint32_t unmapped_read32(uint32_t addr);
void unmapped_write8(uint32_t addr, uint8_t value);
// void unmapped_write16(uint32_t addr, uint16_t value);
// void unmapped_write32(uint32_t addr, uint32_t value);

}  // namespace Expansion