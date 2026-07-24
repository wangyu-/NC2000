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
#include <vector>
#include <mutex>
using std::string;
using std::deque;
using std::fstream;
using std::vector;

#define IO_API

typedef unsigned long long u64_t;  // this works on most platform,avoid using the PRId64
typedef long long i64_t;
/*
typedef unsigned int u32_t;
typedef int i32_t;
typedef unsigned short u16_t;
typedef short i16_t;
*/
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
    std::string nandFlashPath;
    std::string nand0Path;
    std::string statesPath;
};

enum NorFormat{
    INVALID=0,
    PHYSICAL_ORDER=1,
    WQX2KUTIL=2
};

enum CpuVersion {
    CPU_HANDYPSP = 1,
    CPU_EMUX = 2,
};

enum CpuLoopVersion {
    CPU_RUN1 = 1, // cpu_run,    only IO_V1
    CPU_RUN2 = 2, // cpu_run2,   both IO_V1 and IO_V2
    CPU_RUN3 = 3, // cpu_run3,   only IO_V2
};

enum IoVersion {
    IO_V1 = 1,
    IO_V2 = 2,
    IO_EMUX = 3
};

/*
===================
global switch
===================
*/

extern bool nc1020mode;
extern bool nc2000mode;
extern bool nc3000mode;
extern bool pc1000mode;
extern bool nc1020tw_mode;

extern CpuVersion cpu_version;
extern CpuLoopVersion cpu_loop_version;
extern IoVersion io_version;

const bool use_legacy_key_io = false;

extern NorFormat nor_read_format;
extern NorFormat nor_write_format;

extern bool enable_load_state;
extern bool reset_after_load_state;
extern bool save_flash_on_exit;
extern bool save_state_on_exit;

extern bool sync_on_resume;

extern bool enable_emulate_cks;

extern int log_on_key_press;

extern bool log_all_dsp_io;

/*
===================
debug related
===================
*/

extern string inject_code;
extern u64_t tick;

extern bool enable_dyn_debug;
extern int enable_dyn_debug_next_n;
extern bool enable_quit_after_debug_next_n;

extern bool enable_debug_nand;
extern bool enable_debug_switch;
extern bool enable_debug_pc;
extern bool enable_oops;
extern bool enable_inject;

extern bool wanna_inject;
extern bool injected;


const bool enabled_dsp=true;
const bool enable_beeper=true;

extern bool enable_debug_beeper;
extern bool enable_debug_dsp;

extern bool enable_debug_timer;

extern bool enable_debug_cks;

extern int enable_key_debug_once;

extern bool enable_assert;
/*
===================
cpu related
===================
*/
const uint16_t NMI_VEC = 0xFFFA;
const uint16_t RESET_VEC = 0xFFFC;
const uint16_t IRQ_VEC = 0xFFFE;

/*
===================
emulation parameter
===================
*/
extern uint32_t SLICE_INTERVAL;
extern uint32_t power_save_interval;
extern uint32_t cpu_batch;
extern bool enable_keepon;
extern bool enable_auto_time_sync;

extern double timer01_speed_fix;

extern bool forced_erase_before_write;

extern bool fast_forward;
extern double fast_forward_limit;

extern int debug_level;

extern double rtc_speed;

extern double speed_multiplier;
/*
===================
display related
===================
*/
const int32_t SCREEN_WIDTH=160;
const int32_t SCREEN_HEIGHT=80;
const int32_t LEFT_GAP=21;
const int32_t RIGHT_GAP=7;

const int32_t LEFT_GAP_EXTRA=0; //unit pixel
const int32_t RIGHT_GAP_EXTRA=0;

extern int pixel_size;
extern int gap_size;
extern int lcd_scale;
extern int total_size;

extern bool enable_lcd_latency_effect;

extern uint32_t LCD_INNER_REFRESH_INTERVAL;
extern uint32_t LCD_OUTER_REFRESH_INTERVAL;

extern string lcdstripe_suffix;

extern bool pro_key;

extern int lcd_effect_charge_a;
extern int lcd_effect_charge_b;
extern int lcd_effect_discharge_a;
extern int lcd_effect_discharge_b;

extern double r_scale,g_scale,b_scale;

//const uint32_t FRAME_RATE=40;   //how many frames in a second
//const uint32_t FRAME_FACTOR=SLICE_RATE/FRAME_RATE;

/*
===================
cycles related
===================
*/

extern double oc_factor;

//extern uint32_t static_multipler;
extern uint32_t CYCLES_SECOND;
extern uint32_t UNKNOWN_TIMER_FREQ;
extern uint32_t TIMER0_FREQ;
extern uint32_t TIMER1_FREQ;
extern uint32_t TIMEBASE_FREQ;
extern uint32_t CYCLES_UNKNOWN_TIMER;
extern uint32_t CYCLES_TIMER0;
extern uint32_t CYCLES_TIMER1;
extern uint32_t CYCLES_TIMEBASE;
extern uint32_t CYCLES_TIMER1_SPEED_UP;
extern uint32_t CYCLES_NMI;
extern uint32_t CYCLES_MS;

const uint32_t DSP_AUDIO_HZ = 8000;
const uint32_t BEEPER_AUDIO_HZ = 44100;

/*
===================
rom related
===================
*/
extern uint32_t num_nor_pages;
extern uint32_t num_nand_pages;
extern uint32_t num_rom_pages;
extern uint32_t ROM_SIZE;
extern uint32_t NOR_SIZE;

/*
===================
misc
===================
*/
const int int_inf=10*10000*10000;
extern bool shift_down;
extern bool ctrl_down;
extern int battery_level;
extern bool patch_nc1020tw_nor;

extern bool patch_table_experiment;
/*
===================
common functions
===================
*/

void ProcessBinaryRev(uint8_t* dest, uint8_t* src, uint32_t size);
void ProcessBinaryLinear(uint8_t* dest, uint8_t* src, uint32_t size);

//use vector char since string cannot store \0 well on mingw
void read_file(string name,vector<char> &v);
int read_file_noexit(string name,vector<char> &v);


void rom_switcher();
void init_parameters();

inline vector<string> split_s(const string &str, const string &sp) {
    vector<string> res;
    size_t i = 0, pos;
    for (;; i = pos + sp.length()) {
        pos = str.find(sp, i);
        if (pos == string::npos) {
			string s=str.substr(i, pos);
            if(!s.empty()) res.push_back(s);
            break;
        } else {
			string s=str.substr(i, pos - i);
            if(!s.empty()) res.push_back(s);
        }
    }
    return res;
}

inline string get_str_of_mode(){
    if(nc1020mode) return "NC1020";
    if(nc2000mode) return "NC2000";
    if(nc3000mode) return "NC3000";
    if(pc1000mode) return "PC1000";
    return "";
}

inline string get_title(){
    string ret= get_str_of_mode();
    if(pro_key) ret+=", pro_key";
    if(fast_forward) ret+=", fast_forward";
    return ret;
}

inline bool fileExists(const std::string& name) {
    FILE *file;
    if ((file = fopen(name.c_str(), "r")))
    {
        fclose(file);
        return 1;
    }
    return 0;
}
