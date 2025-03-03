#pragma once
#include "core/config.h"

namespace Expansion
{

constexpr uint32_t MAPPED_START = 0x040A0000;
constexpr uint32_t MAPPED_END = 0x040B0000;

void initialize(Config::CartInfo& cart);
void shutdown();

uint8_t exp_read8(uint32_t addr);
uint16_t exp_read16(uint32_t addr);
uint32_t exp_read32(uint32_t addr);
void exp_write8(uint32_t addr, uint8_t value);
void exp_write16(uint32_t addr, uint16_t value);
void exp_write32(uint32_t addr, uint32_t value);

}  // namespace Expansion