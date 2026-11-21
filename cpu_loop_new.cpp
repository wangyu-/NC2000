#include "compare/c6502.h"
#include "comm.h"
#include "compare/ibus6502.h"
#include "cpu.h"
#include "mem.h"
#include "cmd.h"
#include "ram.h"
#include "state.h"
#include "misc/disassembler.h"
#include "compare/pc1000bus.h"
#include "io_new.h"
#include "iv_uart.h"
#include "sound.h"
#include "rtc.h"

#define qDebug(...)


extern nc2k_states_t nc2k_states;
static u64_t& cycles = nc2k_states.cycles;
static u64_t& last_cycles = nc2k_states.last_cycles;

static u64_t& p_cycles = nc2k_states.p_cycles;
static u64_t& last_p_cycles = nc2k_states.last_p_cycles;

static uint8_t * rtc_reg=nc2k_states.ext_reg;
static uint8_t * ext_reg=nc2k_states.ext_reg;
//static uint8_t& interr_flag = nc2k_states.interr_flag;

static bool& do_warm_reset=nc2k_states.do_warm_reset;

BusWrapper *dummy_bus;
BusPC1000 *bus_pc1000;
C6502 *c6502;

void init_cpu_new(){
	if(cpu){delete cpu; cpu=nullptr;}
	if(c6502){delete c6502; c6502=nullptr;}
	if(dummy_bus){delete dummy_bus; dummy_bus=nullptr;}
	if(bus_pc1000){delete bus_pc1000; bus_pc1000=nullptr;}

	if(io_version == IO_V1 || io_version == IO_V2){
		if(cpu_version== CPU_HANDYPSP) {
			cpu=new CPUInterface();
		} else if(cpu_version==CPU_EMUX) {
			dummy_bus=new BusWrapper();
			c6502 = new C6502(dummy_bus);
			cpu = new CPUInterface(c6502);
		} else {
			assert(false);
		}
	}else if(io_version == IO_EMUX){
		//emux io只能运行于pc1000mode + bus_pc1000
		assert(pc1000mode);
		assert(cpu_loop_version==CPU_RUN3);
		bus_pc1000=new BusPC1000();
		c6502=new C6502(bus_pc1000);
		cpu = new CPUInterface(c6502);
		bus_pc1000->cpu=c6502;
	}else{
		printf("unknown io version %d\n", io_version);
		assert(false);
	}
	
}

int trigger_every_x_cycle(int x){
	uint32_t trigger_cycles= x;
	return cycles/trigger_cycles - last_cycles/trigger_cycles;
}

int trigger_every_x_ms(int x){
	uint32_t trigger_cycles=  CYCLES_SECOND*x/1000;
	return cycles/trigger_cycles - last_cycles/trigger_cycles;
}

int trigger_x_times_per_s(int x){
	uint32_t trigger_cycles=CYCLES_SECOND/x;
	return cycles/trigger_cycles - last_cycles/trigger_cycles;
}

int trigger_every_x_p_cycle(int x){
	uint32_t trigger_cycles= x;
	return p_cycles/trigger_cycles - last_p_cycles/trigger_cycles;
}

bool is_clk_off(){
	return ram_io[0x05]>>5==7;
}
void set_clk_on(){
	uint8_t value=ram_io[0x05];
	value&= 0x1f;
	value|=0x3<<5;
	Store(0x05,value);
}
void warm_reset_if_clkoff(){
	if(is_clk_off()){//clk off
		do_warm_reset=1;
		printf("warm reset!!\n");
	}
}
void debug_pc(){
	if(debug_level>=9){
		unsigned char buf[10];
		buf[0]=Peek16(cpu->PC);
		buf[1]=Peek16(cpu->PC+1);
		buf[2]=Peek16(cpu->PC+2);
		buf[3]=0;
		if(buf[0]==0x00){
			printf("brk %02x %02x %02x\n",buf[0],buf[1],buf[2]);
			//Store(cpu->PC+2,0x10);
		}
	}
	if(enable_debug_pc||enable_dyn_debug||enable_dyn_debug_next_n){
		unsigned char buf[10];
		buf[0]=Peek16(cpu->PC);
		buf[1]=Peek16(cpu->PC+1);
		buf[2]=Peek16(cpu->PC+2);
		buf[3]=0;

		//printf("tick=%lld ",tick /*, reg_pc*/);
		//printf("%02x %02x %02x %02x; ",Peek16Debug(cpu->PC), Peek16Debug(cpu->PC+1),Peek16Debug(cpu->PC+2),Peek16Debug(cpu->PC+3));
		printf("%s",disassemble_next2(cpu->PC).c_str());
		printf("\n");
		if(enable_dyn_debug_next_n>0) {
			enable_dyn_debug_next_n--;
			if(enable_dyn_debug_next_n==0){
				if(enable_quit_after_debug_next_n) {
					printf("quit after debug next n\n");
					exit(-1);
				}
			}
		}
		//getchar();
	}
	if(debug_level>=1 || enable_assert_for_wqx_software){
		if(Peek16(cpu->PC)==0x00 && Peek16(cpu->PC+1)==0x00 && Peek16(cpu->PC+2)==0x00){
			if(debug_level>=1)printf("oops brk 0000!!!!!!!!!!!!!!!!!!!!!!\n");
			if(enable_assert_for_wqx_software) assert(false);
		}
	}

}

bool pc1000mode_normal(){
	return pc1000mode && io_version!= IO_EMUX;
}

bool pc1000mode_emux(){
	return pc1000mode && io_version== IO_EMUX;
}

void cpu_run3(){
	if(do_warm_reset){
		do_warm_reset=0;
		//prepare_soft_reset(); //shouldn't call here
		nc2k_warm_reset();
	}
	char *peeked_msg=peek_message();
	if(peeked_msg){
		bool need_wait=false;
		auto cmds=split_s(string(peeked_msg), " ");
		if(cmds.empty()){
			get_message();
			return;
		}
		string cmd=cmds[0];
		string translate_cmd_alias(string name);
		cmd=translate_cmd_alias(cmd);
		//printf("cmd is %s\n",cmd.c_str());
		if(cmd=="file_manager"||cmd=="put"||cmd=="get"||cmd=="create_folder"||cmd=="create_folder_hex"){
			need_wait=true;
			//printf("need wait!!\n");
		}
		if(!need_wait||(cpu->P()&4)==0)
		{
			if(cmd=="file_manager"||cmd=="put"||cmd=="get"){
				ram_io[0x05]=(ram_io[0x05]&0x1f)|(3<<5); //set cks to highest
				if(debug_level>=1) printf("set cks to highest\n");
			}
			string msg=get_message();
			if(!msg.empty()){
				handle_cmd(msg);
			}
		}
	}
	if(reload_pending){
		return;
	}

	sync_time_cold_start();
	//tick++;

	if(debug_level >=9){  //not for emulation, just trying to log some peridic debug info
		if(trigger_x_times_per_s(1)){
			int& patch_idx=nc2k_states.patch_idx;
			unsigned char *patch_table=nc2k_states.patch_table;
			printf("patch table= ");
			for(int i=0x10;i<=0x1f;i++){
				printf("%02x ",patch_table[i]);	
			}
			printf("\n");
		}
	}

	if(is_clk_off()){
		last_cycles=cycles;
		last_p_cycles=p_cycles;//this cannot be omitted
		cycles+=cpu_batch;
		//p cycles don't change at all
	}else{
		int speed_scaledown=1;
		if(enable_emulate_cks) speed_scaledown=cks_to_speed_scaledown(ram_io[0x05]>>5);
		int expect_cycles=cpu_batch/speed_scaledown;
		int delta=cpu->execute(expect_cycles);
		last_cycles=cycles;
		last_p_cycles=p_cycles;
		p_cycles+=delta;//todo: change more events to p_cycle based
		cycles+=delta*speed_scaledown;
	}


	if(pc1000mode){
		//magic number to fit pc1000emux's timer0,timer1 and timerA code
		//todo: better emulation
		if(int trigger_cnt=trigger_x_times_per_s(576*50)){
			for(int i=0;i<trigger_cnt;i++){
				if(pc1000mode_normal()) {
					//timerA/B are usually for record and play
					//looks like nobody use timerA on nc1020/nc2000
					if(setTimerA()){
						//note: in orignally pc1000emux, it doesn't set irq pending here
						cpu->set_irq_pending();
					}
				}
				if(pc1000mode_emux()) {
					bus_pc1000->setTimer();
					if (bus_pc1000->setTimer0()) {
						//timer0用于录放音，蜂鸣器音乐
						bus_pc1000->setIrqTimer0();
						//printf("irq1!\n");
						cpu->irq_now();
					}
					if (bus_pc1000->setTimer1()) {
						//timer1用于秒表的百分之一秒，每秒200次
						bus_pc1000->setIrqTimer1();
						cpu->irq_now();
						//printf("irq2!\n");
					}
				}
			}
		}
	}

	if(nc1020mode||nc2000mode||nc3000mode||(pc1000mode_normal())) {
		bool KeepTimer01( unsigned int cpuTick);
		long long scaled_last_p_cycles = last_p_cycles;
		long long scaled_p_cycles = p_cycles;
		if(timer01_speed_fix!=1){
			scaled_last_p_cycles *= timer01_speed_fix;
			scaled_p_cycles *= timer01_speed_fix;
		}
		int delta= scaled_p_cycles - scaled_last_p_cycles;
		if(KeepTimer01(delta)){ //note: during clk off timer01 is auto shut down because p_cycle doesn't move, no special handling needed
				if(enable_dyn_debug_next_n) printf("time to timer01!!!!\n");
				cpu->set_irq_pending();
		}
	}
	if(nc2000mode||nc3000mode||nc1020mode||pc1000mode_normal()){
		//timebase is trigged by address line of lcd, 
		//the trigger rule is complex, it depends on TBC cps and cpf.
		//for simplicty just use fixed value

		//timebase affect's game music speed

		//nc2000 and nc3000 use TBC=0c, pc1000 and nc1020 use TBC=0a
		if(nc1020mode||pc1000mode_normal()) {
			// for nc1020:
			// tbc=0x0a, so L8 triggers TBI interrupt, it happens every time L8 flips from 0 to 1
			// from 09c0 to 0x0fff, L8 flips 0->1 three times.
			// since cps=011, cp=p/4, cp=3686400/4=921600    (every bit triggers a cp) 
			// lp=cp/160=921600/160=5760     (a row contains 160 bits)
			// fp=5760/80=72   (agree with document value)
			// freq_tbi=72*3=216     (compared with physical wqx game music, sounds correct)
			if(trigger_every_x_cycle(CYCLES_SECOND/216)){  //if use trigger_every_x_p_cycle(), key is too laggy
				if (timeBaseEnable()) {
					setIrqTimeBase();
					cpu->set_irq_pending();
				}
			}
		}

		if(nc2000mode||nc3000mode) {
			// for nc2000 only:
			// tbc=0x0c, so L10 triggers TBI interrupt, it happens every time L10 flips from 0 to 1
			// from 0x1830 to 0x1fff, L10 flips 0->1 only 1 time.  but 1 sounds too slow, 2 sounds correct. (compared with physical wqx game music)
			// freq_tbi=fp*2=73.8*2=147.6
			if(trigger_every_x_cycle(CYCLES_SECOND*10/1476)){
				if (timeBaseEnable()) {
					setIrqTimeBase();
					cpu->set_irq_pending();
				}
			}
		}
	}

	if(pc1000mode_emux()){
		if(trigger_x_times_per_s(250)){
			if (bus_pc1000->timeBaseEnable()) {
				//timebase中断为4ms一次，主要用于键盘扫描
				bus_pc1000->setIrqTimeBase();
				cpu->irq_now();
			}
		}
	}

	if(nc1020mode||nc2000mode||nc3000mode){
		int n=256;
		if(rtc_speed!=1.0) {
			n=int(256*rtc_speed);
		}
		int trigger256=trigger_x_times_per_s(n);
		// in some extreme cases, rtc might trigger faster than cpu execution
		// e.g. when speed_scaledown=512 and CYCLES_SECOND is underclocked to a low value
		// but practically, trigger256 should be either 0 or 1
		// update: this is no longer a concern in new implementation with p_cycles, but kept for robustness
		for(int i=0;i<trigger256;i++)
		{
			rtc_reg[TR_ms]++;
			auto &trigger256_cnt= rtc_reg[TR_ms];
			if(trigger256_cnt==0){
				if(enable_keepon){
					//prevents from sleep
					//not exist in physical devide, but 
					if(nc1020mode) Store(1143, 0);//prevent sleep
					else if(nc2000mode||nc3000mode) Store(1025, 0);
				}
				//bump the 1/256 second
				bumpRTC();
			}
			
			if(trigger256_cnt%128==0){
				if(trigger256_cnt==0&& (RCR0&RCR0_ALARM) && chk_ar()){
					if(debug_level>=1) printf("chk_ar() return true!\n");
					put_iv(IV_ALARM);
				}
				if((RCR0&RCR0_2HZ) ){
					if(debug_level>=2){
						printf("2hz!!!!\n");
					}
					put_iv(IV_2HZ);
				}
			}

			if(trigger256_cnt%128==64){ //avoid nmi triggerd at same time as rtc irq
				if (nmiEnable()){
					if(nc1020mode||nc2000mode||nc3000mode){
						if(debug_level>=1) printf("nmi!\n");
					}
					cpu->set_nmi_pending();
				}
			}

		}//end of for(int i=0;i<trigger256;i++)

		uint32_t sample_hz=get_sample_hz();
		if(rtc_speed!=1.0) {
			sample_hz=int(sample_hz*rtc_speed);
		}
		if(sample_hz>0 && trigger_x_times_per_s(sample_hz)){
			if(debug_level>=2) printf("IV_SAMPLE triggered\n");
			put_iv(IV_SAMPLE);
		}

		uint8_t iv=peek_iv();
		if(iv!=IV_NONE){
			cpu->set_irq_pending();
			//if(iv==IV_ALARM) warm_reset_if_clkoff();
			if(nc2k_states.iv_set&(1<<IV_ALARM)){
				warm_reset_if_clkoff();
			}
		}
	}

	if(nc1020mode||nc2000mode){
		// without FP nc1020 physical dump won't work (include nc1020 and nc1020tw)
		// pc2000hd.doc says "Lcd Common data FP (72HZ)" , nc2000hd.doc says "P34: Lcd Common data FP (73.8HZ)"
		// todo: make this part of better lcd driver implementation
		int trigger_cycles= 0;
		if(nc1020mode) trigger_cycles= CYCLES_SECOND/72;
		else if(nc2000mode) trigger_cycles= CYCLES_SECOND*10/738;
		else assert(false);
		
		int FP_tigger=trigger_every_x_cycle(trigger_cycles);
		auto &r0c = nc2k_states.r0c_general_status;
		if(FP_tigger){
			r0c|=0x01;  //this value is latched, read will clear it
		}
	}

	if(pc1000mode_normal() && trigger_x_times_per_s(2)){
		if (nmiEnable()){
			cpu->set_nmi_pending();
		}
	}
	if(pc1000mode_emux() && trigger_x_times_per_s(2)){
		setTime1000emux();
		if (bus_pc1000->nmiEnable()){
			cpu->set_nmi_pending();
		}
	}

	dsp_move(trigger_x_times_per_s(100)*(long long)(DSP_AUDIO_HZ)/100);
}
