#pragma once

#include <cstdint>

#include <core/config.h>

namespace Printer
{

void initialize(Config::SystemInfo& config);
void shutdown();

bool motor_move_hook(uint32_t src_addr, uint32_t dst_addr);
bool printer_hook(uint32_t src_addr, uint32_t dst_addr);

}