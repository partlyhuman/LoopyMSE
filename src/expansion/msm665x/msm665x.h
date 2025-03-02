#pragma once
#include <string>

#include "expansion/expansion.h"

namespace Expansion::MSM665X
{

bool enable(uint32_t cart_checksum);
void initialize(std::string rom_path);
void shutdown();

}  // namespace Expansion::MSM665X