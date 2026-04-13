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

#include "cmd.h"
#include <thread>
#include "CC800IOName.h"
#include "compare/ibus6502.h"
#include "compare/c6502.h"

#define qDebug(...)

#define TF_IRQFLAG 0x10

extern nc2k_states_t nc2k_states;

static unsigned short &gThreadFlags=nc2k_states.gThreadFlags;
static bool &timer0run= nc2k_states.timer0run;
static bool &timer1run_tmie= nc2k_states.timer1run_tmie;
static int gDeadlockCounter = 0;
static unsigned char *zpioregs=nc2k_states.ram_io;

static uint64_t& cycles = nc2k_states.cycles;
//static bool& should_irq = nc1020_states.should_irq;
static bool& timer0_toggle = nc2k_states.timer0_toggle;
static uint64_t& unknown_timer_cycles = nc2k_states.unknown_timer_cycles;
static uint64_t& timer0_cycles = nc2k_states.timer0_cycles;
static uint64_t& timer1_cycles = nc2k_states.timer1_cycles;
static uint64_t& timebase_cycles = nc2k_states.timebase_cycles;

static uint64_t& nmi_cycles = nc2k_states.nmi_cycles;

static bool& should_wake_up = nc2k_states.should_wake_up;

void AdjustTime(){ //legacy code, only used in old cpu loop
	uint8_t* clock_buff = nc2k_states.clock_buff;
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

bool IsCountDown(){//legacy code, only used in old cpu loop
	uint8_t* clock_buff = nc2k_states.clock_buff;
	uint8_t& clock_flags = nc2k_states.clock_flags;
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

	memcpy(nc2k_states.ext_ram, inject_code.c_str(), inject_code.size());
	ram_io[0x00]=0x80;
	ram_io[0x0a]=0x80;
	super_switch();
	cpu->PC=0x4018;
}


void CheckTimebaseAndSetIRQTBI()//legacy code, only used in old cpu loop
{
    if (zpioregs[io04_general_ctrl] & 0x0F) {
        gThreadFlags |= 0x10; // Add IRQ flag
        //irq = 0; // TODO: move to NMI check
        zpioregs[io01_int_status] |= 0x8; // TIMEBASE INTERRUPT
    }
}

void CheckTimebaseSetTimer0IntStatusAddIRQFlag()//legacy code, only used in old cpu loop
{
    if ( zpioregs[io04_general_ctrl] & 0x0F )
    {
        gThreadFlags |= 0x10u; // Add 0x10 Flag to gThreadFlag
        //irq = 0; // TODO: move to 
        zpioregs[io01_int_status] |= 0x10u; // TIMER/COUNTER 0 INTERRUPT : TMODE0: TMI / TMODE1;3: TM0I
    }
}

void EnableWatchDogFlag()//legacy code, only used in old cpu loop
{
    gThreadFlags |= 0x80;
}

// TODO: increase timer value by speed
// seems PC1000's rom never start timer0/1 in tracing
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
    // timer 1 next, only mode1
    if (timer1run_tmie && w0c_b67_TMODESL == 1) {
        timer1ticks += cpuTick;
        qDebug("timer1ticks: %d", timer1ticks);

        int inc1 = timer1ticks >> ((w0c_b23_TM1S+1)*2);
        if (inc1) {
            timer1ticks -= inc1 << ((w0c_b23_TM1S+1)*2);
        }

        //originl wayback code. seems like it's wrong according to datasheet
        ////systools wav播放fill buffer速度不对造成破音
        if(false){
            int inc1 = timer1ticks >> (1 + w0c_b23_TM1S);
            if (inc1) {
                timer1ticks -= inc1 << (1 + w0c_b23_TM1S);
            }
        }
        unsigned short newt = zpioregs[io03_timer1_val] + inc1;
        zpioregs[io03_timer1_val] = newt;
        bool overflow = newt > 0xFF;
        if (overflow) {
            _ADD_TM1I_BIT();
            needirq = true;
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

		if(injected && tick%1==0){
			//printf("bs=%x roa_bbs=%x pc=%x  %x %x %x %x \n",ram_io[0x00], ram_io[0x0a], reg_pc,  Peek16(reg_pc), Peek16(reg_pc+1),Peek16(reg_pc+2),Peek16(reg_pc+3));
			//getchar();
		}

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
			auto &reg_pc=cpu->PC;
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
				nc2k_states.clock_flags &= 0xFD;
			}
			//g_irq = true;
		}

		if ((nc1020mode) && cycles >= timebase_cycles) {
			timebase_cycles += CYCLES_TIMEBASE;

			nc2k_states.clock_buff[4] ++;
			if (should_wake_up) {
				should_wake_up = false;
				ram_io[0x01] |= 0x01;
				ram_io[0x02] |= 0x01;
				cpu->PC = PeekW(RESET_VEC);
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

void cpu_run2(){
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
		cpu->set_nmi_pending();
		qDebug("ggv wanna NMI.");
		gDeadlockCounter--; // wrong behavior of wqxsim
	} else if ((gThreadFlags & TF_IRQFLAG) != 0) {
		gThreadFlags &= 0xFFEFu; // remove 0x10 IRQ Flag
		g_irq = TRUE; // B flag (AF_BREAK) will remove in CpuExecute
		cpu->irq_now();
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
		printf("bs=%02x roa_bbs=%02x ramb=%02x zp=%02x reg=%02x,%02x,%02x,%02x,%03o  pc=%s",ram_io[0x00], ram_io[0x0a], ram_io[0x0d], ram_io[0x0f],cpu->A,cpu->X,cpu->Y,cpu->SP,cpu->P(),disassemble_next(buf,cpu->PC).c_str());
		printf("\n");

		//getchar();		
	}

	
	uint32_t CpuTicks=cpu->execute(0);
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
			nc2k_states.clock_flags &= 0xFD;
		}
		//g_irq = true;
	}
	static int my_cnt=0;
	if ((nc1020mode) && cycles >= timebase_cycles) {
		timebase_cycles += CYCLES_TIMEBASE;
		my_cnt++;
		// cheat boot program to pass
		if(my_cnt%20==10){
				ram_io[0x0c]|=0x01;
		}
		else{
				ram_io[0x0c]&=0xfe;
		}
		nc2k_states.clock_buff[4] ++;
		if (should_wake_up) {
			should_wake_up = false;
			ram_io[0x01] |= 0x01;
			ram_io[0x02] |= 0x01;
			cpu->PC = PeekW(RESET_VEC);
		} else {
			ram_io[0x01] |= 0x08;
			////////////g_irq = true;
			gThreadFlags |= 0x10;
		}
		//printf("timebase!!\n");
	}
}
