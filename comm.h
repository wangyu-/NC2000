#pragma once

#include <string>
#include <stddef.h>
#include <stdint.h>
#include <cassert>
#include <cstdio>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <deque>
using std::string;
using std::deque;
using std::fstream;

#define IO_API


extern string inject_code;
extern uint64_t tick;

extern bool enable_dyn_debug;
extern bool enable_debug_nand;
extern bool enable_debug_switch;
extern bool enable_debug_pc;
extern bool enable_oops;
extern bool enable_inject;

extern bool wanna_inject;
extern bool injected;

/*
===================
global switch
===================
*/

const bool nc1020 = false;
const bool nc2000 = !nc1020;

/*
===================
cpu related
===================
*/
const uint16_t NMI_VEC = 0xFFFA;
const uint16_t RESET_VEC = 0xFFFC;
const uint16_t IRQ_VEC = 0xFFFE;

const uint32_t VERSION = 0x06;
/*
===================
display related
===================
*/
const uint32_t SCREEN_WIDTH=160;
const uint32_t SCREEN_HEIGHT=80;
const uint32_t LINE_SIZE=2;

const uint32_t FRAME_RATE=30;
const uint32_t FRAME_INTERVAL= (1000u/FRAME_RATE);


/*
===================
cycles related
===================
*/
// cpu cycles per second (cpu freq).
const uint32_t CYCLES_SECOND = 5120000*(nc2000?10:1); //multiple by 10, tmp solution to fix speed
const uint32_t TIMER0_FREQ = 2;
const uint32_t TIMER1_FREQ = 0x100;
// cpu cycles per timer0 period (1/2 s).
const uint32_t CYCLES_TIMER0 = CYCLES_SECOND / TIMER0_FREQ;
// cpu cycles per timer1 period (1/256 s).
const uint32_t CYCLES_TIMER1 = CYCLES_SECOND / TIMER1_FREQ;
// speed up
const uint32_t CYCLES_TIMER1_SPEED_UP = CYCLES_SECOND / TIMER1_FREQ / 20;
// cpu cycles per ms (1/1000 s).
const uint32_t CYCLES_MS = CYCLES_SECOND / 1000;


/*
===================
rom related
===================
*/
const uint32_t num_nor_pages =0x10+uint32_t(nc1020)*0x10;
//const uint32_t num_nor_pages =0x20;
const uint32_t num_rom_pages =0x300;
static const uint32_t ROM_SIZE = 0x8000 * num_rom_pages;
static const uint32_t NOR_SIZE = 0x8000 * num_nor_pages;


/*
===================
misc
===================
*/
extern bool enable_dyn_debug;

/*
===================
common functions
===================
*/
/**
 * ProcessBinary
 * encrypt or decrypt wqx's binary file. just flip every bank.
 */
void ProcessBinary(uint8_t* dest, uint8_t* src, uint32_t size);

/*
===================
common types
===================
*/
typedef uint8_t (IO_API *io_read_func_t)(uint8_t);
typedef void (IO_API *io_write_func_t)(uint8_t, uint8_t);
struct WqxRom {
    std::string romPath;
    std::string norFlashPath;
    std::string statesPath;
};
