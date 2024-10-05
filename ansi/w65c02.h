#ifndef _WQX_65C02_H
#define _WQX_65C02_H

/*
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include "wintypes.h"
#include <objc/objc.h>
#endif*/

typedef unsigned char byte;
typedef unsigned DWORD;
typedef unsigned char BYTE;
typedef char CHAR;
typedef unsigned short WORD;
typedef WORD* LPWORD;
typedef DWORD* LPDWORD;
typedef CHAR* LPSTR;
typedef bool BOOL;

#ifndef TRUE
#define TRUE true
#define FALSE false
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
////#define __iocallconv __fastcall
#define __iocallconv

typedef BYTE(__iocallconv *iofunction1)(BYTE);
typedef void (__iocallconv *iofunction2)(BYTE, BYTE);

typedef struct _regsrec {
    BYTE a;   // accumulator
    BYTE x;   // index X
    BYTE y;   // index Y
    BYTE ps;  // processor status
    WORD pc;  // program counter
    WORD sp;  // stack pointer
} regsrec, *regsptr;

extern iofunction1 ioread[0x40];
extern iofunction2 iowrite[0x40];
extern void checkflashprogram(WORD addr, BYTE data);

extern unsigned char fixedram0000[0x10002]; // just like simulator, 64k+2
extern unsigned char* pmemmap[8]; // 0000~1FFF ... E000~FFFF
extern unsigned char* may4000ptr; // TODO: move into NekoDriver.h
extern unsigned char* norbankheader[0x10];
extern unsigned char* volume0array[0x100];
extern unsigned char* volume1array[0x100];
#ifdef USE_BUSROM
extern unsigned char* volume2array[0x100];
extern unsigned char* volume3array[0x100];
#endif
extern unsigned char* bbsbankheader[0x10];
extern unsigned char* zp40ptr;  // used in io_zp_bsw

#define iorange 0x40
extern regsrec    regs;
extern BOOL       restart;
extern BOOL       g_irq;    // FIXME: NO MORE REVERSE
extern BOOL       g_nmi;    // FIXME: NO MORE REVERSE
extern BOOL       g_wai, g_wai_saved;
extern BOOL       g_stp;



DWORD   CpuExecute();
void    CpuInitialize();
void    MemDestroy();
void    MemInitialize();
void    MemReset();



unsigned char GetByte(unsigned short address);
unsigned short GetWord(unsigned short address);

//
// Status Register Bits
//
#define  AF_SIGN       0x80
#define  AF_OVERFLOW   0x40
#define  AF_RESERVED   0x20
#define  AF_BREAK      0x10
#define  AF_DECIMAL    0x08
#define  AF_INTERRUPT  0x04
#define  AF_ZERO       0x02
#define  AF_CARRY      0x01

// Handy

#ifdef TRACE_CPU

#define TRACE_CPU0(msg)                 _RPT1(_CRT_WARN,"C65C02::"msg" (Time=%012d)\n",gSystemCycleCount)
#define TRACE_CPU1(msg,arg1)            _RPT2(_CRT_WARN,"C65C02::"msg" (Time=%012d)\n",arg1,gSystemCycleCount)
#define TRACE_CPU2(msg,arg1,arg2)       _RPT3(_CRT_WARN,"C65C02::"msg" (Time=%012d)\n",arg1,arg2,gSystemCycleCount)
#define TRACE_CPU3(msg,arg1,arg2,arg3)  _RPT4(_CRT_WARN,"C65C02::"msg" (Time=%012d)\n",arg1,arg2,arg3,gSystemCycleCount)

#else

#define TRACE_CPU0(msg)
#define TRACE_CPU1(msg,arg1)
#define TRACE_CPU2(msg,arg1,arg2)
#define TRACE_CPU3(msg,arg1,arg2,arg3)

#endif


//
// Handy definitions
//

#define NMI_VECTOR  0xfffa
#define BOOT_VECTOR 0xfffc
#define IRQ_VECTOR  0xfffe

#define MAX_CPU_BREAKPOINTS 8

//
// ACCESS MACROS
//

//#define CPU_PEEK(m)               (mSystem.Peek_CPU(m))
//#define CPU_PEEKW(m)          (mSystem.PeekW_CPU(m))
//#define CPU_POKE(m1,m2)           (mSystem.Poke_CPU(m1,m2))

//#define CPU_PEEK(m)             (((m<0xfc00)?mRamPointer[m]:mSystem.Peek_CPU(m)))
//#define CPU_PEEKW(m)            (((m<0xfc00)?(mRamPointer[m]+(mRamPointer[m+1]<<8)):mSystem.PeekW_CPU(m)))
//#define CPU_POKE(m1,m2)         {if(m1<0xfc00) mRamPointer[m1]=m2; else mSystem.Poke_CPU(m1,m2);}

// Don't use ++/-- in addr, or will be execute multi time
// TODO: should place io operation in first case to prefer io speed (side effect: slown down other normal memory access)
inline uint8_t CPU_PEEK(uint16_t addr){
  uint8_t Peek16_2(uint16_t);
  return Peek16_2(addr);
  /*
    if(addr >= 0x80) {
      return *(pmemmap[unsigned(addr) >> 0xD] + (addr & 0x1FFF));
    }else{
      return (addr >= iorange?zp40ptr[addr-0x40]:ioread[addr & 0xFF]((BYTE)(addr & 0xff)));
    }*/
}

inline uint16_t CPU_PEEKW(uint16_t addr){
    return  (CPU_PEEK((addr)) + (CPU_PEEK((addr + 1)) << 8));
}

inline void CPU_POKE(uint16_t addr, uint8_t a)   
{ 
  void Store2(uint16_t addr, uint8_t value);
  Store2(addr,a);
  /*
  if ((addr >= 0x80)) { 
    if (addr < 0x4000) {
    *(pmemmap[unsigned(addr) >> 0xD] + (addr & 0x1FFF)) = (BYTE)(a);
    } else {
    checkflashprogram(addr, (BYTE)(a));
    }
  } else if ((addr >= iorange)) {
    zp40ptr[addr-0x40] = (BYTE)(a);
  }  else {
  iowrite[addr & 0xFF]((BYTE)(addr & 0xff),(BYTE)(a)); 
  }*/
}
    

enum {  illegal = 0,
        accu,
        imm,
        absl,
        zp,
        zpx,
        zpy,
        absx,
        absy,
        iabsx,
        impl,
        rel,
        zrel,
        indx,
        indy,
        iabs,
        ind
     };

//typedef struct {
//    int PS;     // Processor status register   8 bits
//    int A;      // Accumulator                 8 bits
//    int X;      // X index register            8 bits
//    int Y;      // Y index register            8 bits
//    int SP;     // Stack Pointer               8 bits
//    int Opcode; // Instruction opcode          8 bits
//    int Operand;// Instructions operand       16 bits
//    int PC;     // Program Counter            16 bits
//    bool NMI;
//    bool IRQ;
//    bool WAIT;
//#ifdef _LYNXDBG
//    int cpuBreakpoints[MAX_CPU_BREAKPOINTS];
//#endif
//} C6502_REGS;

//CSystemBase   &mSystem;

// CPU Flags & status

extern int mA;     // Accumulator                 8 bits
extern int mX;     // X index register            8 bits
extern int mY;     // Y index register            8 bits
extern int mSP;        // Stack Pointer               8 bits
extern int mOpcode;  // Instruction opcode          8 bits
extern int mOperand; // Instructions operand         16 bits
extern int mPC;        // Program Counter            16 bits

extern int mN;     // N flag for processor status register
extern int mV;     // V flag for processor status register
extern int mB;     // B flag for processor status register
extern int mD;     // D flag for processor status register
extern int mI;     // I flag for processor status register
extern int mZ;     // Z flag for processor status register
extern int mC;     // C flag for processor status register

extern int mIRQActive;

#ifdef _LYNXDBG
extern int mPcBreakpoints[MAX_CPU_BREAKPOINTS];
extern int mDbgFlag;
#endif
//UBYTE *mRamPointer;

// Associated lookup tables

extern int mBCDTable[2][256];

//
// Opcode prototypes
//

int PS();
void setPS(int ps);

#endif
