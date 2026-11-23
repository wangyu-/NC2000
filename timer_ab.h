#pragma once
#include <cstdint>

// $10-$13 read-to-clear live counter bytes
uint8_t timer_ab_read_10_tmal();
uint8_t timer_ab_read_11_tmah();
uint8_t timer_ab_read_12_tmbl();
uint8_t timer_ab_read_13_tmbh();

// $10-$13 reload writes (low stages reload; high finishes reload & loads live counter)
void timer_ab_write_10_tmal(uint8_t value);
void timer_ab_write_11_tmah(uint8_t value);
void timer_ab_write_12_tmbl(uint8_t value);
void timer_ab_write_13_tmbh(uint8_t value);

// Advance Timer A/B across (last_cycles, cycles] using $14 (TMAS/TMBS), $19 b4 (TMACT), and $01 (TMAIE/TMBIE).
void timer_ab_step(uint64_t last_cycles, uint64_t cycles);
