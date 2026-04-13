#pragma once

#include "compare/c6502.h"
#include "comm.h"

extern "C" {
#include "ansi/w65c02.h"
}

void cpu_run();
void cpu_run2();
void cpu_run3();

//void init_cpu();
void init_cpu_new();

class CPUInterface{
public:
	int &A;
    int &X;
    int &Y;
    //int &P;
    int &SP;
    int &PC;

    CPUInterface():A(mA), X(mX), Y(mY), SP(mSP), PC(mPC) {
        printf("using handypsp cpu\n");
        CpuInitialize();
    };
    CPUInterface(C6502 *cpu) :A(cpu->A), X(cpu->X), Y(cpu->Y), SP(cpu->SP), PC(cpu->PC) {
        printf("using emux cpu\n");
        cpu_impl_emux = cpu;
	    cpu->reset();
    };

    void reset();
    int execute(int max_cycles);
    void set_nmi_pending();
	void irq_now();//only used for old code
    int P();

    void set_irq_pending();


    private:

    //only for emux cpu
    C6502 *cpu_impl_emux = NULL;
    int emux_exec_helper(int max_cycles);

    //only for w65c02
    int cycle=0;
};

extern CPUInterface* cpu;

void initalize_illegal_op_tables();

extern unsigned char illegal_op_byte[256];
extern unsigned char illegal_op_cycle[256];
