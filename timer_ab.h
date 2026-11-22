#pragma once
#include <cstdint>

uint8_t timer_ab_read_tmal();
uint8_t timer_ab_read_tmah();
uint8_t timer_ab_read_tmbl();
uint8_t timer_ab_read_tmbh();

void timer_ab_write_tmal(uint8_t value);
void timer_ab_write_tmah(uint8_t value);
void timer_ab_write_tmbl(uint8_t value);
void timer_ab_write_tmbh(uint8_t value);
void timer_ab_write_ctrl(uint8_t value);
void timer_ab_on_write_19(uint8_t value);

// Advance Timer A and Timer B by `base_cycles` (delta of `cycles` at CYCLES_SECOND Hz).
void timer_ab_step(uint64_t base_cycles);
