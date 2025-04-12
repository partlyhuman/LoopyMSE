#pragma once
#include <cstdint>

#define READ_HALFWORD(group, addr)                       \
    uint32_t wordop_addr = (addr);                       \
	uint16_t wordop_value = group##_read16(wordop_addr); \
	return (uint8_t)((wordop_addr & 1) ? (wordop_value & 0xFF) : (wordop_value >> 8));

#define READ_DOUBLEWORD(group, addr) \
    uint32_t wordop_addr = (addr);   \
	return (uint32_t)((group##_read16(wordop_addr) << 16) | (group##_read16(wordop_addr + 2)));

#define WRITE_HALFWORD(group, addr, value)        \
    uint32_t wordop_addr = (addr);                \
    uint8_t wordop_value = (value);               \
	int wordop_shift = (wordop_addr & 1) ? 0 : 8; \
	int wordop_mask = ~(0xFF << wordop_shift);    \
	group##_write16(wordop_addr, (uint16_t)((group##_read16(wordop_addr) & wordop_mask) | (wordop_value << wordop_shift)));

#define WRITE_DOUBLEWORD(group, addr, value)                      \
    uint32_t wordop_addr = (addr);                                \
    uint32_t wordop_value = (value);                              \
    group##_write16(wordop_addr, (uint16_t)(wordop_value >> 16)); \
    group##_write16(wordop_addr + 2, (uint16_t)(wordop_value & 0xFFFF));
