#ifdef HANDYPSP

extern "C" {
#include "w65c02.h"
#include "w65c02macro.h"
}
#include <stdio.h>
#include "state.h"
//regsrec regs;

//DWORD     autoboot          = 0;
//BOOL      restart           = 0;
//WORD      iorange           = 0x0040;
extern nc2k_states_t nc2k_states;

BOOL      &g_irq             = nc2k_states.g_irq;    // FIXME: NO MORE REVERSE
BOOL      &g_nmi             = nc2k_states.g_nmi;    // FIXME: NO MORE REVERSE
BOOL      &g_stp             = nc2k_states.g_stp;
BOOL      &g_wai             = nc2k_states.g_wai;
BOOL      &g_wai_saved       = nc2k_states.g_wai_saved;
// CPU Flags & status

int &mA=nc2k_states.mA;     // Accumulator                 8 bits
int &mX=nc2k_states.mX;     // X index register            8 bits
int &mY=nc2k_states.mY;     // Y index register            8 bits
int &mSP=nc2k_states.mSP;        // Stack Pointer               8 bits
int &mOpcode=nc2k_states.mOpcode;  // Instruction opcode          8 bits
int &mOperand=nc2k_states.mOperand; // Instructions operand         16 bits
int &mPC=nc2k_states.mPC;        // Program Counter            16 bits

int &mN=nc2k_states.mN;     // N flag for processor status register
int &mV=nc2k_states.mV;     // V flag for processor status register
int &mB=nc2k_states.mB;     // B flag for processor status register
int &mD=nc2k_states.mD;     // D flag for processor status register
int &mI=nc2k_states.mI;     // I flag for processor status register
int &mZ=nc2k_states.mZ;     // Z flag for processor status register
int &mC=nc2k_states.mC;     // C flag for processor status register
//int mIRQActive;
/*
#ifdef _LYNXDBG
int mPcBreakpoints[MAX_CPU_BREAKPOINTS];
int mDbgFlag;
#endif
*/
//UBYTE *mRamPointer;

// Associated lookup tables

//int mBCDTable[2][256];


//int PS();
//void PS(int ps);

void CpuInitialize()
{
    TRACE_CPU0("Reset()");
    //mRamPointer=mSystem.GetRamPointer();
    mA = 0;
    mX = 0;
    mY = 0;
    mSP = 0xff;
    mOpcode = 0;
    mOperand = 0;
    mPC = CPU_PEEKW(BOOT_VECTOR);
    //printf("<pc=%d>\n",mPC);
    mN = FALSE;
    mV = FALSE;
    mB = FALSE;
    mD = FALSE;
    mI = TRUE;
    mZ = FALSE; // GGV
    mC = FALSE;
    //mIRQActive = FALSE;

    g_nmi = FALSE; // MERGE
    g_irq = FALSE; // MERGE
    g_wai = FALSE;
    g_wai_saved = FALSE;


    if(true){
        setPS(0x24); // originally in NekoDriverMem.cpp, moved here
    }
}

//void SetRegs(C6502_REGS &regs)
//{
//    setPS(regs.PS);
//    mA = regs.A;
//    mX = regs.X;
//    mY = regs.Y;
//    mSP = regs.SP;
//    mOpcode = regs.Opcode;
//    mOperand = regs.Operand;
//    mPC = regs.PC;
//    g_wai = regs.WAIT;
//#ifdef _LYNXDBG
//    for (int loop = 0; loop < MAX_CPU_BREAKPOINTS; loop++) mPcBreakpoints[loop] = regs.cpuBreakpoints[loop];
//#endif
//    g_nmi = regs.NMI; // MERGE
//    g_irq = regs.IRQ; // MERGE
//}

//void GetRegs(C6502_REGS &regs)
//{
//    regs.PS = PS();
//    regs.A = mA;
//    regs.X = mX;
//    regs.Y = mY;
//    regs.SP = mSP;
//    regs.Opcode = mOpcode;
//    regs.Operand = mOperand;
//    regs.PC = mPC;
//    regs.WAIT = (g_wai) ? true : false;
//#ifdef _LYNXDBG
//    for (int loop = 0; loop < MAX_CPU_BREAKPOINTS; loop++) regs.cpuBreakpoints[loop] = mPcBreakpoints[loop];
//#endif
//    regs.NMI = (g_nmi) ? true : false;
//    regs.IRQ = (g_irq) ? true : false;
//}

int GetPC(void)
{
    return mPC;
}

void xILLEGAL(void)
{
    //char addr[1024];
    //sprintf(addr,"C65C02::Update() - Illegal opcode (%02x) at PC=$%04x.",mOpcode,mPC);
    //gError->Warning(addr);
    uint8_t & Peek16Debug(uint16_t addr);
    if(debug_level>=1 || enable_dyn_debug || enable_dyn_debug_next_n>0) {
        printf("illegal opcode %02x at pc=$%04x, bs=%02x roabbs=%02x vol=%02x, but not know how to handle\n",mOpcode,mPC-1, Peek16Debug(0), Peek16Debug(0xa), Peek16Debug(0xd));
    }
}

// Answers value of the Processor Status register
int PS()
{
    unsigned char ps = 0x20;
    if (mN) ps |= 0x80;
    if (mV) ps |= 0x40;
    if (mB) ps |= 0x10;
    if (mD) ps |= 0x08;
    if (mI) ps |= 0x04;
    if (mZ) ps |= 0x02;
    if (mC) ps |= 0x01;
    return ps;
}


// Change the processor flags to correspond to the given value
void setPS(int ps)
{
    mN = ps & 0x80;
    mV = ps & 0x40;
    mB = ps & 0x10;
    mD = ps & 0x08;
    mI = ps & 0x04;
    mZ = ps & 0x02;
    mC = ps & 0x01;
}

#endif // HANDYPSP
