// Build & run from repo root:
//   g++ -std=c++11 -DHANDYPSP -I. $(sdl2-config --cflags) test/test_timer01.cpp NekoDriverIO.cpp -o /tmp/test_timer01 && /tmp/test_timer01

#include <cassert>
#include <cstdio>
#include <cstdint>
#include "../comm.h"
#include "../state.h"
#include "../NekoDriverIO.h"

// Minimal stubs required by NekoDriverIO.cpp
nc2k_states_t nc2k_states;
uint8_t *ram_io = nc2k_states.ram_io;
bool pc1000mode = false, nc1020mode = true, nc2000mode = false, nc3000mode = false;
int debug_level = 0;
bool enable_debug_timer = false;
int enable_key_debug_once = 0;
void super_switch() {}
void beeper_on_io_write(int) {}

static uint64_t g_p = 0;

static void reset_all() {
    nc2k_states.zero_init();
    g_p = 0;
}

static bool step(uint64_t delta) {
    uint64_t next = g_p + delta;
    bool irq = KeepTimer01_new(g_p, next);
    g_p = next;
    return irq;
}

int main() {
    // 1. Mode 0: 8-bit auto-reload from $03, gated by timer1run_tmie ($07 start / $06 stop)
    reset_all();
    Write0CTimer01Control(0x0C, 0x00); // mode=0, TMS=0 => P/2
    ram_io[0x02] = 0xF0; // initial T0
    ram_io[0x03] = 0xF0; // reload = 240 => period = 16 ticks = 32 p_cycles
    assert(Read05StartTimer0(0x05) == 0xF0);
    // Overflow with TMIE=0 ($06 stopped): reloads, does NOT set TM0I
    assert(!step(30));
    assert(ram_io[0x02] == 0xFF);
    assert(!step(2));
    assert(ram_io[0x02] == 0xF0);
    assert((ram_io[0x01] & 0x10) == 0);
    // Enable TMIE ($07): overflow sets TM0I ($01 b4)
    assert(Read07StartTimer1(0x07) == 0xF0);
    assert(step(32));
    assert(ram_io[0x02] == 0xF0);
    assert((ram_io[0x01] & 0x10) == 0x10);
    // Multi-overflow in one step: 3*16 + 5 = 53 ticks = 106 p_cycles -> reload + 5 = 0xF5
    ram_io[0x01] = 0;
    assert(step(106));
    assert(ram_io[0x02] == 0xF5);
    assert((ram_io[0x01] & 0x10) == 0x10);

    // 2. Hot prescaler switch WITHOUT stopping (where stateful KeepTimer01 dumped +127 ticks)
    // Running at P/256 (TMS=7) for 255 p_cycles -> switch to P/2 (TMS=0) -> step 2 p_cycles
    reset_all();
    Write0CTimer01Control(0x0C, 0x38); // mode=0, TMS=7 => P/256
    ram_io[0x02] = 0x00;
    ram_io[0x03] = 0x00;
    Read05StartTimer0(0x05);
    step(255);
    assert(ram_io[0x02] == 0x00); // 0 ticks yet
    Write0CTimer01Control(0x0C, 0x00); // live switch to P/2
    step(2);
    assert(ram_io[0x02] == 1); // only 1 tick in (255, 257] at P/2

    // 3. Mode 1: independent 8-bit T0 (P/2, P/8, P/32, P/128) & T1 (P/4, P/16, P/64, P/256)
    reset_all();
    // mode=1 (0x40), TM0S=1 (P/8 => 0x10), TM1S=2 (P/64 => 0x08) -> 0x58
    Write0CTimer01Control(0x0C, 0x58);
    ram_io[0x02] = 0xFE;
    ram_io[0x03] = 0xFF;
    Read05StartTimer0(0x05);
    Read07StartTimer1(0x07);
    // 16 p_cycles => T0 gets 2 ticks (0xFE -> 0x00, TM0I!), T1 gets 0 ticks
    assert(step(16));
    assert(ram_io[0x02] == 0x00);
    assert(ram_io[0x03] == 0xFF);
    assert((ram_io[0x01] & 0x30) == 0x10); // TM0I only
    ram_io[0x01] = 0;
    // 48 more p_cycles (total 64) => T0 gets 6 ticks (0x06), T1 gets 1 tick (0xFF -> 0x00, TM1I!)
    assert(step(48));
    assert(ram_io[0x02] == 0x06);
    assert(ram_io[0x03] == 0x00);
    assert((ram_io[0x01] & 0x30) == 0x20); // TM1I only
    // Stop T0 ($04), T1 keeps running
    assert(Read04StopTimer0(0x04) == 0x06);
    ram_io[0x01] = 0;
    step(64);
    assert(ram_io[0x02] == 0x06);
    assert(ram_io[0x03] == 0x01);

    // 4. Mode 2: 16-bit non-reload (t0=lo, t1=hi), no TM0I, overflow sets TM1I
    reset_all();
    Write0CTimer01Control(0x0C, 0x80); // mode=2, TMS=0 => P/2
    ram_io[0x02] = 0xFE;
    ram_io[0x03] = 0xFF;
    Read05StartTimer0(0x05);
    assert(!step(2));
    assert(ram_io[0x02] == 0xFF && ram_io[0x03] == 0xFF);
    assert((ram_io[0x01] & 0x30) == 0);
    assert(step(2));
    assert(ram_io[0x02] == 0x00 && ram_io[0x03] == 0x00);
    assert((ram_io[0x01] & 0x30) == 0x20);

    // 5. Mode 3: 8-bit T0 cascades overflow into T1 (only when timer1run_tmie is true)
    reset_all();
    Write0CTimer01Control(0x0C, 0xC0); // mode=3, TMS=0 => P/2
    ram_io[0x02] = 0xFF;
    ram_io[0x03] = 0xFF;
    Read05StartTimer0(0x05);
    // timer1run_tmie=false: T0 overflows -> TM0I set, T1 NOT incremented
    assert(step(2));
    assert(ram_io[0x02] == 0x00);
    assert(ram_io[0x03] == 0xFF);
    assert((ram_io[0x01] & 0x30) == 0x10);
    // Enable T1 ($07), overflow T0 -> both TM0I and TM1I set
    ram_io[0x01] = 0;
    ram_io[0x02] = 0xFF;
    Read07StartTimer1(0x07);
    assert(step(2));
    assert(ram_io[0x02] == 0x00);
    assert(ram_io[0x03] == 0x00);
    assert((ram_io[0x01] & 0x30) == 0x30);

    // 6. Large single-step increment (> 65536 ticks) does not suffer 16-bit truncation
    reset_all();
    Write0CTimer01Control(0x0C, 0x80); // mode=2 (16-bit), TMS=0 => P/2
    ram_io[0x02] = 0x00;
    ram_io[0x03] = 0x00;
    Read05StartTimer0(0x05);
    // 70000 ticks = 0x11170 -> t0=0x70, t1=0x11, TM1I=1
    assert(step(70000ULL * 2));
    assert(ram_io[0x02] == 0x70);
    assert(ram_io[0x03] == 0x11);
    assert((ram_io[0x01] & 0x30) == 0x20);

    printf("ALL TIMER 0/1 NEW TESTS PASSED!\n");
    return 0;
}
