#pragma once

#include "compare/c6502.h"
#include "comm.h"

void cpu_run();
void cpu_run2();
void cpu_run3();

//void init_cpu();
void init_cpu_new();

extern unsigned char illegal_op_byte[256];
extern unsigned char illegal_op_cycle[256];

class CPUInterface{
public:
	int &A;
    int &X;
    int &Y;
    //int &P;
    int &SP;
    int &PC;

    //simulate a delay after mI is cleared, which is seen on some old 6502 CPUs
    //to improve compatiblity with CLI + RTI  (CLI followed by RTI immediately)
    // (not 100% sure if this is real wqx hardware behavior)
    int mi_clear_delay=0; 

    CPUInterface();
    CPUInterface(C6502 *cpu);

    void init();
    void reset();
    int execute(int max_cycles);
    void set_nmi_pending();
	void irq_now();//only used for old code
    int P();

    void set_irq_pending();


    private:
    CpuVersion version = CPU_INVALID; //use a flag instead of inheritance, to avoid virtual function overhead

    //only for emux cpu
    C6502 *cpu_impl_emux = NULL;
    int emux_exec_helper(int max_cycles);

    //only for w65c02
    int cycle=0;
};

extern CPUInterface* cpu;
