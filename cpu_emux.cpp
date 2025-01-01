#include "ansi/c6502.h"
#include "comm.h"
#include "mem.h"
#include "console.h"
#include "ram.h"
#include "state.h"
#include "disassembler.h"
#include "bus.h"

#define qDebug(...)


extern unsigned short gThreadFlags;
extern nc1020_states_t nc1020_states;
static uint64_t& cycles = nc1020_states.cycles;
static uint64_t& last_cycles = nc1020_states.last_cycles;
static uint8_t * rtc_reg=nc1020_states.rtc_reg;
static uint8_t& interr_flag = nc1020_states.interr_flag;
struct BusPC1000 *bus;
extern C6502* cpu;

void cpu_init_emux(){
	//assert(use_emux_cpu);
	// assert(use_emux_bus);
	bus=new BusPC1000();
	if(use_emux_cpu){
		cpu=new C6502(bus);
		bus->cpu=cpu;
		cpu->reset();
	}
}

bool trigger_every_x_ms(int x){
	uint32_t target_cycles=  CYCLES_SECOND*x/1000;
	return (cycles/target_cycles > last_cycles/target_cycles);
}

bool trigger_x_times_per_s(int x){
	uint32_t target_cycles=CYCLES_SECOND/x;
	return (cycles/target_cycles > last_cycles/target_cycles);
}

void setTime1000() {
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

#define	TR_s		0x0
#define	TR_m		0x01
#define	TR_h		0x02
#define	TR_d		0x03
#define	TR_ms		0x04
#define	AR_s		0x05
#define	AR_m		0x06
#define	AR_h		0x07
#define	RTC_CTRL	0x0a
#define	INT_CLEAR	0x0b
// nc3000lee1.1 version, only for compare
unsigned char chk_ar0()
{
	unsigned char alm=0;
	if(!(rtc_reg[RTC_CTRL]&0x02)||!(interr_flag&0x02))
		return alm;
	if((rtc_reg[AR_h]&0x80))
		{
		alm=1;
		if((rtc_reg[AR_h]&0x1f)!=(rtc_reg[TR_h]&0x1f))
			return	0;
		}
	if((rtc_reg[AR_m]&0x80))
		{
		alm=1;
		if((rtc_reg[AR_m]&0x3f)!=(rtc_reg[TR_m]&0x3f))
			return	0;
		}
	if((rtc_reg[AR_s]&0x80))
		{
		alm=1;
		if((rtc_reg[AR_s]&0x3f)!=(rtc_reg[TR_s]&0x3f))
			return	0;
		}
	return	alm;
}

bool chk_ar(){
  uint8_t bVar1;
  uint8_t uVar2;
  
  if ((rtc_reg[10] >> 1 & 1) == 0) {
    return 0;
  }
  if (((byte)interr_flag >> 1 & 1) == 0) {
    return 0;
  }
  if ((signed char)rtc_reg[7] < (signed char)'\0') {
    if (((rtc_reg[2] ^ rtc_reg[7]) & 0x1f) != 0) {
      return 0;
    }
    uVar2 = 1;
    bVar1 = rtc_reg[6];
  }
  else {
    uVar2 = 0;
    bVar1 = rtc_reg[6];
  }
  if ((signed char)bVar1 < (signed char)'\0') {
    if (((rtc_reg[1] ^ bVar1) & 0x3f) != 0) {
      return 0;
    }
    uVar2 = 1;
    bVar1 = rtc_reg[5];
  }
  else {
    bVar1 = rtc_reg[5];
  }
  if (-1 < (signed char)bVar1) {
    return uVar2;
  }
  if (((rtc_reg[0] ^ bVar1) & 0x3f) == 0) {
    return 1;
  }
  return 0;
}
void setTime3000(){
	Store(1025, 0);
	rtc_reg[0]++;
	if (rtc_reg[0] == '<') {
      rtc_reg[0] = '\0';
      rtc_reg[1] = rtc_reg[1] + '\x01';
      if (rtc_reg[1] == '<') {
        rtc_reg[1] = '\0';
        rtc_reg[2] = rtc_reg[2] + '\x01';
        if (rtc_reg[2] == '\x18') {
          rtc_reg[2] = '\0';
          rtc_reg[3] = rtc_reg[3] + '\x01';
        }
      }
    }
}
uint8_t trigger256_cnt=0;
void cpu_run_emux(){
	//assert(cycles==cpu->getTotalCycles()/12);

	if((cpu->P&4)==0)
	{
		bus->speed_slowdown=1;
		string msg=get_message();
		if(!msg.empty()){
			handle_cmd(msg);
		}
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

	bool trigger256=trigger_x_times_per_s(256);

	if(trigger256){
		if(nc1020mode||nc2000mode||nc3000mode){
			if(trigger256_cnt==0){
				//at the begin of every second
				setTime3000();
			}
			//bump the 1/256 second
			rtc_reg[4]=trigger256_cnt;
		}
	}

	//todo study datasheet of how speed affect timers
	uint32_t target_cycles=128*12; target_cycles/=bus->speed_slowdown;
	uint32_t CpuTicks=cpu->exec2(target_cycles)/12;
	CpuTicks*=bus->speed_slowdown;
	last_cycles=cycles;
	cycles+=CpuTicks;

	//magic number to fit timer0 and timer1's code
	if(trigger_x_times_per_s(576*50)){
		//printf("trigger1!\n");
		bus->setTimer();
		if(pc1000mode){
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
		
	}

	if(nc1020mode||nc2000mode||nc3000mode){
		bool KeepTimer01( unsigned int cpuTick);
		if(KeepTimer01(CpuTicks)){
			//adapted from wayback
			if ( ram_io[0x04] & 0x0F ) {
				//why only set flag for timer0? why not set 0x20
				extern bool hack_need_irq_by_timer0;
				extern bool hack_need_irq_by_timer1;
				if(hack_need_irq_by_timer0){
					ram_io[0x01] |= 0x10u; 
					cpu->IRQ();
				}
				if(hack_need_irq_by_timer1){
					ram_io[0x01] |= 0x20u; 
					cpu->IRQ();
				}
			}
		}
	}
	if(trigger256){
		if (bus->timeBaseEnable()) {
            //timebase中断为4ms一次，主要用于键盘扫描
            bus->setIrqTimeBase();
            cpu->IRQ();
        }

		if(nc1020mode||nc2000mode||nc3000mode){
			if(trigger256_cnt%128==0){
				if(trigger256_cnt==0&& chk_ar()){
					printf("chk_ar() return true!\n");
					ram_io[0x3d] = 0x20;
					interr_flag&=0xfd;	
				}else{
					if(rtc_reg[10]&1 &&interr_flag&1){
						ram_io[0x3d] =0;
						cpu->IRQ();
					}
				}
				if (bus->nmiEnable()){
					printf("nmi!\n");
					cpu->NMI();
				}
			}
		}

	}

	if(pc1000mode&&trigger_x_times_per_s(2)){
		setTime1000();
		if (bus->nmiEnable()){
			cpu->NMI();
		}
	}

	if(trigger256){
		trigger256_cnt++;
	}
}
