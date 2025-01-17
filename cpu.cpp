#include "cpu.h"
#include "NekoDriverIO.h"

extern "C" {
#include "ansi/w65c02.h"
}
#include "comm.h"
#include "disassembler.h"
#include "mem.h"
#include "state.h"
#include <mutex>

#include "console.h"
#include <thread>
#include "CC800IOName.h"
#include "ansi/ibus6502.h"
#include "ansi/c6502.h"

#define qDebug(...)

#define TF_IRQFLAG 0x10
extern unsigned short gThreadFlags;
extern unsigned char * zpioregs;
extern bool timer0run;
extern bool timer1run_tmie;
int gDeadlockCounter = 0;

extern nc1020_states_t nc1020_states;

static uint64_t& cycles = nc1020_states.cycles;
//static bool& should_irq = nc1020_states.should_irq;
static bool& timer0_toggle = nc1020_states.timer0_toggle;
static uint64_t& unknown_timer_cycles = nc1020_states.unknown_timer_cycles;
static uint64_t& timer0_cycles = nc1020_states.timer0_cycles;
static uint64_t& timer1_cycles = nc1020_states.timer1_cycles;
static uint64_t& timebase_cycles = nc1020_states.timebase_cycles;

static uint64_t& nmi_cycles = nc1020_states.nmi_cycles;

static bool& should_wake_up = nc1020_states.should_wake_up;

/*
static uint16_t& reg_pc = nc1020_states.cpu.reg_pc;
static uint8_t& reg_a = nc1020_states.cpu.reg_a;
static uint8_t& reg_ps = nc1020_states.cpu.reg_ps;
static uint8_t& reg_x = nc1020_states.cpu.reg_x;
static uint8_t& reg_y = nc1020_states.cpu.reg_y;
static uint8_t& reg_sp = nc1020_states.cpu.reg_sp;
*/

static int& reg_pc = mPC;

double speed_multiplier=1.0;

void reset_cpu_states(){
	//nc1020_states.should_irq = false;
	nc1020_states.cycles = 0;
	/*
	nc1020_states.cpu.reg_a = 0;
	nc1020_states.cpu.reg_ps = 0x24;
	nc1020_states.cpu.reg_x = 0;
	nc1020_states.cpu.reg_y = 0;
	nc1020_states.cpu.reg_sp = 0xFF;
	nc1020_states.cpu.reg_pc = PeekW(RESET_VEC);*/
	nc1020_states.timer0_cycles = CYCLES_TIMER0;
	nc1020_states.timer1_cycles = CYCLES_TIMER1;
	nc1020_states.nmi_cycles = 0;
	nc1020_states.unknown_timer_cycles = CYCLES_UNKNOWN_TIMER;
	CpuInitialize();
	setPS(0x24);
	CreateHotlinkMapping();
}
void AdjustTime(){
	uint8_t* clock_buff = nc1020_states.clock_buff;
    if (++ clock_buff[0] >= 60) {
        clock_buff[0] = 0;
        if (++ clock_buff[1] >= 60) {
            clock_buff[1] = 0;
            if (++ clock_buff[2] >= 24) {
                clock_buff[2] &= 0xC0;
                ++ clock_buff[3];
            }
        }
    }
}

bool IsCountDown(){
	uint8_t* clock_buff = nc1020_states.clock_buff;
	uint8_t& clock_flags = nc1020_states.clock_flags;
    if (!(clock_buff[10] & 0x02) ||
        !(clock_flags & 0x02)) {
        return false;
    }
    return (
        ((clock_buff[7] & 0x80) && !(((clock_buff[7] ^ clock_buff[2])) & 0x1F)) ||
        ((clock_buff[6] & 0x80) && !(((clock_buff[6] ^ clock_buff[1])) & 0x3F)) ||
        ((clock_buff[5] & 0x80) && !(((clock_buff[5] ^ clock_buff[0])) & 0x3F))
        );
}

void inject(){
	for(int i=0;i<8;i++){
		for(int j=0;j<=0xFE;j+=2){
			ram_b[0x100*i+j]=j;
			ram_b[0x100*i+j+1]=i;
		}
	}

	for(int i=0;i<16;i++){
		printf("<%x>",ram_b[i]);
	}
	printf("\n");

	memcpy(nc1020_states.ext_ram, inject_code.c_str(), inject_code.size());
	ram_io[0x00]=0x80;
	ram_io[0x0a]=0x80;
	//Peek16(0xe3)=0x40;
	//ram_io[0x0d]&=0x5d;
	super_switch();
	//Peek16(0x280)=0xFF;
	//Peek16(0x281)=0xFF;
	//Peek16(0x282)=0xFF;
	//Peek16(0x283)=0xFF;
	//Peek16(0xe3)=0x40;
	//Peek16(0xe4)=0xb2;
	//nc1020_states.ext_ram[0x17]=0x58;
	reg_pc=0x4018;
}


void CheckTimebaseAndSetIRQTBI()
{
	//////////TODO: remove the or true
    if (zpioregs[io04_general_ctrl] & 0x0F) {
        gThreadFlags |= 0x10; // Add IRQ flag
        //irq = 0; // TODO: move to NMI check
        zpioregs[io01_int_status] |= 0x8; // TIMEBASE INTERRUPT
    }
}

void CheckTimebaseSetTimer0IntStatusAddIRQFlag()
{
    if ( zpioregs[io04_general_ctrl] & 0x0F )
    {
        gThreadFlags |= 0x10u; // Add 0x10 Flag to gThreadFlag
        //irq = 0; // TODO: move to 
        zpioregs[io01_int_status] |= 0x10u; // TIMER/COUNTER 0 INTERRUPT : TMODE0: TMI / TMODE1;3: TM0I
    }
}

void EnableWatchDogFlag()
{
    gThreadFlags |= 0x80;
}

// TODO: increase timer value by speed
// seems PC1000's rom never start timer0/1 in tracing
bool hack_need_irq_by_timer0;
bool hack_need_irq_by_timer1;
bool KeepTimer01( unsigned int cpuTick )
{
    bool needirq = false;
    // 0: no timerbase 1~15 = LN0,L0..L13
    unsigned char tbc = zpioregs[io04_general_ctrl] & 0xF;
    //if (tbc == 0) {
    //    return false;
    //}
    // proctimeer0 first
    if (timer0run) {
        timer0ticks += cpuTick;
        qDebug("timer0ticks: %d", timer0ticks);
        int mul0 = 1 + (w0c_b67_TMODESL == 1?w0c_b45_TM0S:w0c_b345_TMS);
        int inc0, inc1;
        inc0 = timer0ticks >> mul0;
        if (inc0) {
            timer0ticks -= inc0 << mul0;
        }
        if (w0c_b67_TMODESL == 1 || w0c_b67_TMODESL == 0) {
            // TODO: speed by CpuTicks
            unsigned short newt = zpioregs[io02_timer0_val] + inc0;
            bool overflow = newt > 0xFF;
            if (overflow) {
                if (w0c_b67_TMODESL == 1) {
                    _ADD_TM0I_BIT();
                    needirq = true;
                } else if (timer1run_tmie) {
                    _ADD_TM0I_BIT();
                    needirq = true;
                }
            }
            zpioregs[io02_timer0_val] = w0c_b67_TMODESL==1?newt:newt + zpioregs[io03_timer1_val]; // as reload value
        }
        // 16bit
        if (w0c_b67_TMODESL == 2) {
            unsigned short newt = zpioregs[io02_timer0_val] + inc0;
            zpioregs[io02_timer0_val] = newt;
            bool overflow = newt > 0xFF;
            if (overflow) {
                unsigned short newt1 = zpioregs[io03_timer1_val] + (newt >> 8);
                if (newt1 > 0xFF) {
                    _ADD_TM1I_BIT();
                    needirq = true;
                }
                zpioregs[io03_timer1_val] = newt1;
            }
        }
        if (w0c_b67_TMODESL == 3) {
            unsigned short newt = zpioregs[io02_timer0_val] + inc0;
            zpioregs[io02_timer0_val] = newt;
            bool overflow = newt > 0xFF;
            if (overflow) {
                _ADD_TM0I_BIT();
                needirq = true;
                if (timer1run_tmie) {
                    unsigned short newt1 = zpioregs[io03_timer1_val] + (newt >> 8);
                    if (newt1 > 0xFF) {
                        _ADD_TM1I_BIT();
                        needirq = true;
                    }
                    zpioregs[io03_timer1_val] = newt1;
                }
            }

        }
    }
	hack_need_irq_by_timer0=needirq;
    // timer 1 next, only mode1
    if (timer1run_tmie && w0c_b67_TMODESL == 1) {
        timer1ticks += cpuTick;
        qDebug("timer1ticks: %d", timer1ticks);

        int inc1 = timer1ticks >> (1 + w0c_b23_TM1S);
        if (inc1) {
            timer1ticks -= inc1 << (1 + w0c_b23_TM1S);
        }
        unsigned short newt = zpioregs[io03_timer1_val] + inc1;
        zpioregs[io03_timer1_val] = newt;
        bool overflow = newt > 0xFF;
        if (overflow) {
            _ADD_TM1I_BIT();
            needirq = true;
			hack_need_irq_by_timer1=true;
        }else{
			hack_need_irq_by_timer1=false;
		}
    }
    return needirq;
}


void cpu_run(){
		string msg=get_message();
		if(!msg.empty()){
			handle_cmd(msg);
		}
		tick++;

		if(enable_inject&& tick>60000000) wanna_inject=true;
		if(wanna_inject&& !injected){
			inject();
			printf("injected!!!");
			wanna_inject=false;
			injected=true;
		}

		//if(tick>=6012850) enable_debug_pc=true;
		//if(injected && reg_pc==0x2000) enable_debug_pc=true;

		if(injected && tick%1==0){
			//printf("bs=%x roa_bbs=%x pc=%x  %x %x %x %x \n",ram_io[0x00], ram_io[0x0a], reg_pc,  Peek16(reg_pc), Peek16(reg_pc+1),Peek16(reg_pc+2),Peek16(reg_pc+3));
			//getchar();
		}

		//printf("%d\n",cycles);

		if(pc1000mode){
			//using spdc1016freq is a hack for diff with wayback
			//otherwise should use CYCLES_SECOND
			const uint32_t spdc1016freq=3686400;
			if(nmi_cycles ==0 ){
				nmi_cycles +=spdc1016freq/2;
			}
			if (cycles >= nmi_cycles) {
				nmi_cycles += spdc1016freq/2;
				gThreadFlags |= 0x08; // Add NMIFlag
			}
		}

		// NMI > IRQ
		
		if ((gThreadFlags & 0x08) != 0) {
			gThreadFlags &= 0xFFF7u; // remove 0x08 NMI Flag
			// FIXME: NO MORE REVERSE
			g_nmi = TRUE; // next CpuExecute will execute two instructions
			qDebug("ggv wanna NMI.");
			//fprintf(stderr, "ggv wanna NMI.\n");
			gDeadlockCounter--; // wrong behavior of wqxsim
		} else if (((PS() & AF_INTERRUPT) == 0) && ((gThreadFlags & TF_IRQFLAG) != 0)) {
			gThreadFlags &= 0xFFEFu; // remove 0x10 IRQ Flag
			g_irq = TRUE; // B flag (AF_BREAK) will remove in CpuExecute
			qDebug("ggv wanna IRQ.");
			gDeadlockCounter--; // wrong behavior of wqxsim
		}

		if(enable_debug_pc||enable_dyn_debug){
			uint8_t & Peek16Debug(uint16_t addr);
			unsigned char buf[10];
			buf[0]=Peek16Debug(reg_pc);
			buf[1]=Peek16Debug(reg_pc+1);
			buf[2]=Peek16Debug(reg_pc+2);
			buf[3]=0;
			printf("tick=%lld ",tick /*, reg_pc*/);
			printf("%02x %02x %02x %02x; ",Peek16Debug(reg_pc), Peek16Debug(reg_pc+1),Peek16Debug(reg_pc+2),Peek16Debug(reg_pc+3));
			printf("bs=%02x roa_bbs=%02x ramb=%02x zp=%02x reg=%02x,%02x,%02x,%02x,%03o  pc=%s",ram_io[0x00], ram_io[0x0a], ram_io[0x0d], ram_io[0x0f],mA,mX,mY,mSP,PS(),disassemble_next(buf,reg_pc).c_str());
			printf("\n");

			//getchar();		
		}

		uint32_t CpuTicks = CpuExecute();
		cycles+=CpuTicks;

		if(nc2000mode||nc3000mode){
			Store(1025, 0); //set idle time to zero, prevent sleep
		}
		gDeadlockCounter++;
		bool needirq = false;
		//don't use magic number
		//if (gDeadlockCounter == 6000) {
		if ((nc2000mode||nc3000mode||pc1000mode) && cycles >= timebase_cycles) {
			timebase_cycles += CYCLES_TIMEBASE;
			// overflowed
			gDeadlockCounter = 0;
			if ((gThreadFlags & 0x80u) == 0) {
				// CheckTimerbaseAndEnableIRQnEXIE1
				CheckTimebaseAndSetIRQTBI();//??? timebase doesn't trigger needirq??
				needirq = KeepTimer01(CpuTicks);
			} else {
				assert(false);
				// RESET
				zpioregs[io01_int_enable] |= 0x1; // TIMER A INTERRUPT ENABLE
				zpioregs[io02_timer0_val] |= 0x1; // [io01+1] Timer0 bit1 = 1
				gThreadFlags &= 0xFF7F;      // remove 0x80 | 0x10
				//mPC = *(unsigned short*)&pmemmap[mapE000][0x1FFC];
				mPC = Peek16(0xFFFC);
			}
		} else {
			needirq = KeepTimer01(CpuTicks);
		}
		
		if (needirq) {
			//printf("needirq is true\n");
			CheckTimebaseSetTimer0IntStatusAddIRQFlag();
		}

		if ((nc1020mode) && cycles >= unknown_timer_cycles) {
			unknown_timer_cycles += CYCLES_UNKNOWN_TIMER;
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
			//g_irq = true;
		}

		if ((nc1020mode) && cycles >= timebase_cycles) {
			timebase_cycles += CYCLES_TIMEBASE;

			nc1020_states.clock_buff[4] ++;
			if (should_wake_up) {
				should_wake_up = false;
				ram_io[0x01] |= 0x01;
				ram_io[0x02] |= 0x01;
				reg_pc = PeekW(RESET_VEC);
			} else {
				ram_io[0x01] |= 0x08;
				////////////g_irq = true;
				gThreadFlags |= 0x10;
			}
			//printf("timebase!!\n");
		}
		
		/*
		if(should_irq && (enable_debug_pc ||enable_dyn_debug)&&false)
			printf("should irq!\n");*/

}

static struct Bus:IBus6502{
	Bus(){
	}
	int read(int address){
		return Load(address);
	}
    void write(int address, int value){
		Store(address,value);
	}
}bus;
extern C6502 *cpu;
void init_cpu2(){
	cpu=new C6502(&bus);
	// now in resetStates()
	//cpu->reset();
}
void cpu_run2(){
	assert(use_emux_cpu);
	assert(!use_emux_bus);

	assert(cycles==cpu->getTotalCycles()/12);

	string msg=get_message();
	if(!msg.empty()){
		handle_cmd(msg);
	}
	tick++;

	if(pc1000mode){
		const uint32_t spdc1016freq=3686400;
		if(nmi_cycles ==0 ){
			nmi_cycles +=spdc1016freq/2;
		}
		if (cycles >= nmi_cycles) {
			nmi_cycles += spdc1016freq/2;
			gThreadFlags |= 0x08; // Add NMIFlag
		}
	}

	if ((gThreadFlags & 0x08) != 0) {
		gThreadFlags &= 0xFFF7u; // remove 0x08 NMI Flag
		// FIXME: NO MORE REVERSE
		g_nmi = TRUE; // next CpuExecute will execute two instructions
		cpu->NMI();
		qDebug("ggv wanna NMI.");
		//fprintf(stderr, "ggv wanna NMI.\n");
		gDeadlockCounter--; // wrong behavior of wqxsim
	} else if ((gThreadFlags & TF_IRQFLAG) != 0) {
		gThreadFlags &= 0xFFEFu; // remove 0x10 IRQ Flag
		g_irq = TRUE; // B flag (AF_BREAK) will remove in CpuExecute
		cpu->IRQ();
		qDebug("ggv wanna IRQ.");
		gDeadlockCounter--; // wrong behavior of wqxsim
	}

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

	
	uint32_t CpuTicks=cpu->exec2(0)/12;
	cycles+=CpuTicks;

	gDeadlockCounter++;
	bool needirq = false;
	//don't use magic number
	//if (gDeadlockCounter == 6000) {
	if ((nc2000mode||nc3000mode||pc1000mode) && cycles >= timebase_cycles) {
		timebase_cycles += CYCLES_TIMEBASE;
		// overflowed
		gDeadlockCounter = 0;
		if ((gThreadFlags & 0x80u) == 0) {
			// CheckTimerbaseAndEnableIRQnEXIE1
			CheckTimebaseAndSetIRQTBI();//??? timebase doesn't trigger needirq?? answer: it triggers by set flag directly
			needirq = KeepTimer01(CpuTicks);
		} else {
			assert(false);
			// RESET
			zpioregs[io01_int_enable] |= 0x1; // TIMER A INTERRUPT ENABLE
			zpioregs[io02_timer0_val] |= 0x1; // [io01+1] Timer0 bit1 = 1
			gThreadFlags &= 0xFF7F;      // remove 0x80 | 0x10
			//mPC = *(unsigned short*)&pmemmap[mapE000][0x1FFC];
			mPC = Peek16(0xFFFC);
		}
	} else {
		needirq = KeepTimer01(CpuTicks);
	}
	
	if (needirq) {
		//printf("needirq is true\n");
		CheckTimebaseSetTimer0IntStatusAddIRQFlag();
	}

	if ((nc1020mode) && cycles >= unknown_timer_cycles) {
		unknown_timer_cycles += CYCLES_UNKNOWN_TIMER;
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
		//g_irq = true;
	}

	if ((nc1020mode) && cycles >= timebase_cycles) {
		timebase_cycles += CYCLES_TIMEBASE;

		nc1020_states.clock_buff[4] ++;
		if (should_wake_up) {
			should_wake_up = false;
			ram_io[0x01] |= 0x01;
			ram_io[0x02] |= 0x01;
			reg_pc = PeekW(RESET_VEC);
		} else {
			ram_io[0x01] |= 0x08;
			////////////g_irq = true;
			gThreadFlags |= 0x10;
		}
		//printf("timebase!!\n");
	}
}
