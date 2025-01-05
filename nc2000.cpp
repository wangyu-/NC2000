#include "nc2000.h"
#include "comm.h"
#include "disassembler.h"
#include "state.h"
#include "cpu.h"
#include "mem.h"
#include "io.h"
#include "rom.h"
#include "nor.h"
#include "nand.h"
#include <SDL2/SDL.h>
#include <SDL_timer.h>
#include <cassert>
#include <cstdio>
#include <deque>
#include "sound.h"
#include "ansi/c6502.h"
extern WqxRom nc1020_rom;

nc1020_states_t nc1020_states;
C6502 *cpu;

//static uint32_t& version = nc1020_states.version;

static bool& slept = nc1020_states.slept;
static bool& should_wake_up = nc1020_states.should_wake_up;

static uint8_t* keypad_matrix = nc1020_states.keypad_matrix;
//static uint32_t& lcd_addr = nc1020_states.lcd_addr;

static bool& wake_up_pending = nc1020_states.pending_wake_up;
static uint8_t& wake_up_key = nc1020_states.wake_up_flags;

void ResetStates(){
	//version = VERSION;
	memset(&nc1020_states,0,sizeof(nc1020_states_t));
	init_mem();
	reset_cpu_states();
	cpu->reset();
}

/*
void Reset() {
	init_nor();
	ResetStates();
}*/

void LoadStates(){
	FILE* file = fopen(nc1020_rom.statesPath.c_str(), "rb");
	if (file == NULL) {
		return;
	}
	fread(&nc1020_states, 1, sizeof(nc1020_states), file);
	fclose(file);
	/*
	if (version != VERSION) {
		return;
	}*/
	super_switch();
}

void SaveStates(){
	FILE* file = fopen(nc1020_rom.statesPath.c_str(), "wb");
	fwrite(&nc1020_states, 1, sizeof(nc1020_states), file);
	fflush(file);
	fclose(file);
}

void LoadNC1020(){
	init_io();
	cpu_init_emux();
	if(use_emux_cpu&&!use_emux_bus){
		void init_cpu2();
		init_cpu2();
	}

	rom_switcher();
	init_nor();
	if(pc1000mode||nc1020mode) {
		init_rom();
	}
	if(nc2000mode||nc3000mode) {
		read_nand_file();
	}
	ResetStates();

	if(nc2000mode||nc3000mode){
		//nc3000c-lee has it but seems like no need?
		//ram_io[0x18]=0x20;
	}
	//LoadStates();
}
/*
void SaveNC1020(){
	SaveNor();
	//SaveStates();
}*/

void SetKey(uint8_t key_id, bool down_or_up){
	uint8_t row = key_id % 8;
	uint8_t col = key_id / 8;
	uint8_t bits = 1 << col;
	if (key_id == 0x0F) {
		bits = 0xFE;
	}
	extern uint8_t* ram_io;
	if (down_or_up) {
		keypad_matrix[row] |= bits;
		//hack for now
		if(key_id==0x25){
			ram_io[0x8]=0x55;
		}else if (key_id==0x35){
			ram_io[0x8]=0x45;
		}
	} else {
		//hack for now
		ram_io[0x8]=0;
		keypad_matrix[row] &= ~bits;
	}

	if (down_or_up) {

		if (slept) {
			if (key_id >= 0x08 && key_id <= 0x0F && key_id != 0x0E) {
				switch (key_id) {
				case 0x08: wake_up_key = 0x00; break;
				case 0x09: wake_up_key = 0x0A; break;
				case 0x0A: wake_up_key = 0x08; break;
				case 0x0B: wake_up_key = 0x06; break;
				case 0x0C: wake_up_key = 0x04; break;
				case 0x0D: wake_up_key = 0x02; break;
				case 0x0E: wake_up_key = 0x0C; break;
				case 0x0F: wake_up_key = 0x00; break;
				}
				should_wake_up = true;
				wake_up_pending = true;
				slept = false;
			}
		} else {
			if (key_id == 0x0F) {
				slept = true;
			}
		}
	}
}
bool is_grey_mode(){
    extern unsigned short lcdbuffaddr;
    extern unsigned short lcdbuffaddrmask;
	unsigned short lcd_addr = lcdbuffaddr&lcdbuffaddrmask;
	//printf("%x  ",lcd_addr);
	//fflush(stdout);
	if(nc2000mode||nc3000mode)
		return lcd_addr==0x1380;
	return false;
}
bool CopyLcdBuffer(uint8_t* buffer){
    extern unsigned short lcdbuffaddr;
    extern unsigned short lcdbuffaddrmask;
    unsigned short lcd_addr = lcdbuffaddr&lcdbuffaddrmask;
	if (lcd_addr == 0) return false;
	memcpy(buffer, ram_buff + lcd_addr, 1600);

	if(nc2000mode||nc3000mode){
		if(!is_grey_mode()){
			memcpy(buffer, ram_buff + 0x19c0, 1600 );
		}else{
			memcpy(buffer, ram_buff + 0x19c0 -1600, 1600 *2);
		}

	}
	return true;
}


void RunTimeSlice(uint32_t time_slice, bool speed_up) {
	uint32_t new_cycles = time_slice * CYCLES_MS;

	new_cycles= new_cycles * speed_multiplier;

	uint64_t target_cycles=nc1020_states.cycles +new_cycles;

	//auto old=sound_stream.size();
	//printf("<%u,%u, %lld>",cycles,end_cycles,SDL_GetTicks64());
	while (nc1020_states.cycles < target_cycles) {
		if(use_emux_cpu){
			if(use_emux_bus){
				cpu_run_emux();
			}else{
				cpu_run2();
			}
		}else{
			cpu_run();
		}
	}

	post_cpu_run_sound_handling();

	//nc1020_states.previous_cycles+=end_cycles;
	//nc1020_states.cycles -= end_cycles;
	//nc1020_states.timer0_cycles -= end_cycles;
	//nc1020_states.timer1_cycles -= end_cycles;


}
