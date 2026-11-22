#include "timer_ab.h"
#include "comm.h"
#include "state.h"
#include "CC800IOName.h"
#include <cstdio>

extern nc2k_states_t nc2k_states;

static inline uint32_t tmb_limit() {
    // SPDC1016 (PC1000): 12-bit Timer B; SPDC1024/1064 (NC1020/NC2000/NC3000): 16-bit Timer B
    return pc1000mode ? 0x1000u : 0x10000u;
}

static inline uint8_t tmb_hi_mask() {
    return pc1000mode ? 0x0Fu : 0xFFu;
}

static inline uint32_t get_osc_mult() {
    // PC1000/NC1020: PLL/OSC = 7.3728MHz, base CPU clock = 3.6864MHz (OSC / 2)
    // NC2000:        PLL/OSC = 14.7456MHz, base CPU clock = 4.9152MHz (OSC / 3)
    // NC3000:        base CPU clock = 9.8304MHz, OSC = 2 * base
    return nc2000mode ? 3u : 2u;
}

// Advance an auto-reloading up-counter in [0, limit) by `ticks`.
// Returns true if the counter overflowed at least once.
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

// Divide `osc_ticks` by 2^prescale (prescale in 1..15) using exact integer accumulator.
static inline uint64_t step_prescaled(uint64_t &acc, uint64_t osc_ticks, uint8_t prescale) {
    acc += osc_ticks;
    uint64_t ticks = acc >> prescale;
    acc &= (1ULL << prescale) - 1;
    return ticks;
}

// Convert `base_cycles` (at CYCLES_SECOND Hz) to 32.768 kHz crystal ticks with exact rational accumulator.
static inline uint64_t step_xtal32k(uint64_t &acc, uint64_t base_cycles) {
    acc += base_cycles * 32768ULL;
    uint64_t ticks = acc / CYCLES_SECOND;
    acc %= CYCLES_SECOND;
    return ticks;
}

// $10-$13 are read-to-clear per SPDC1016 / SPDC1024 datasheet (clears only the read byte of the live counter).
uint8_t timer_ab_read_tmal() {
    auto &ab = nc2k_states.timer_ab;
    uint8_t v = ab.tma_val & 0xFF;
    ab.tma_val &= 0xFF00;
    return v;
}

uint8_t timer_ab_read_tmah() {
    auto &ab = nc2k_states.timer_ab;
    uint8_t v = (ab.tma_val >> 8) & 0xFF;
    ab.tma_val &= 0x00FF;
    return v;
}

uint8_t timer_ab_read_tmbl() {
    auto &ab = nc2k_states.timer_ab;
    uint8_t v = ab.tmb_val & 0xFF;
    ab.tmb_val &= 0xFF00;
    return v;
}

uint8_t timer_ab_read_tmbh() {
    auto &ab = nc2k_states.timer_ab;
    uint8_t v = (ab.tmb_val >> 8) & tmb_hi_mask();
    ab.tmb_val &= 0x00FF;
    return v;
}

// Low byte updates reload latch; high byte updates reload latch and loads the live counter.
void timer_ab_write_tmal(uint8_t value) {
    auto &ab = nc2k_states.timer_ab;
    ab.tma_reload = (uint16_t)((ab.tma_reload & 0xFF00) | value);
}

void timer_ab_write_tmah(uint8_t value) {
    auto &ab = nc2k_states.timer_ab;
    ab.tma_reload = (uint16_t)((ab.tma_reload & 0x00FF) | ((uint16_t)value << 8));
    ab.tma_val = ab.tma_reload;
    ab.tma_acc = 0;
}

void timer_ab_write_tmbl(uint8_t value) {
    auto &ab = nc2k_states.timer_ab;
    ab.tmb_reload = (uint16_t)((ab.tmb_reload & 0xFF00) | value);
}

void timer_ab_write_tmbh(uint8_t value) {
    auto &ab = nc2k_states.timer_ab;
    uint8_t hi = value & tmb_hi_mask();
    ab.tmb_reload = (uint16_t)((ab.tmb_reload & 0x00FF) | ((uint16_t)hi << 8));
    ab.tmb_val = ab.tmb_reload;
    ab.tmb_acc = 0;
}

void timer_ab_write_ctrl(uint8_t value) {
    auto &ab = nc2k_states.timer_ab;
    uint8_t prev = nc2k_states.ram_io[0x14];
    if ((value & 0xF0) != (prev & 0xF0)) {
        ab.tma_acc = 0;
    }
    if ((value & 0x0F) != (prev & 0x0F)) {
        ab.tmb_acc = 0;
    }
    nc2k_states.ram_io[0x14] = value;
}

void timer_ab_on_write_19(uint8_t value) {
    if ((value & 0x10) != (nc2k_states.ram_io[0x19] & 0x10)) {
        // TMACT changed (Timer <-> Counter); reset accumulator to avoid stale clock scale
        nc2k_states.timer_ab.tma_acc = 0;
    }
}

void timer_ab_step(uint64_t base_cycles) {
    uint8_t ctrl = nc2k_states.ram_io[0x14];
    if (base_cycles == 0 || ctrl == 0) {
        return;
    }

    auto &ab = nc2k_states.timer_ab;
    uint64_t osc_ticks = base_cycles * get_osc_mult();

    // Timer A: controlled by TMAS[3:0] ($14 b7..4) and TMACT ($19 b4)
    uint8_t tmas = (ctrl >> 4) & 0x0F;
    if (tmas != 0) {
        bool tmact = (nc2k_states.ram_io[0x19] & 0x10) != 0;
        uint64_t ticks = 0;
        if (!tmact) {
            // Timer mode: internal OSC / 2^TMAS
            ticks = step_prescaled(ab.tma_acc, osc_ticks, tmas);
        } else if ((tmas & 0x08) == 0) {
            // Counter mode with TMAS3=0: external 32.768 kHz crystal (XXTAL)
            ticks = step_xtal32k(ab.tma_acc, base_cycles);
        }
        // Counter mode with TMAS3=1: external P44 (TMACI) pin edges (no external signal connected)

        if (ticks > 0 && advance_counter(ab.tma_val, ab.tma_reload, 0x10000u, ticks)) {
            if (debug_level >= 2) printf("timer A overflow\n");
            if (nc2k_states.inner_interrupt_control & 0x01) {
                nc2k_states.ram_io[io01_int_status] |= 0x01; // TMAI
            }
        }
    }

    // Timer B: always Timer mode, controlled by TMBS[3:0] ($14 b3..0), OSC / 2^TMBS
    uint8_t tmbs = ctrl & 0x0F;
    if (tmbs != 0) {
        uint64_t ticks = step_prescaled(ab.tmb_acc, osc_ticks, tmbs);
        if (ticks > 0 && advance_counter(ab.tmb_val, ab.tmb_reload, tmb_limit(), ticks)) {
            if (debug_level >= 2) printf("timer B overflow\n");
            if (nc2k_states.inner_interrupt_control & 0x02) {
                nc2k_states.ram_io[io01_int_status] |= 0x02; // TMBI
            }
        }
    }
}
