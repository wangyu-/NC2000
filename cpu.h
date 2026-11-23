#pragma once

#include "compare/c6502.h"
#include "comm.h"

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

    bool I();

    //use irq_line instead of set_irq_pending()
    void set_irq_mode(IrqMode mode){
        irq_mode = mode;
    }

    bool irq_pending();

    void set_irq_pending();


    private:
    CpuVersion version = CPU_INVALID; //use a flag instead of inheritance, to avoid virtual function overhead
    IrqMode irq_mode= IrqMode::IRQ_NONE;

    //only for emux cpu
    C6502 *cpu_impl_emux = NULL;
    int emux_exec_helper(int max_cycles);

    //only for w65c02
    int cycle=0;
};

extern CPUInterface* cpu;
