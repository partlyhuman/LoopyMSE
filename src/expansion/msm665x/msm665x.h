/*
* This is an extremely proof-of-concept high level replacement for Wanwan Aijou Monogatari's OKI MSM665X expansion audio chip.
* Subject to change.
*
* Based on research and python implementation by kasami 2025
* Quick and dirty port by partlyhuman, so don't blame kasami for this module!
*/
#pragma once
#include <string>

#include "expansion/expansion.h"

namespace Expansion::MSM665X
{

bool enable(uint32_t cart_checksum);
bool is_enabled();
void initialize(std::string rom_path);
void shutdown();
void write8(uint32_t addr, uint8_t value);

}  // namespace Expansion::MSM665X