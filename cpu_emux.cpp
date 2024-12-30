#include "ansi/c6502.h"
#include "comm.h"
#include "mem.h"
#include "console.h"
#include "state.h"
#include "disassembler.h"
#include "bus.h"

#define qDebug(...)


extern unsigned short gThreadFlags;
extern nc1020_states_t nc1020_states;
static uint64_t& cycles = nc1020_states.cycles;
static uint64_t& last_cycles = nc1020_states.last_cycles;

struct BusPC1000 *bus;
C6502 *cpu;

void cpu_init_emux(){
	bus=new BusPC1000();
	cpu=new C6502(bus);
	bus->cpu=cpu;
	cpu->reset();
}

bool trigger_every_x_ms(int x){
	uint32_t target_cycles=  CYCLES_SECOND*x/1000;
	return (cycles/target_cycles > last_cycles/target_cycles);
}

bool trigger_x_times_per_s(int x){
	uint32_t target_cycles=CYCLES_SECOND/x;
	return (cycles/target_cycles > last_cycles/target_cycles);
}

void setTime() {
	const int ADDR_POWER_UP_FLAG = 0x435;
	const int ADDR_WATCH_DOG = 0x468;
	const int ADDR_IDLESEC = 0x471;
    const int ADDR_HOUR = 0x469;
    const int ADDR_YEAR = 0x46c;

    bus->write(ADDR_IDLESEC, 0); //设置idlesec为0，禁止自动关机
    int year = bus->read(ADDR_YEAR) + 1881;
    if (year == 2000) {
		/*SYSTEMTIME sys;
		GetLocalTime(&sys);
        bus->write(ADDR_YEAR, sys.wYear - 1881);
        bus->write(ADDR_YEAR + 1, sys.wMonth - 1);
        bus->write(ADDR_YEAR + 2, sys.wDay - 1);
        bus->write(ADDR_HOUR, sys.wHour);
        bus->write(ADDR_HOUR + 1, sys.wMinute);
        bus->write(ADDR_HOUR + 2, sys.wSecond * 2);*/
    }
}
void cpu_run_emux(){
	//assert(cycles==cpu->getTotalCycles()/12);

	string msg=get_message();
	if(!msg.empty()){
		handle_cmd(msg);
	}
	tick++;

	if(enable_debug_pc||enable_dyn_debug){
		uint8_t & Peek16Debug(uint16_t addr);
		unsigned char buf[10];
		buf[0]=Peek16Debug(cpu->PC);
		buf[1]=Peek16Debug(cpu->PC+1);
		buf[2]=Peek16Debug(cpu->PC+2);
		buf[3]=0;
		printf("tick=%lld ",tick /*, reg_pc*/);
		printf("%02x %02x %02x %02x; ",Peek16Debug(cpu->PC), Peek16Debug(cpu->PC+1),Peek16Debug(cpu->PC+2),Peek16Debug(cpu->PC+3));
		printf("bs=%02x roa_bbs=%02x ramb=%02x zp=%02x reg=%02x,%02x,%02x,%02x,%03o  pc=%s",ram_io[0x00], ram_io[0x0a], ram_io[0x0d], ram_io[0x0f],cpu->A,cpu->X,cpu->Y,cpu->SP,cpu->P,disassemble_next(buf,cpu->PC).c_str());
		printf("\n");

		//getchar();		
	}
	uint32_t CpuTicks=cpu->exec_one()/12;
	last_cycles=cycles;
	cycles+=CpuTicks;

	if(trigger_x_times_per_s(576*50)){
		//printf("trigger1!\n");
		bus->setTimer();
		if (bus->setTimer0()) {
            //timer0用于录放音，蜂鸣器音乐
            bus->setIrqTimer0();
			//printf("irq1!\n");
            cpu->IRQ();
        }
		if (bus->setTimer1()) {
            //timer1用于秒表的百分之一秒，每秒200次
            bus->setIrqTimer1();
            cpu->IRQ();
			//printf("irq2!\n");
        }
	}

	if(trigger_every_x_ms(4)){
		if(nc1020mode||nc2000mode||nc3000mode){
			//this makes lava games work
			nc1020_states.clock_buff[4] ++;
		}
		if (bus->timeBaseEnable()) {
            //timebase中断为4ms一次，主要用于键盘扫描
            bus->setIrqTimeBase();
            cpu->IRQ();
        }
	}

	if(trigger_x_times_per_s(1)){
		//experiment
		if(nc2000mode||nc3000mode){
			////void AdjustTime();
			////AdjustTime();
		}
	}
	if(trigger_x_times_per_s(2)){
		if(nc1020mode){
			//nc2000 crash if enabled
			void AdjustTime();
			bool IsCountDown(void);
			static bool& timer0_toggle = nc1020_states.timer0_toggle;
			timer0_toggle = !timer0_toggle;
			if (!timer0_toggle) {
				AdjustTime();
			}
			if (!IsCountDown() || timer0_toggle) {
				ram_io[0x3D] = 0;
			} else {
				ram_io[0x3D] = 0x20;
				nc1020_states.clock_flags &= 0xFD;
			}
		}
		if(nc2000mode||nc3000mode){
			Store(1025, 0); //set idle time to zero, prevent sleep
		}
		if(pc1000mode){
			//set system time & prevent sleep, not important
			setTime();
		}
		if(pc1000mode||nc2000mode){
			if (bus->nmiEnable()){
				cpu->NMI();
				//printf("nmi!\n");
			}
		}
	}

}
