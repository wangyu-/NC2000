#include "nc2000.h"
#include "comm.h"
#include "ram.h"
#include "state.h"
#include "cpu.h"
#include "mem.h"
#include "rom.h"
#include "nor.h"
#include "nand.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include "sound.h"
#include "compare/c6502.h"
#include "console.h"
#include "iv_uart.h"
#include "cmd.h"
#include "key_new.h"
#include <algorithm>
#include "cpu_loop.h"
#include "rtc.h"

extern WqxRom nc2k_rom;

nc2k_states_t nc2k_states;

void save_state(string file_name){
	if(file_name.empty()) file_name=nc2k_rom.statesPath;
	else file_name+=".state";
	FILE* file = fopen(file_name.c_str(), "wb");
	if (file == NULL) {
		printf("states file %s open failed, skip saving!\n", file_name.c_str());
		return;
	}
	fwrite(&nc2k_states.SAVE_STATE_BEGIN, 1, &nc2k_states.SAVE_STATE_END-&nc2k_states.SAVE_STATE_BEGIN, file);
	fflush(file);
	fclose(file);
	printf("state saved to file %s!!\n",file_name.c_str());
}

void delete_state(string file_name){
	if(file_name.empty()) file_name=nc2k_rom.statesPath;
	else file_name+=".state";
	if(remove(file_name.c_str())==0){
		printf("state file %s deleted!\n",file_name.c_str());
	}else{
		printf("state file %s not exist or no permission to delete!\n",file_name.c_str());
	}
}

void load_state(){
	FILE* file = fopen(nc2k_rom.statesPath.c_str(), "rb");
	if (file == NULL) {
		printf("states file %s open failed, skip loading!\n", nc2k_rom.statesPath.c_str());
		return;
	}
	int ret=fread(&nc2k_states.SAVE_STATE_BEGIN, 1, &nc2k_states.SAVE_STATE_END-&nc2k_states.SAVE_STATE_BEGIN, file);
	fclose(file);
	printf("loaded states from %s, ret=%d\n", nc2k_rom.statesPath.c_str(),ret);
	//super_switch();

	if(nc2k_states.target_cycles==0){
		//if target_cycles is not set, initialize it to the current cycles
		// this keeps save format backward compatible
		// this code can be dropped in future for simplicity
		printf("using current cycles as target_cycles!\n");
		nc2k_states.target_cycles = nc2k_states.cycles;
	}
}

void LoadNC2k(){
	nc2k_states.zero_init();
	
	init_keyitems();

	init_nor();
	if(pc1000mode||nc1020mode) {
		init_rom();
	}
	if(nc2000mode||nc3000mode) {
		read_nand0_file();
		read_nand_file();
	}

	init_mem();

	//reset_cpu_states();
	init_cpu_new();

	nc2k_cold_reset();

	if(enable_load_state){
		load_state();
		if(nc2000mode||nc3000mode){
			if(enable_auto_time_sync) sync_time_2000();
		}
		if(nc1020mode){
			if(enable_auto_time_sync) sync_time_1020();
		}
	}

	super_switch();

	if(enable_load_state&&reset_after_load_state){
		void set_warm_reset_flag();
		set_warm_reset_flag();
		nc2k_warm_reset();
	}
}

void SaveNC2kIfNeed(){
    if(save_flash_on_exit){
      save_flash("");
    }
    if(save_state_on_exit){
      save_state("");
    }
}

static unsigned short &lcdbuffaddr = nc2k_states.lcdbuffaddr;
static unsigned short &lcdbuffaddrmask = nc2k_states.lcdbuffaddrmask;
int get_lcd_segment_size(){
	uint8_t lcd_seg_conf = nc2k_states.ram_io[0x0d]>>3;
	if(lcd_seg_conf ==0x00) return 512;
	else return lcd_seg_conf*16;
}

int get_lcd_end_addr(){
	uint8_t lcdx = (nc2k_states.ram_io[0x0b] & 0x60) >> 5;
	uint32_t mask= (2<< (3-lcdx)) -1;
	uint32_t lcd_end= (mask<<10)|0x3ff;
	assert(lcd_end <= 0x3fff);
	//printf("lcdx=%d, mask=%x, lcd_end=%x\n", lcdx, mask, lcd_end);
	return lcd_end;
}

bool CopyLcdBuffer(uint8_t* buffer){
	//int lcd_addr = lcdbuffaddr&lcdbuffaddrmask;

	int lcd_segment_size = get_lcd_segment_size();
	if (lcd_segment_size != 160 && lcd_segment_size != 320) {
		RATE_LIMIT_LOG(3000,5,"lcd driver not properly initialized, lcd_segment_size=%d", lcd_segment_size);
		return false;
	}
	int buffer_size=0;
	if(lcd_segment_size ==160){
		buffer_size = 160 * (SCREEN_HEIGHT/8);
	}else{
		buffer_size = 320 * (SCREEN_HEIGHT/8);
	}

	int lcd_start = lcdbuffaddr&lcdbuffaddrmask;

	if (lcd_start <=0x300) {
		lcd_start = 0x300;
	}

	int lcd_end_open = get_lcd_end_addr()+1;

	if(lcd_start <= lcd_end_open - buffer_size){
		lcd_start = lcd_end_open - buffer_size;
	}

	int real_len = lcd_end_open - lcd_start;
	if (real_len < buffer_size) {
		memset(buffer, 0, buffer_size - real_len);
	}
	memcpy(buffer + buffer_size - real_len, ram_buff + lcd_start, real_len);
	return true;
}

void RunTimeSlice(uint32_t time_slice) {
	uint32_t new_cycles = time_slice * CYCLES_MS;

	if(!fast_forward) {
		new_cycles= new_cycles * speed_multiplier;
	}else if(fast_forward_limit==0){
		new_cycles= new_cycles*1;
	}else{
		new_cycles= new_cycles * fast_forward_limit;
	}

	nc2k_states.target_cycles += new_cycles;

	while (nc2k_states.cycles < nc2k_states.target_cycles && !reload_pending) {
		if(cpu_loop_version == CPU_RUN1){
			cpu_run();
		}else if (cpu_loop_version == CPU_RUN2){
			cpu_run2();
		}else if (cpu_loop_version == CPU_RUN3){
			cpu_run3();
		}else{
			assert(false);
		}
		post_cpu_run_sound_handling();
	}
}

void save_flash(string file){
	write_nand0_file(file);
	write_nand_file(file);
	SaveNor(file);
	printf("flash saved to file!!\n");
}

void nc2k_reset_comm(){
	uint8_t* ioReg=nc2k_states.ram_io;

	//value confirmed to be reset according to 1016 datasheet
	// (assume RESCPUB is warm reset and PRESZP (RESB) is cold reset)
	ioReg[0x00] = 0;                          // B0-B7 (bank switch)
	nc2k_states.inner_interrupt_control &= ~0x10; // $01 b4 NMIOFF
	ioReg[0x05] &= 0x1f;                      // CKS2-0 -> 000 (OSC/8)
	ioReg[0x0a] &= 0x70;                      // ROA (b7) and BBS0-3 (b0-3)
	ioReg[0x0d] &= 0xf8;                      // SAVE (b2) and VSL0-1 (b0-1)
	ioReg[0x0f] &= 0xf8;                      // ZB0-2 (b0-2)

#if false //below is according to spdc1024 datasheet, which doesn't work well
	//according to datasheet, warm reset will set both 0x2 and 0x3 to zero
	// but it will make on/off key trigger cold reset
	// so either on/off key doesn't really count as the "warm reset" on datasheet,
	// or datasheet is not same version as the hardware
	//so for simplicity we only set clk to default, ignoring other registers
	// it seems to be working well
	const int simple_warm_reset=true;
  if(simple_warm_reset){
	ram_io[0x05] &= 0x1f;
  } /*else {
    //0x00
    ioReg[0x00]=0;

    //0x01
    nc2k_states.inner_interrupt_control&=0xfc; //TMBIE TMAIE clear
    
    //0x02
    ////ioReg[0x02]=0;          //if reset both 0x02 and 0x03, on/off key will trigger cold reset
    
    //0x03
    ////ioReg[0x03]=0;

    //0x04
    nc2k_states.w04_b03_TBC &=0xf0;
    
    //0x05
    ioReg[0x05] &=0x1f;
    ioReg[0x05] &=0xf7;
    nc2k_states.lcdon=0;
    nc2k_states.speed_scaledown=1;

    //0x0a
    ioReg[0x0a] &=0xe0;
    
    //0x0b
    ioReg[0x0b]&=0xfd;
    nc2k_states.lcden=0;

    //0x0c
    ioReg[0x0c]&=0xfc;
    nc2k_states.w0c_b67_TMODESL =0;
    nc2k_states.w0c_b45_TM0S =0;
    nc2k_states.w0c_b23_TM1S = 0;
    nc2k_states.w0c_b345_TMS = 0;

    //0x0d
    ioReg[0x0d]&=0xf8;

    //0x14
    ioReg[0x14]=0;

    //0x19
    ioReg[0x19]=0xef; // should be &= ?

    //0x1a
    ioReg[0x1a]=0;

    //0x1b
    ioReg[0x1b]=0;

    //0x1c
    ioReg[0x1c]&=0xbf;

    //0x1e
    ioReg[0x1e]=0;
  }*/
#endif
}

void nc2k_warm_reset(){
    nc2k_reset_comm();
    super_switch();
    cpu->reset();//must be called after super_switch is done
}

void nc2k_cold_reset(){
    clear_cmds();

    nc2k_states.reset();
    nc2k_reset_comm();
    super_switch();
    cpu->reset();//must be called after super_switch is done
}
