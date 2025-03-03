#pragma once
#include <string>

#include "expansion/expansion.h"

namespace Expansion::MSM665X
{

bool enable(uint32_t cart_checksum);
void initialize(std::string rom_path);
void shutdown();
void unmapped_write8(uint32_t addr, uint8_t value);

}  // namespace Expansion::MSM665X