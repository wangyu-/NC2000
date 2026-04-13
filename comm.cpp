#include "comm.h"
#include <cstdint>
#include <sys/types.h>

/*
===================
global switch
===================
*/
bool nc1020mode = false;
bool nc2000mode = false;
bool nc3000mode = false;
bool pc1000mode = false;
bool nc1020tw_mode =false;

CpuVersion cpu_version = CPU_HANDYPSP;
CpuLoopVersion cpu_loop_version = CPU_RUN3;
IoVersion io_version = IO_V2;

NorFormat nor_read_format = NorFormat::PHYSICAL_ORDER;
NorFormat nor_write_format = NorFormat::PHYSICAL_ORDER;

bool enable_load_state=false;
bool reset_after_load_state=false;
bool save_flash_on_exit=false;
bool save_state_on_exit=false;

bool sync_on_resume = true;

bool pro_key= false;

int log_on_key_press = 0;

bool log_all_dsp_io = false;
/*
===================
debug related
===================
*/

string inject_code;
uint64_t tick=0;  //tick is mostly for debug

bool enable_dyn_debug=false;
int enable_dyn_debug_next_n=0;
bool enable_quit_after_debug_next_n=false;


bool enable_debug_nand=false;

bool enable_debug_switch=false;
bool enable_debug_pc=false;
bool enable_oops=false;
bool enable_inject=false;

bool wanna_inject=false;
bool injected=false;

bool enable_debug_beeper=false;
bool enable_debug_dsp=false;

bool enable_debug_timer=false;
bool enable_debug_cks = false;

int enable_key_debug_once=0;

int debug_level = 0;

bool enable_assert = false;

/*
===================
emulation parameter
===================
*/
uint32_t SLICE_INTERVAL= 1;  //unit ms
int power_save_interval=1200;
uint32_t cpu_batch=64;

bool enable_keepon = true;
bool enable_auto_time_sync= true;

double timer01_speed_fix=1.0;

bool enable_emulate_cks = false;

bool forced_erase_before_write = true;

bool fast_forward=false;

double speed_multiplier=1.0;
double rtc_speed=1.0;
double fast_forward_limit=0;
/*
===================
cycles related
===================
*/

double oc_factor=1.0;

//uint32_t static_multipler;
uint32_t CYCLES_SECOND;
uint32_t UNKNOWN_TIMER_FREQ;
uint32_t TIMER0_FREQ;
uint32_t TIMER1_FREQ;
uint32_t TIMEBASE_FREQ;
uint32_t CYCLES_UNKNOWN_TIMER;
uint32_t CYCLES_TIMER0;
uint32_t CYCLES_TIMER1;
uint32_t CYCLES_TIMEBASE;
uint32_t CYCLES_TIMER1_SPEED_UP;
uint32_t CYCLES_NMI;
uint32_t CYCLES_MS;
/*
===================
rom related
===================
*/
uint32_t num_nor_pages;
uint32_t num_nand_pages;
uint32_t num_rom_pages;
uint32_t ROM_SIZE;
uint32_t NOR_SIZE;

/*
===================
display related
===================
*/
int pixel_size=4;
int gap_size=1;
int lcd_scale=1;
int total_size;

bool enable_lcd_latency_effect = true;
uint32_t LCD_INNER_REFRESH_INTERVAL=8; //unit ms
uint32_t LCD_OUTER_REFRESH_INTERVAL=16;
string lcdstripe_suffix;

int lcd_effect_charge_a=1;
int lcd_effect_charge_b=6;
int lcd_effect_discharge_a=1;
int lcd_effect_discharge_b=8;

const double rgb_base=0.90;
double r_scale=rgb_base+0.02,g_scale=rgb_base+0.04,b_scale=rgb_base;
/*
===================
misc
===================
*/
bool shift_down =false;
bool ctrl_down =false;
int battery_level=11;
bool patch_nc1020tw_nor=false;

bool patch_table_experiment=false;

WqxRom nc2k_rom;

void init_parameters(){
    /*
    ===================
    cycles related
    ===================
    */
    //static_multipler=1; //tmp fix for speed and crash

    // cpu cycles per second (cpu freq).
    CYCLES_SECOND = 3686400*(pc1000mode) + 5120*1000*(nc1020mode||nc2000mode)+10240*1000*nc3000mode;
    CYCLES_SECOND *= oc_factor;
    CYCLES_MS = CYCLES_SECOND / 1000;
    printf("cycles per second is %d\n",CYCLES_SECOND);
    
    // below are not used in new cpu loop,
    // but they are kept for old cpu loop for compare
    UNKNOWN_TIMER_FREQ = 2;
    TIMER0_FREQ = 2; //not used now
    TIMER1_FREQ = 200;//not used now
    TIMEBASE_FREQ = 250;
    CYCLES_UNKNOWN_TIMER = CYCLES_SECOND / UNKNOWN_TIMER_FREQ;
    // cpu cycles per timer0 period (1/2 s).
    CYCLES_TIMER0 = CYCLES_SECOND / TIMER0_FREQ;
    // cpu cycles per timer1 period (1/256 s).
    CYCLES_TIMER1 = CYCLES_SECOND / TIMER1_FREQ;
    CYCLES_TIMEBASE = CYCLES_SECOND / TIMEBASE_FREQ;
    // speed up
    CYCLES_TIMER1_SPEED_UP = CYCLES_SECOND / TIMER1_FREQ / 20;
    // cpu cycles per ms (1/1000 s).
    CYCLES_NMI = CYCLES_SECOND / 2;


    /*
    ===================
    rom related
    ===================
    */
    num_nor_pages =0x10+uint32_t(nc1020mode&&nc1020tw_mode)*0x10+uint32_t(nc3000mode)*0x10;

    //this is the nand pages of 528byte each
    num_nand_pages = 0+ uint32_t(nc2000mode)*65536  + uint32_t(nc3000mode)*65536*2;

    //const uint32_t num_nor_pages =0x20;
    num_rom_pages =0x300;
    ROM_SIZE = 0x8000 * num_rom_pages;
    NOR_SIZE = 0x8000 * num_nor_pages;

    /*
    ===================
    display related
    ===================
    */
    total_size=pixel_size+gap_size;

}

void ProcessBinaryRev(uint8_t* dest, uint8_t* src, uint32_t size){
	uint32_t offset = 0;
    while (offset < size) {
        memcpy(dest + offset + 0x4000, src + offset, 0x4000);
        memcpy(dest + offset, src + offset + 0x4000, 0x4000);
        offset += 0x8000;
    }
}

void ProcessBinaryLinear(uint8_t* dest, uint8_t* src, uint32_t size){
	uint32_t offset = 0;
    while (offset < size) {
        memcpy(dest + offset , src + offset, 0x4000);
        memcpy(dest + offset + 0x4000, src + offset + 0x4000, 0x4000);
        offset += 0x8000;
    }
}


void read_file(string name,vector<char> &v){
    FILE *f = fopen(name.c_str(), "rb");
    if(f==0) {
        printf("open file %s fail!\n",name.c_str());
        exit(-1);
    }
    fseek(f, 0, SEEK_END);
    int fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  /* same as rewind(f); */
    v.resize(fsize);
    fread(&v[0], fsize, 1, f);
    fclose(f);
}

int read_file_noexit(string name,vector<char> &v){
    FILE *f = fopen(name.c_str(), "rb");
    if(f==0) {
        printf("open file %s fail!\n",name.c_str());
        return -1;
    }
    fseek(f, 0, SEEK_END);
    int fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  /* same as rewind(f); */
    v.resize(fsize);
    fread(&v[0], fsize, 1, f);
    fclose(f);
    return 0;
}
