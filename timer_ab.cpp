#include "timer_ab.h"
#include "comm.h"
#include "state.h"
#include "CC800IOName.h"
#include <cstdio>

/*
 * this file is timerAB implementation (AI-written)
 */

extern nc2k_states_t nc2k_states;

// Timer B ($12/$13) width differs by chip (Timer A $10/$11 is always 16-bit):
// - SPDC1016 (PC1000): 12-bit (overflow at 0x1000, TMBH uses b3..b0 only)
// - SPDC1024/1064 (NC1020/2000/3000): 16-bit (overflow at 0x10000, TMBH uses b7..b0)
static inline uint32_t tmb_limit() {
    return pc1000mode ? 0x1000u : 0x10000u;
}

static inline uint8_t tmb_hi_mask() {
    return pc1000mode ? 0x0Fu : 0xFFu;
}

// Internal OSC rate relative to emulator base cycles (CYCLES_SECOND):
// - PC1000 (SPDC1016): OSC = 7.3728 MHz, CPU capped at OSC/2 = 3.6864 MHz -> 2x
// - NC1020/2000/3000 (SPDC1024/1064): OSC = 3.6864 / 4.9152 MHz = CPU clock -> 1x
static inline uint32_t get_osc_mult() {
    return pc1000mode ? 2u : 1u;
}

// Timer A/B are auto-reload UP-counters in [0, limit): overflow at `limit`
// reloads `val = reload`. Modulo handles multiple overflows in one step
// without losing phase; returns true if >=1 overflow happened.
static bool advance_counter(uint16_t &val, uint16_t reload, uint32_t limit, uint64_t ticks) {
    uint32_t first_ovf = limit - val;
    if (ticks < first_ovf) {
        val = (uint16_t)(val + ticks);
        return false;
    }
    uint64_t rem = ticks - first_ovf;
    uint32_t period = limit - reload;
    val = (uint16_t)(reload + (rem % period));
    return true;
}

// Stateless OSC / 2^prescale edge counter over (last_osc, osc] (prescale 1..15 => OSC/2 .. OSC/32768).
// Using (last, cur] diff instead of a per-timer remainder accumulator avoids
// stale remainder bugs when software switches $14/$19 mode or prescaler mid-run.
static inline uint64_t step_prescaled(uint64_t last_osc, uint64_t osc, uint8_t prescale) {
    return (osc >> prescale) - (last_osc >> prescale);
}

// Exact floor(c * freq / CYCLES_SECOND) without 64-bit mul overflow at large cycle counts.
static inline uint64_t ticks_at_freq(uint64_t c, uint64_t freq) {
    return (c / CYCLES_SECOND) * freq + (c % CYCLES_SECOND) * freq / CYCLES_SECOND;
}

// Stateless 32.768 kHz XTAL divider over (last_cycles, cycles] for TMACT=1, TMAS3=0.
// tmas_low3 = TMAS[2:0] in 1..7 => 32768 / 2^(N-1) Hz (32768, 16384, 8192, 4096, 2048, 1024, 512).
static inline uint64_t step_xtal32k(uint64_t last_cycles, uint64_t cycles, uint8_t tmas_low3) {
    uint64_t freq = 32768ULL >> (tmas_low3 - 1);
    return ticks_at_freq(cycles, freq) - ticks_at_freq(last_cycles, freq);
}

// $10 TMAL / $11 TMAH / $12 TMBL / $13 TMBH (Read):
// Datasheet specifies "READ TO CLEAR AL/AH/BL/BH". Return that byte of the live
// counter (`tm*_val`) and zero only that byte, keeping `tm*_reload` untouched.
uint8_t timer_ab_read_10_tmal() {
    auto &ab = nc2k_states.timer_ab;
    uint8_t v = ab.tma_val & 0xFF;
    ab.tma_val &= 0xFF00;
    return v;
}

uint8_t timer_ab_read_11_tmah() {
    auto &ab = nc2k_states.timer_ab;
    uint8_t v = (ab.tma_val >> 8) & 0xFF;
    ab.tma_val &= 0x00FF;
    return v;
}

uint8_t timer_ab_read_12_tmbl() {
    auto &ab = nc2k_states.timer_ab;
    uint8_t v = ab.tmb_val & 0xFF;
    ab.tmb_val &= 0xFF00;
    return v;
}

uint8_t timer_ab_read_13_tmbh() {
    auto &ab = nc2k_states.timer_ab;
    uint8_t v = (ab.tmb_val >> 8) & tmb_hi_mask(); // PC1000: only b3..b0 valid
    ab.tmb_val &= 0x00FF;
    return v;
}

// $10 TMAL / $11 TMAH / $12 TMBL / $13 TMBH (Write):
// Datasheet requires writing low byte first, then high byte.
// Low write stages `tm*_reload` without tearing the live counter; high write
// finishes `tm*_reload` and loads `tm*_val = tm*_reload`.
void timer_ab_write_10_tmal(uint8_t value) {
    auto &ab = nc2k_states.timer_ab;
    ab.tma_reload = (uint16_t)((ab.tma_reload & 0xFF00) | value);
}

void timer_ab_write_11_tmah(uint8_t value) {
    auto &ab = nc2k_states.timer_ab;
    ab.tma_reload = (uint16_t)((ab.tma_reload & 0x00FF) | ((uint16_t)value << 8));
    ab.tma_val = ab.tma_reload;
}

void timer_ab_write_12_tmbl(uint8_t value) {
    auto &ab = nc2k_states.timer_ab;
    ab.tmb_reload = (uint16_t)((ab.tmb_reload & 0xFF00) | value);
}

void timer_ab_write_13_tmbh(uint8_t value) {
    auto &ab = nc2k_states.timer_ab;
    uint8_t hi = value & tmb_hi_mask(); // PC1000: 12-bit Timer B ignores b7..b4
    ab.tmb_reload = (uint16_t)((ab.tmb_reload & 0x00FF) | ((uint16_t)hi << 8));
    ab.tmb_val = ab.tmb_reload;
}

// Called per step from cpu_loop_new.cpp while !clk_off (so both timers pause in sleep).
// $14 and $19 need no write hooks in io_new.cpp — read live from ram_io[] here.
void timer_ab_step(uint64_t last_cycles, uint64_t cycles) {
    // $14 (TMAB_CTRL): b7..b4 = TMAS[3:0] (Timer A), b3..b0 = TMBS[3:0] (Timer B); 0000 = OFF
    uint8_t ctrl = nc2k_states.ram_io[0x14];
    if (cycles <= last_cycles || ctrl == 0) {
        return;
    }

    auto &ab = nc2k_states.timer_ab;
    uint32_t mult = get_osc_mult();
    uint64_t last_osc = last_cycles * mult;
    uint64_t osc = cycles * mult;

    // Timer A: clock source chosen by $14 b7..b4 (TMAS[3:0]) and $19 b4 (TMACT)
    // ($19 b5=TMAG_i gate, b6=TMA_o, b7=TMB_o are external pin muxes, not emulated)
    uint8_t tmas = (ctrl >> 4) & 0x0F;
    if (tmas != 0) {
        // $19 b4 (TMACT): 0 = Timer mode (OSC), 1 = Counter mode (32.768 kHz or P44)
        bool tmact = (nc2k_states.ram_io[0x19] & 0x10) != 0;
        uint64_t ticks = 0;
        if (!tmact) {
            // TMACT=0: TMAS 0001..1111 => internal OSC / 2^TMAS (OSC/2 .. OSC/32768)
            ticks = step_prescaled(last_osc, osc, tmas);
        } else if ((tmas & 0x08) == 0) {
            // TMACT=1, TMAS3(CLKIS)=0: TMAS[2:0] 001..111 => 32768 Hz / 2^(TMAS[2:0] - 1)
            // (tmas != 0 and bit3 == 0 guarantees TMAS[2:0] is in 1..7)
            ticks = step_xtal32k(last_cycles, cycles, tmas & 0x07);
        }
        // TMACT=1, TMAS3(CLKIS)=1: external P44 (TMACI) pin / 2^(TMAS[2:0]-1) (1000=OFF); not emulated

        if (ticks > 0 && advance_counter(ab.tma_val, ab.tma_reload, 0x10000u, ticks)) {
            if (debug_level >= 2) printf("timer A overflow\n");
            // $01 W b0 = TMAIE (inner_interrupt_control), $01 R b0 = TMAI (ram_io[0x01], read-to-clear).
            // Setting ram_io[0x01] drives the level-triggered /IRQ line in cpu_loop_new.cpp.
            if (nc2k_states.inner_interrupt_control & 0x01) {
                nc2k_states.ram_io[io01_int_status] |= 0x01; // TMAI
            }
        }
    }

    // Timer B: $14 b3..b0 (TMBS[3:0]), Timer mode only — 0001..1111 => OSC / 2^TMBS
    uint8_t tmbs = ctrl & 0x0F;
    if (tmbs != 0) {
        uint64_t ticks = step_prescaled(last_osc, osc, tmbs);
        if (ticks > 0 && advance_counter(ab.tmb_val, ab.tmb_reload, tmb_limit(), ticks)) {
            if (debug_level >= 2) printf("timer B overflow\n");
            // $01 W b1 = TMBIE (inner_interrupt_control), $01 R b1 = TMBI (ram_io[0x01], read-to-clear).
            if (nc2k_states.inner_interrupt_control & 0x02) {
                nc2k_states.ram_io[io01_int_status] |= 0x02; // TMBI
            }
        }
    }
}
