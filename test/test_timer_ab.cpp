// Build & run from repo root:
//   g++ -std=c++11 -I. $(sdl2-config --cflags) test/test_timer_ab.cpp timer_ab.cpp -o /tmp/test_timer_ab && /tmp/test_timer_ab

#include <cassert>
#include <cstdio>
#include <cstring>
#include "comm.h"
#include "state.h"
#include "timer_ab.h"

// Minimal globals required by timer_ab.cpp
nc2k_states_t nc2k_states;
bool pc1000mode = false, nc1020mode = false, nc2000mode = false, nc3000mode = false;
uint32_t CYCLES_SECOND = 3686400;
int debug_level = 0;

static uint64_t g_cyc = 0;

static void reset_all(bool pc1k, bool nc1020, bool nc2k, bool nc3k, uint32_t cps) {
    pc1000mode = pc1k; nc1020mode = nc1020; nc2000mode = nc2k; nc3000mode = nc3k;
    CYCLES_SECOND = cps;
    nc2k_states.zero_init();
    g_cyc = 0;
}

static void step(uint64_t delta) {
    uint64_t next = g_cyc + delta;
    timer_ab_step(g_cyc, next);
    g_cyc = next;
}

int main() {
    // 1. PC1000: Timer A prescale OSC/2 (TMAS=1 -> 1 tick per base_cycle), auto-reload, TMAIE masking
    reset_all(true, false, false, false, 3686400);
    timer_ab_write_10_tmal(0xF0);
    timer_ab_write_11_tmah(0xFF); // reload=0xFFF0, val=0xFFF0 -> 16 ticks to overflow
    nc2k_states.ram_io[0x14] = 0x10; // TMAS=1, TMBS=0
    step(15);
    assert(nc2k_states.timer_ab.tma_val == 0xFFFF);
    assert((nc2k_states.ram_io[0x01] & 0x01) == 0);
    // Overflow with TMAIE=0 -> reloads, does NOT set TMAI
    step(1);
    assert(nc2k_states.timer_ab.tma_val == 0xFFF0);
    assert((nc2k_states.ram_io[0x01] & 0x01) == 0);
    // Enable TMAIE=1, overflow -> sets TMAI
    nc2k_states.inner_interrupt_control = 0x01;
    step(16);
    assert(nc2k_states.timer_ab.tma_val == 0xFFF0);
    assert((nc2k_states.ram_io[0x01] & 0x01) == 0x01);
    // Multi-overflow in single batch (16*3 + 5 = 53 ticks -> reload + 5 = 0xFFF5)
    nc2k_states.ram_io[0x01] = 0;
    step(53);
    assert(nc2k_states.timer_ab.tma_val == 0xFFF5);
    assert((nc2k_states.ram_io[0x01] & 0x01) == 0x01);

    // 2. Read-to-clear on TMAL/TMAH preserves reload and other byte
    assert(timer_ab_read_10_tmal() == 0xF5);
    assert(nc2k_states.timer_ab.tma_val == 0xFF00);
    assert(timer_ab_read_11_tmah() == 0xFF);
    assert(nc2k_states.timer_ab.tma_val == 0x0000);
    assert(nc2k_states.timer_ab.tma_reload == 0xFFF0);

    // 3. Timer A prescaler OSC/256 (TMAS=8): 128 base_cycles per tick on PC1000 (OSC=2*base)
    reset_all(true, false, false, false, 3686400);
    timer_ab_write_10_tmal(0x00);
    timer_ab_write_11_tmah(0x00);
    nc2k_states.ram_io[0x14] = 0x80; // TMAS=8 -> OSC/256 = 28800Hz
    step(127);
    assert(nc2k_states.timer_ab.tma_val == 0);
    step(1);
    assert(nc2k_states.timer_ab.tma_val == 1);
    // Changing TMBS in $14 does not disturb Timer A phase
    step(100);
    nc2k_states.ram_io[0x14] = 0x81;
    step(28);
    assert(nc2k_states.timer_ab.tma_val == 2);

    // 4. Timer A Counter mode: TMACT=1, TMAS3=0 -> 32768 / 2^(TMAS[2:0]-1) ticks in 1 second
    // Test on NC1020 (3,686,400 Hz -> 112.5 cycles/tick, non-integer period!) AND huge cycle offset
    reset_all(false, true, false, false, 3686400);
    g_cyc = 1000000000000000ULL; // 10^15 cycles -> verifies no 64-bit overflow in ticks_at_freq
    nc2k_states.ram_io[0x19] = 0x10; // TMACT=1
    for (uint8_t tmas = 1; tmas <= 7; tmas++) {
        timer_ab_write_10_tmal(0x00);
        timer_ab_write_11_tmah(0x00);
        nc2k_states.ram_io[0x14] = (uint8_t)(tmas << 4);
        for (int i = 0; i < 3600; i++) {
            step(1024); // 3600 * 1024 = 3686400 = exact 1.000000 sec
        }
        uint16_t expected = (uint16_t)(32768u >> (tmas - 1));
        assert(nc2k_states.timer_ab.tma_val == expected);
    }
    // Rapid mode switch back to Timer mode directly via ram_io (no write hook needed, zero corruption)
    timer_ab_write_10_tmal(0x00);
    timer_ab_write_11_tmah(0x00);
    nc2k_states.ram_io[0x19] = 0x00; // TMACT=0
    nc2k_states.ram_io[0x14] = 0x10; // OSC/2 (NC1020: 0.5 ticks/cycle)
    step(100);
    assert(nc2k_states.timer_ab.tma_val == 50);

    // TMACT=1, TMAS=0 (OFF) and TMAS=8 (P44 OFF): 0 ticks
    timer_ab_write_10_tmal(0x00);
    timer_ab_write_11_tmah(0x00);
    nc2k_states.ram_io[0x19] = 0x10;
    nc2k_states.ram_io[0x14] = 0x00;
    step(100000);
    assert(nc2k_states.timer_ab.tma_val == 0);
    nc2k_states.ram_io[0x14] = 0x80;
    step(100000);
    assert(nc2k_states.timer_ab.tma_val == 0);

    // 5. Timer B: 12-bit on PC1000 (SPDC1016)
    reset_all(true, false, false, false, 3686400);
    nc2k_states.inner_interrupt_control = 0x02; // TMBIE=1
    timer_ab_write_12_tmbl(0xF8);
    timer_ab_write_13_tmbh(0xFF); // upper 4 bits masked -> 0x0FF8
    assert(nc2k_states.timer_ab.tmb_val == 0x0FF8);
    assert(nc2k_states.timer_ab.tmb_reload == 0x0FF8);
    nc2k_states.ram_io[0x14] = 0x01; // TMBS=1 (OSC/2 = 1 tick per base_cycle)
    step(7);
    assert(nc2k_states.timer_ab.tmb_val == 0x0FFF);
    assert((nc2k_states.ram_io[0x01] & 0x02) == 0);
    step(1);
    assert(nc2k_states.timer_ab.tmb_val == 0x0FF8);
    assert((nc2k_states.ram_io[0x01] & 0x02) == 0x02);
    assert(timer_ab_read_13_tmbh() == 0x0F);
    assert(nc2k_states.timer_ab.tmb_val == 0x00F8);
    assert(timer_ab_read_12_tmbl() == 0xF8);
    assert(nc2k_states.timer_ab.tmb_val == 0x0000);

    // 6. Timer B: 16-bit on NC2000 (SPDC1024, OSC = 1 * 4.9152M = 4.9152MHz)
    reset_all(false, false, true, false, 4915200);
    nc2k_states.inner_interrupt_control = 0x02;
    timer_ab_write_12_tmbl(0x00);
    timer_ab_write_13_tmbh(0xFF); // full 16-bit 0xFF00 -> 256 ticks to overflow
    assert(nc2k_states.timer_ab.tmb_val == 0xFF00);
    nc2k_states.ram_io[0x14] = 0x01; // TMBS=1 (OSC/2 = 0.5 ticks per base_cycle)
    step(511);
    assert(nc2k_states.timer_ab.tmb_val == 0xFFFF);
    assert((nc2k_states.ram_io[0x01] & 0x02) == 0);
    step(1);
    assert(nc2k_states.timer_ab.tmb_val == 0xFF00);
    assert((nc2k_states.ram_io[0x01] & 0x02) == 0x02);

    printf("ALL TIMER A/B TESTS PASSED!\n");
    return 0;
}
