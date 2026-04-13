#pragma once
#include "comm.h"
#include <cstdint>

/*struct cpu_states_t {
	uint16_t reg_pc;
	uint8_t reg_a;
	uint8_t reg_ps;
	uint8_t reg_x;
	uint8_t reg_y;
	uint8_t reg_sp;
};*/

struct nc2k_states_t{
	//uint32_t version;
	////////////cpu_states_t cpu;

	uint8_t SAVE_STATE_BEGIN;
	uint8_t ram_io[0x40*2];
	uint8_t ram_b[0x2000];
	uint8_t ram_b2[0x2000];
	uint8_t ram[0x8000*2];
	uint8_t ext_ram[0x8000];
	
	// 3e/3f ext registers
	uint8_t ext_reg[256];

	uint8_t RESET_STATE_BEGIN;

	// 3a-3d registers
	uint8_t bk;

	uint8_t RHR,THR;
	uint8_t BSR;
	uint8_t CSTOP;
	uint8_t GPC;

	uint8_t LSR,LCR;
	uint8_t IRCR;
	uint8_t CSTART;
	uint8_t RESERVED;

	uint8_t MCR;
	uint8_t MSR;
	uint8_t TMR;
	uint8_t P05;

	uint8_t IVR; //only for UCE bit
	uint8_t FCR;
	uint8_t IER;
	// 3a-3d registers end

	//NekoDriverIO internal states
	unsigned short gThreadFlags;
	bool timer0run;
	bool timer1run_tmie;
	bool timer0waveoutstart;
	int prevtimer0value;
	bool rw0f_b4_DIR00;
	bool rw0f_b5_DIR01;
	bool rw0f_b6_DIR023;
	bool rw0f_b7_DIR047;
	bool rw0f_b3_SH;
	uint8_t rw0f_b02_ZB02;
	bool w04_b7_EPOL;
	uint8_t w04_b46_PTYPE;
	uint8_t w04_b03_TBC;
	uint8_t w15_port1_DIR107;
	uint8_t w08_port0_OL;
	uint8_t r08_port0_ID;
	uint8_t w09_port1_OL;
	uint8_t r09_port1_ID;
	uint8_t w0c_b67_TMODESL;
	uint8_t w0c_b45_TM0S;
	uint8_t w0c_b23_TM1S;
	uint8_t w0c_b345_TMS;
	int timer0ticks;
	int timer1ticks;
	uint8_t w01_int_enable;
	
	bool lcdoffshift0flag;
	unsigned short lcdbuffaddr;
	unsigned short lcdbuffaddrmask;
	unsigned char cpf;  
	unsigned char lcden; 
	//NekoDriverIO internal states end

	//io_new.cpp internal states
	int dspRetData;
	bool dspTrans;
	bool dspSleep;
	int tmaValue;
	int tmaReload;
	unsigned char inner_interrupt_control;
	unsigned int speed_scaledown;
	uint8_t cps;
	uint8_t lcdon;
	bool dsp_0xd0;
	int dsp_0x7001_0x7002;
	bool dsp_data_feeded_but_hasnt_fetched;
	unsigned char dsp_data_low;
	int patch_idx;
	unsigned char patch_table[256];
	//io_new.cpp internal states end
	
	//nor states
	uint8_t fp_step;
	uint8_t fp_type;
	//nor states end

	uint8_t RESET_STATE_END;

	//handypsp cpu states
	bool g_irq,g_nmi,g_stp,g_wai,g_wai_saved;
	int mA,mX,mY,mSP,mPC;
	int mOpcode,mOperand;
	int mN,mV,mB,mD,mI,mZ,mC;
	//handypsp cpu states end

	//c6502 cpu states
	int A,X,Y,P,SP,PC;
	bool irqPending,nmiPending,nmiRequest;
	int clk;
	unsigned int lineclk;
	long long total_cycles; //used internally in c6502 cpu only
	//c6502 cpu states end

	uint64_t cycles;
	uint64_t last_cycles;

	uint8_t SAVE_STATE_END;

/*
===================
below are all legacy fields, only used in old cpu_loop or io
===================
*/

	uint8_t clock_buff[80];
	uint8_t clock_flags;

	uint8_t jg_wav_data[0x20];
	uint8_t jg_wav_flags;
	uint8_t jg_wav_idx;
	bool jg_wav_playing;
    
	bool slept;
	bool should_wake_up;
	bool pending_wake_up;
	uint8_t wake_up_flags;

	bool timer0_toggle;

	uint64_t unknown_timer_cycles;
	uint64_t timer0_cycles;
	uint64_t timer1_cycles;
	uint64_t timebase_cycles;
	uint64_t nmi_cycles;
	uint8_t keypad_matrix[8];

	nc2k_states_t(){
		memset(this,0,sizeof(nc2k_states_t));
		reset();
	}
	void reset(){
		memset(ram_io,0,sizeof(ram_io));
		memset(ext_reg+8, 0, sizeof(ext_reg)-8);// clear non-rtc regs
		memset(&RESET_STATE_BEGIN,0,(size_t)(&RESET_STATE_END - &RESET_STATE_BEGIN));
		lcdbuffaddr=0x09C0;
		lcdbuffaddrmask=0x0FFF;
		speed_scaledown=1;
	}
};
