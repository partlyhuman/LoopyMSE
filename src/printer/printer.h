#pragma once

#include <cstdint>

#include <core/config.h>

namespace Printer
{

void initialize(Config::SystemInfo& config);
void shutdown();

bool motor_move_hook(uint32_t addr);
bool printer_hook(uint32_t addr);

}