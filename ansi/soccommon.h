#ifndef SOC_COMMON_H
#define SOC_COMMON_H

#include <stdint.h>

#define __iocallconv __fastcall

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
extern void checkwatchram(WORD addr, BYTE data);

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

#endif
