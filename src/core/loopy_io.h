#pragma once
#include <cstdint>

#include "core/config.h"

namespace LoopyIO
{

constexpr static int BASE_ADDR = 0x0405D000;
constexpr static int END_ADDR = 0x0405E000;

void initialize(Config::SystemInfo &info);
void shutdown(Config::SystemInfo &info);

uint8_t reg_read8(uint32_t addr);
uint16_t reg_read16(uint32_t addr);
uint32_t reg_read32(uint32_t addr);

void reg_write8(uint32_t addr, uint8_t value);
void reg_write16(uint32_t addr, uint16_t value);
void reg_write32(uint32_t addr, uint32_t value);

Config::ControllerType get_plugged_controller();
void set_plugged_controller(Config::ControllerType type);
void set_controller_scan_mode(bool scan_pad, bool scan_mouse);
void update_pad(int btn_info, bool pressed);
void update_mouse_buttons(int btn_info, bool pressed);
void update_mouse_position(int delta_x, int delta_y);

void update_print_temp();
void update_sensors();

}  // namespace LoopyIO