#pragma once

#include "ansi/ibus6502.h"
#include "ansi/c6502.h"
#include "dsp/dsp.h"

#define final const
class BusPC1000 : public IBus6502 {
    static final int IO_BANK_SWITCH = 0;
    static final int IO_INT_ENABLE = 1;
    static final int IO_INT_STATUS = 1;
    static final int IO_TIMER0_VAL = 2;
    static final int IO_TIMER1_VAL = 3;
    static final int IO_STOP_TIMER0 = 4;
    static final int IO_GENERAL_CTRL = 4;
    static final int IO_START_TIMER0 = 5;
    static final int IO_CLOCK_CTRL = 5;
    static final int IO_STOP_TIMER1 = 6;
    static final int IO_LCD_CONFIG = 6;
    static final int IO_START_TIMER1 = 7;
    static final int IO_PORT_CONFIG = 7;
    static final int IO_PORT0 = 8;
    static final int IO_PORT1 = 9;
    static final int IO_BIOS_BSW = 0xa;
    static final int IO_PORT3 = 0xb;
    static final int IO_GENERAL_STATUS = 0xc;
    static final int IO_TIMER01_CTRL = 0xc;
    static final int IO_LCD_SEGMENT = 0xd;
    static final int IO_DAC_DATA = 0xe;
    static final int IO_ZP_BSW = 0xf;
    static final int IO_TIMERA_VAL_L = 0x10;
    static final int IO_TIMERA_VAL_H = 0x11;
    static final int IO_TIMERB_VAL_L = 0x12;
    static final int IO_TIMERB_VAL_H = 0x13;
    static final int IO_TIMERAB_CTRL = 0x14;
    static final int IO_PORT1_DIR = 0x15;
    static final int IO_PORT2_DIR = 0x16;
    static final int IO_PORT2 = 0x17;
    static final int IO_PORT4 = 0x18;
    static final int IO_CTV_SELECT = 0x19;
    static final int IO_VOLUME_SET = 0x1a;

    static final int IO_DSP_STAT = 0x20;
    static final int IO_DSP_RET_DATA = 0x21;
    static final int IO_DSP_DATA_LOW = 0x22;
    static final int IO_DSP_DATA_HI = 0x23;
    static final int DSP_WAKEUP_FLAG = 0x80;
    static final int DSP_RESET_FLAG = 0x40;
    static final int DSP_SLEEP_FLAG = 0x80;

    static final int INT_TIME1 = 0x20;
    static final int INT_TIME0 = 0x10;
    static final int INT_TIME_BASE = 8;

    static final int O_INT_ENABLE = 0x40;
    static final int O_PORT0 = 0x41;

public:
	C6502* cpu;
	Dsp* dsp;
    ////////int ioReg[0x80];
    unsigned char* ioReg;
    int tmaValue;
    int tmaReload;
    int key_posi[8];
    boolean timer0run;
    boolean timer1run;
    double tm0v;
    double tm0r;
    double tm1v;
    boolean dspSleep;
    boolean isPlayMusic;
	int dspData;
    ////////////////SoundStream* sound;
    int musicSample;
    boolean musicEnable;// = true;

    unsigned int speed_slowdown=1;

public:
	BusPC1000();
	void reset();
	void warmReset();
	int read(int address);
	void write(int address, int value);
	void setTimer();
	boolean setTimer0();
	boolean setTimer1();
	void setIrqTimeBase();
	void setIrqTimer0();
	void setIrqTimer1();
    boolean keyDown2(int x, int y);
	void keyUp();
	boolean nmiEnable();
	boolean timeBaseEnable();
	void playSample();

    int in(int address);
	void out(int address, int value);

private:
	void checkPlay();
	int dspStat();
	int dspRetData();
	void dspCmd(int cmd);
	int readPort3();
	int read_key();
	int record();
};

#undef final
