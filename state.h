#pragma once
#include "comm.h"
#include <cstdint>


//a POD vector that can be safely reset as zeros
template<size_t N>
struct PODVec {
	uint32_t len;
	uint8_t buf[N];

	size_t size() const { return len; }
	bool empty() const { return len == 0; }
	void clear() { len = 0; }
	uint8_t* data() { return buf; }
	uint8_t& operator[](size_t i) { return buf[i]; }
	void push_back(uint8_t val) {
		assert(len < N);
		buf[len++] = val;
	}
	void pop_back() {
		assert(len >0);
		len--;
	}
};

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
	unsigned int speed_scaledown; //not used any more
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

	u64_t cycles;
	u64_t last_cycles;
	u64_t target_cycles;

	uint8_t w1e_port6_OL;
	uint8_t r1e_port6_ID;

	uint8_t r0c_general_status;

	uint32_t iv_set;

	struct nand_state_t{
		uint32_t nand_pointer_latch;
		uint32_t nand_cmd_latch;
		uint32_t nand_read_offset;
		PODVec<MAX_NAND_ADDR_SIZE> nand_addr;
		PODVec<MAX_NAND_DATA_SIZE> nand_data;
	} nand_state;

	//append new variable to the end if possible, so that the save state can maintain backward compatibility

	uint8_t SAVE_STATE_END; // =========END of SAVE STATE=========

	bool time_adjusted;
	bool do_warm_reset;

	bool nc1020_time_adjusted_phase2; //nc1020 only
	u64_t nc1020_time_adjusted_cycle;

	bool woke_by_interrupt;//only for debug
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

	u64_t unknown_timer_cycles;
	u64_t timer0_cycles;
	u64_t timer1_cycles;
	u64_t timebase_cycles;
	u64_t nmi_cycles;
	uint8_t keypad_matrix[8];

	int gDeadlockCounter; //only used in leacy cpu loop

	nc2k_states_t(){
		zero_init();
	}
	void zero_init(){
		memset(this,0,sizeof(nc2k_states_t));
	}
	void reset(){ //called as part of cold_reset
		memset(ram_io,0,sizeof(ram_io));
		memset(ext_reg+8, 0, sizeof(ext_reg)-8);// clear non-rtc regs
		memset(&RESET_STATE_BEGIN,0,(size_t)(&RESET_STATE_END - &RESET_STATE_BEGIN));

		if(nc3000mode){
			//without this reset doesn't work correctly sometimes
			//not sure if this is real hardware behavior on 3k
			memset(ram,0,sizeof(ram));
		}

		//=================================================================
		//a few fields that missed by RESET_STATE_BEGIN and RESET_STATE_END
		//=================================================================
		iv_set=0; //if this is put into warm_reset, alarm wakeup will not work correctly
		memset(&nand_state,0,sizeof(nand_state));
		w1e_port6_OL=0;
		r1e_port6_ID=0;
		r0c_general_status=0;

		//================================================================
		//a few fields that is in RESET_STATE_BEGIN but not zero initalize
		//================================================================

		//from wayback, not not really needed
		//lcdbuffaddr=0x09C0;
		//lcdbuffaddrmask=0x0FFF;

		if(nc1020mode){
			ram_io[0x0b]=0x01;//if without it on nc1020, on/off need to be press twice to power off
		}

		if(nc2000mode||nc3000mode){
			//nc3000c-lee has it but seems like no need?
			//ram_io[0x18]=0x20;
		}
	}
};
