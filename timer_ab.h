#pragma once
#include <cstdint>

uint8_t timer_ab_read_10_tmal();
uint8_t timer_ab_read_11_tmah();
uint8_t timer_ab_read_12_tmbl();
uint8_t timer_ab_read_13_tmbh();

void timer_ab_write_10_tmal(uint8_t value);
void timer_ab_write_11_tmah(uint8_t value);
void timer_ab_write_12_tmbl(uint8_t value);
void timer_ab_write_13_tmbh(uint8_t value);
void timer_ab_write_14_ctrl(uint8_t value);
void timer_ab_write_19_tmact(uint8_t value);

// Advance Timer A and Timer B by `base_cycles` (delta of `cycles` at CYCLES_SECOND Hz).
void timer_ab_step(uint64_t base_cycles);
