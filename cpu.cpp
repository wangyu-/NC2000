#include "compare/c6502.h"
extern "C" {
#include "ansi/w65c02.h"
}
#include "comm.h"
#include "cpu.h"
#include "ram.h"

CPUInterface *cpu;

unsigned char illegal_op_byte[256];
unsigned char illegal_op_cycle[256];

void CPUInterface::reset(){
	if(cpu_impl_emux) return cpu_impl_emux->reset();

	CpuInitialize();
	
}

int CPUInterface::emux_exec_helper(int max_cycles) {
	auto &clk=cpu_impl_emux->clk;
	auto &lineclk=cpu_impl_emux->lineclk;
	auto &total_cycles=cpu_impl_emux->total_cycles;
	auto &nmiPending=cpu_impl_emux->nmiPending;
	auto &irqPending=cpu_impl_emux->irqPending;

	if(nmiPending){
		nmiPending = false;
		cpu_impl_emux->doNMI();
	}
	do{	
		if (irqPending && (P() & 4) == 0) {
			irqPending = false;
			cpu_impl_emux->doIRQ();
		}
		void debug_pc();
		debug_pc();
		cpu_impl_emux->doCode(cpu_impl_emux->getCode());//allow one cycle anyway
	}while(clk<=max_cycles);
	
	int res=clk;// - initial_clk;
	lineclk += clk;
	total_cycles += clk;
	clk=0; //set to 0, instead of clk-= max_cycles. this the way how the new cpu loop works, this isn't abug.

	return res;
}

int CPUInterface::execute(int max_cycles){
	if(cpu_impl_emux) {
		return emux_exec_helper(max_cycles*12)/12;
	}

	//note: cycle can be non-zero, since IRQ can be called from outside 

    if(g_nmi){
        g_nmi = false;
        cycle+=CpuExecuteNMI();
		mI = true;     //todo: is this needed?
    }

    do{
		if (g_irq && !mI){
			g_irq = false;
			if(enable_dyn_debug_next_n) printf("execute irq!!!!!!!\n");
			cycle+=CpuExecuteIRQ();
			//cycle+=1;
		}
		void debug_pc();
		debug_pc();
		if(g_wai) {cycle=max_cycles;if(cycle<=0) cycle=1;break;}
		cycle += CpuExecuteOP();
    }while(cycle<=max_cycles);

	int res=cycle;
	cycle=0;
    
    return res;
}

void CPUInterface::set_nmi_pending() {
	if(cpu_impl_emux) return cpu_impl_emux->NMI();

	g_nmi = true;
}

void CPUInterface::irq_now() {
	if(cpu_impl_emux) return cpu_impl_emux->IRQ();

	if(!mI) {
		cycle+=CpuExecuteIRQ();
		//cycle+=1;
		g_irq =false; // is this needed?
	}else{
		g_irq = true;
	}

}
void CPUInterface::set_irq_pending() {
	if(enable_dyn_debug_next_n) printf("set_irq_pending!!!\n");
	if(cpu_impl_emux) {
		cpu_impl_emux->irqPending = true;
		return;
	}
	g_irq = true;
}

int CPUInterface::P() {
	if(cpu_impl_emux) return cpu_impl_emux->P;

	return PS();
}

void initalize_illegal_op_tables(){
	unsigned char * byte=illegal_op_byte;
	unsigned char * cycle=illegal_op_cycle;

	//03 fixes nctools (all version) "S h"
	//13 and 1f fixes nctools 4.0's "制作应用程序" and "系统信息->文件列表"

	// below value from https://www.masswerk.at/6502/6502_instruction_set.html

	//ALR(asr)
	byte[0x4b]=2;cycle[0x4b]=2;

	//ANC
	byte[0x0b]=2;cycle[0x0b]=2;

	//ANC(ANC2)
	byte[0x2b]=2;cycle[0x2b]=2;

	//ANE(XAA)
	byte[0x8b]=2;cycle[0x8b]=2;

	//ARR
	byte[0x6b]=2;cycle[0x6b]=2;

	//DCP(DCM)
	byte[0xc7]=2;cycle[0xc7]=5;
	byte[0xd7]=2;cycle[0xd7]=6;
	byte[0xcf]=3;cycle[0xcf]=6;
	byte[0xdf]=3;cycle[0xdf]=7;
	byte[0xdb]=3;cycle[0xdb]=7;
	byte[0xc3]=2;cycle[0xc3]=8;
	byte[0xd3]=2;cycle[0xd3]=8;

	//ISC(ISB,INS)
	byte[0xe7]=2;cycle[0xe7]=5;
	byte[0xf7]=2;cycle[0xf7]=6;
	byte[0xef]=3;cycle[0xef]=6;
	byte[0xff]=3;cycle[0xff]=7;
	byte[0xfb]=3;cycle[0xfb]=7;
	byte[0xe3]=2;cycle[0xe3]=8;
	byte[0xf3]=2;cycle[0xf3]=8;

	//LAS (LAR)
	byte[0xbb]=3;cycle[0xbb]=4;

	//LAX
	byte[0xa7]=2;cycle[0xa7]=3;
	byte[0xb7]=2;cycle[0xb7]=4;
	byte[0xaf]=3;cycle[0xaf]=4;
	byte[0xbf]=3;cycle[0xbf]=4;
	byte[0xa3]=2;cycle[0xa3]=6;
	byte[0xb3]=2;cycle[0xb3]=5;

	//LXA(LAX immediate)
	byte[0xab]=2;cycle[0xab]=2;

	//RLA
	byte[0x27]=2;cycle[0x27]=5;
	byte[0x37]=2;cycle[0x37]=6;
	byte[0x2f]=3;cycle[0x2f]=6;
	byte[0x3f]=3;cycle[0x3f]=7;
	byte[0x3b]=3;cycle[0x3b]=7;
	byte[0x23]=2;cycle[0x23]=8;
	byte[0x33]=2;cycle[0x33]=8;

	//RRA
	byte[0x67]=2;cycle[0x67]=5;
	byte[0x77]=2;cycle[0x77]=6;
	byte[0x6f]=3;cycle[0x6f]=6;
	byte[0x7f]=3;cycle[0x7f]=7;
	byte[0x7b]=3;cycle[0x7b]=7;
	byte[0x63]=2;cycle[0x63]=8;
	byte[0x73]=2;cycle[0x73]=8;

	//SAX(AXS,AAX)
	byte[0x87]=2;cycle[0x87]=3;
	byte[0x97]=2;cycle[0x97]=4;
	byte[0x8f]=3;cycle[0x8f]=4;
	byte[0x83]=2;cycle[0x83]=6;

	//SBX(AXS,SAX)
	byte[0xcb]=2;cycle[0xcb]=2;

	//SHA(AHX,AXA)
	byte[0x9f]=3;cycle[0x9f]=5;
	byte[0x93]=2;cycle[0x93]=6;

	//SHX(A11,SXA,XAS)
	byte[0x9e]=3;cycle[0x9e]=5;

	//SHY(A11,SYA,SAY)
	byte[0x9c]=3;cycle[0x9c]=5;

	//SLO(ASO)
	byte[0x07]=2;cycle[0x07]=5;
	byte[0x17]=2;cycle[0x17]=6;
	byte[0x0f]=3;cycle[0x0f]=6;
	byte[0x1f]=3;cycle[0x1f]=7;
	byte[0x1b]=3;cycle[0x1b]=7;
	byte[0x03]=2;cycle[0x03]=8;
	byte[0x13]=2;cycle[0x13]=8;

	//SRE(LSE)
	byte[0x47]=2;cycle[0x47]=5;
	byte[0x57]=2;cycle[0x57]=6;
	byte[0x4f]=3;cycle[0x4f]=6;
	byte[0x5f]=3;cycle[0x5f]=7;
	byte[0x5b]=3;cycle[0x5b]=7;
	byte[0x43]=2;cycle[0x43]=8;
	byte[0x53]=2;cycle[0x53]=8;

	//TAS(XAS,SHS)
	byte[0x9b]=3;cycle[0x9b]=5;

	//USBC(SBC)
	byte[0xeb]=2;cycle[0xeb]=2;

	//NOPS(including DOP, TOP)
	byte[0x1a]=1;cycle[0x1a]=2;
	byte[0x3a]=1;cycle[0x3a]=2;
	byte[0x5a]=1;cycle[0x5a]=2;
	byte[0x7a]=1;cycle[0x7a]=2;
	byte[0xda]=1;cycle[0xda]=2;
	byte[0xfa]=1;cycle[0xfa]=2;
	byte[0x80]=2;cycle[0x80]=2;
	byte[0x82]=2;cycle[0x82]=2;
	byte[0x89]=2;cycle[0x89]=2;
	byte[0xc2]=2;cycle[0xc2]=2;
	byte[0xe2]=2;cycle[0xe2]=2;
	byte[0x04]=2;cycle[0x04]=3;
	byte[0x44]=2;cycle[0x44]=3;
	byte[0x64]=2;cycle[0x64]=3;
	byte[0x14]=2;cycle[0x14]=4;
	byte[0x34]=2;cycle[0x34]=4;
	byte[0x54]=2;cycle[0x54]=4;
	byte[0x74]=2;cycle[0x74]=4;
	byte[0xd4]=2;cycle[0xd4]=4;
	byte[0xf4]=2;cycle[0xf4]=4;
	byte[0x0c]=3;cycle[0x0c]=4;
	byte[0x1c]=3;cycle[0x1c]=4;
	byte[0x3c]=3;cycle[0x3c]=4;
	byte[0x5c]=3;cycle[0x5c]=4;
	byte[0x7c]=3;cycle[0x7c]=4;
	byte[0xdc]=3;cycle[0xdc]=4;
	byte[0xfc]=3;cycle[0xfc]=4;

	const int jam_cycles=6;//jam hangs the cpu it doesn't really has cycles, use 6 as a placeholder

	//JAM (KIL,HLT)
	byte[0x02]=1;cycle[0x02]=jam_cycles;
	byte[0x12]=1;cycle[0x12]=jam_cycles;
	byte[0x22]=1;cycle[0x22]=jam_cycles;
	byte[0x32]=1;cycle[0x32]=jam_cycles;
	byte[0x42]=1;cycle[0x42]=jam_cycles;
	byte[0x52]=1;cycle[0x52]=jam_cycles;
	byte[0x62]=1;cycle[0x62]=jam_cycles;
	byte[0x72]=1;cycle[0x72]=jam_cycles;
	byte[0x92]=1;cycle[0x92]=jam_cycles;
	byte[0xb2]=1;cycle[0xb2]=jam_cycles;
	byte[0xd2]=1;cycle[0xd2]=jam_cycles;
	byte[0xf2]=1;cycle[0xf2]=jam_cycles;
}
