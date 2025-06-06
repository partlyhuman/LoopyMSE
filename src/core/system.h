#pragma once
#include "core/config.h"

namespace System
{

void initialize(Config::SystemInfo& config);
void shutdown(Config::SystemInfo& config);

void run();

uint16_t* get_display_output();

}