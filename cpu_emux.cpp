#include "ansi/c6502.h"
#include "mem.h"
#include "console.h"
#include "state.h"
#include "disassembler.h"

#define qDebug(...)


extern unsigned short gThreadFlags;
extern nc1020_states_t nc1020_states;
static uint64_t& cycles = nc1020_states.cycles;
static uint64_t& nmi_cycles = nc1020_states.nmi_cycles;
static uint64_t& timebase_cycles = nc1020_states.timebase_cycles;

struct Bus:IBus6502{
	Bus(){
	}
	int read(int address){
		return Load(address);
	}
    void write(int address, int value){
		Store(address,value);
	}
}bus;
C6502 *cpu;
bool cpu_emux_initalized;
void cpu_run_emux(){
	if(!cpu_emux_initalized){
		cpu_emux_initalized=true;
		cpu=new C6502(&bus);
		cpu->reset();
	}
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
		cpu->NMI();
		qDebug("ggv wanna NMI.");
		//fprintf(stderr, "ggv wanna NMI.\n");
	} else if ((gThreadFlags & 0x10) != 0) {
		gThreadFlags &= 0xFFEFu; // remove 0x10 IRQ Flag
		cpu->IRQ();
		qDebug("ggv wanna IRQ.");
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

	
	uint32_t CpuTicks=cpu->exec_one()/12;
	cycles+=CpuTicks;

    void CheckTimebaseAndSetIRQTBI();
    bool KeepTimer01( unsigned int cpuTick );
    void CheckTimebaseSetTimer0IntStatusAddIRQFlag();

	bool needirq = false;
	if ((nc2000mode||nc3000mode||pc1000mode) && cycles >= timebase_cycles) {
		timebase_cycles += CYCLES_TIMEBASE;
		if ((gThreadFlags & 0x80u) == 0) {
			// CheckTimerbaseAndEnableIRQnEXIE1

			CheckTimebaseAndSetIRQTBI();//??? timebase doesn't trigger needirq??
			needirq = KeepTimer01(CpuTicks);
		} else {
			assert(false);
		}
	} else {
		needirq = KeepTimer01(CpuTicks);
	}
	
	if (needirq) {
		//printf("needirq is true\n");
		CheckTimebaseSetTimer0IntStatusAddIRQFlag();
	}

}
