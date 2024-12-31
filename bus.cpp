#include "bus.h"
#include <memory.h>
#include <stdio.h>
#include "ansi/c6502.h"
#include "comm.h"
#include "mem.h"
#include "state.h"
#include "nand.h"
#include "NekoDriverIO.h"
#include "io.h"
extern nc1020_states_t nc1020_states;
extern Dsp dsp;

typedef long long __int64;

static int keyMap[] = {
        0x701, 0x601, 0x602, 0x604, 0x608, 0x610, 0x620, 0x680, 0x702, 0x704,
        0x000, 0x000, 0x000, 0x001, 0x002, 0x640, 0x004, 0x008, 0x010, 0x020,
        0x201, 0x202, 0x204, 0x208, 0x210, 0x220, 0x240, 0x280, 0x101, 0x110,
        0x301, 0x302, 0x304, 0x308, 0x310, 0x320, 0x340, 0x380, 0x102, 0x120,
        0x401, 0x402, 0x404, 0x408, 0x410, 0x420, 0x440, 0x480, 0x104, 0x140,
        0x501, 0x502, 0x504, 0x508, 0x510, 0x520, 0x540, 0x580, 0x108, 0x180
};


__int64 recTick=0,lastDac=0,recTotal=0;

BusPC1000::BusPC1000() {
	///////////sound = 0;
    ioReg=nc1020_states.ram_io;
	musicEnable = true;

	dsp= &::dsp;
	dsp->reset();
	reset();
	for (int i=0;i<8;i++)
		key_posi[i]=0;
}

void BusPC1000::reset() {
	tm0v=tm0r=tm1v=0;
	timer0run=timer1run=false;
	tmaValue=tmaReload=0;
	dspSleep=isPlayMusic=false;
	dspData=0;
	musicSample = 0;

	recTick=lastDac=recTotal=0;
}

void BusPC1000::warmReset() {
	recTick=lastDac=recTotal=0;
    ioReg[IO_TIMER0_VAL] = 1;
}

void BusPC1000::getInfo(char info[]) {
	sprintf(info,"BANK:%02X BSW:%01X INT:%02X",ioReg[IO_BANK_SWITCH],ioReg[IO_BIOS_BSW]&0xf,ioReg[IO_INT_STATUS]);
}

int BusPC1000::read(int address) {
    uint8_t Load(uint16_t addr);
    return Load(address);
}

void BusPC1000::write(int address, int value) {
    void Store(uint16_t addr, uint8_t value);
    Store(address,value);
}

uint8_t IO_API Read3B(uint8_t addr);
uint8_t IO_API Read3F(uint8_t addr);

int BusPC1000::in(int address) {
    if(nc1020mode||nc2000mode||nc3000mode){
        if(address==0x08){
            return ReadPort0(address);
        }
        if(address==0x09){
            return ReadPort1(address);
        }
        if(address==0x18){
            return Read18Port4(address);//not important? seems like hotlink only
        }
        if(address==0x3b){
            return Read3B(address);
        }
        if(address==0x3f){
            return Read3F(address);
        }

    }
    if(nc3000mode){
        if(address==0x39) {
            return read_nand();
        }
        if(address==0x1e){
            return ReadPort6EXP(address);
        }
    } 
    if(nc2000mode){
        if(address==0x29) {
            return read_nand();
        }
    }
    if(pc1000mode){
        switch(address){
            case IO_PORT3://0x0b   //keyboard handling inside
                return readPort3();
            case IO_GENERAL_STATUS://0xc
                return record();
            case IO_DSP_STAT://0x20
                return dspStat();
            case IO_DSP_RET_DATA://0x21
                return dspRetData();
        }
    }
    switch (address) {
        case IO_INT_STATUS://0x01
        {
            int t;
            t = ioReg[IO_INT_STATUS];
            ioReg[IO_INT_STATUS] &= 0xc0;
            return t;
        }
        case IO_START_TIMER0://0x05
            timer0run = true;
            return (int) tm0v;
        case IO_STOP_TIMER0://0x04
            timer0run = false;
            return (int) tm0v;
        case IO_START_TIMER1://0x07
            timer1run = true;
            return (int) tm1v;
        case IO_STOP_TIMER1://0x06
            timer1run = false;
            return (int) tm1v;
        default:
            return ioReg[address];
    }
}

extern unsigned short lcdbuffaddr;
extern unsigned short lcdbuffaddrmask;
void Write23(uint8_t addr, uint8_t value);
void Write3F(uint8_t addr, uint8_t value);
void BusPC1000::out(int address, int value) {
    if(nc3000mode){
        if(address==0x39) {
            return nand_write(value);
        } 
    }
    if(nc2000mode) {
        if(address==0x29) {
            return nand_write(value);
        }
    }
    if(nc1020mode||nc2000mode||nc3000mode){
        if(address==0x04){
            return Write04GeneralCtrl(address,value);
        }
        if(address==0x05){
            return Write05ClockCtrl(address, value);
        }
        if(address==0x06){
            return Write06LCDStartAddr(address, value);
        }
        if(address==0x07){
            return Write07PortConfig(address,value);//not important? seems like only hotlink inside
        }
        if(address==0x08){
            return Write08Port0(address, value);
        }
        if(address==0x09){
            return Write09Port1(address,value);
        }
        if(address==0x0b){
            return Write0BPort3LCDStartAddr(address,value);
        }
        if(address==0x0c){
            return Write0CTimer01Control(address,value);
        }
        if(address==0x0d){
            ioReg[0x0d] = value;
            super_switch();
            return;
        }
        if(address==0x0f){
            return Write0F(address,value);
        }
        if(address==0x15){
            return Write15Dir1(address, value);
        }
        if(address==0x18){
            return Write18Port4(address, value);
        }
        if(address==0x19){
            return Write19CkvSelect(address, value);//not important? seems like only hotlink inside
        }
        if(address==0x20){
            return Write20JG(address, value);
        }
        if(address==0x23){
            return Write23(address,value);
        }
        if(address==0x3f){
            return Write3F(address,value);
        }
    }

    if(pc1000mode){
        switch(address){
            case IO_PORT_CONFIG://0x07
            case IO_CTV_SELECT://0x19
                ioReg[address] = value;
                checkPlay();
                return;
            case IO_PORT0://0x08
                ioReg[IO_PORT0] = value;
                ioReg[O_PORT0] = value;
                return;
            case IO_PORT1://0x09
                ioReg[IO_PORT1] = value;
                read_key();
                return;
            case IO_DAC_DATA://0x0e
                ioReg[IO_DAC_DATA] = value;
                {
                    __int64 t=cpu->getTotalCycles();
                    if (t-lastDac>3686400) {
                        if (lastDac)
                            recTotal+=lastDac-recTick;
                        recTick=t;
                    }
                    lastDac=t;
                }
                return;
            case IO_ZP_BSW://0x0f
                ioReg[IO_ZP_BSW] = value;
                super_switch();
                /////////zpBankSwitch();
                return;
            case IO_PORT4://0x18
                ioReg[IO_PORT4] = value;
                if (isPlayMusic) {
                    musicSample = (value & 0x80) == 0 ? -1 : 1;
                }
                {
                    int a=value &0x80;
                    if (a==0) a=-1;
                    void beeper_on_io_write(int);
                    beeper_on_io_write(a);
                }
                return;
            case IO_DSP_STAT://0x20
                if (value == DSP_RESET_FLAG || value == DSP_WAKEUP_FLAG) {
                    dspSleep = false;
                    dsp->reset();
                }
                return;
            case IO_DSP_DATA_HI://0x23
                ioReg[IO_DSP_DATA_HI] = value;
                dspCmd(ioReg[IO_DSP_DATA_HI] * 256 + ioReg[IO_DSP_DATA_LOW]);
                dsp->write(value,ioReg[IO_DSP_DATA_LOW]);
                return;

        }
    }

    switch (address) {
        case IO_BANK_SWITCH://0x00
            ioReg[IO_BANK_SWITCH] = value;
            super_switch();
            /////////////bankSwitch();
            return;
        case IO_INT_ENABLE://0x01
            ioReg[O_INT_ENABLE] = value;
            return;
        case IO_BIOS_BSW://0x0a
            ioReg[IO_BIOS_BSW] = value;
            super_switch();
            /////////////biosBankSwitch();
            /////////////bankSwitch();
            return;
        case IO_TIMER0_VAL://0x02
            ioReg[IO_TIMER0_VAL] = value;
            tm0v = value + tm0r;
			if (tm0v > 255) {
                tm0v = 255;
			}
            tm0r = 0;
            return;
        case IO_TIMER1_VAL://0x03
            ioReg[IO_TIMER1_VAL] = value;
            tm1v = value;
            return;
        case IO_TIMERA_VAL_L://0x10
            tmaReload = (tmaReload & 0xff00) | value;
            return;
        case IO_TIMERA_VAL_H://0x11
            tmaReload = (tmaReload & 0xff) | (value << 8);
            tmaValue = tmaReload;
            return;

        default:
            ioReg[address] = value;
    }
}

void BusPC1000::playSample() {
	/*************
    if (musicEnable && isPlayMusic && sound) {
		sound->write(musicSample * 8192);
    }*/
}

void BusPC1000::checkPlay() {
    boolean isPlay = (ioReg[IO_PORT_CONFIG] & 0x80) != 0 && (ioReg[IO_CTV_SELECT] & 0x80) == 0;
    if (isPlayMusic != isPlay) {
        isPlayMusic = isPlay;
        if (musicEnable) {
            if (isPlayMusic) {
                /***********
                if (sound) {
                    sound->play();
                }*/
            } else {
                /*************** 
                if (sound)
                    sound->stop();
                */
            }
        }
    }
}

void BusPC1000::setTimer() {
    int temp = ioReg[IO_TIMERAB_CTRL] >> 4;
    if (temp != 0) {
        tmaValue += (256 >> temp);
        if (tmaValue >= 0x10000) {
            tmaValue = tmaReload;
            if ((ioReg[O_INT_ENABLE] & 1) != 0)
                ioReg[IO_INT_STATUS] |= 1;
        }
    }
}

boolean BusPC1000::setTimer0() {
    if (timer0run) {
        tm0v += 64;
        if (tm0v >= 256) {
            tm0v -= 256;
            tm0r = tm0v;
            return true;
        }
    }
    return false;
}

boolean BusPC1000::setTimer1() {
    if (timer1run) {
        tm1v += 0.5;
        if (tm1v >= 256) {
            tm1v = 0;
            return true;
        }
    }
    return false;
}

void BusPC1000::setIrqTimeBase() {
    ioReg[IO_INT_STATUS] |= INT_TIME_BASE;
}

void BusPC1000::setIrqTimer0() {
    ioReg[IO_INT_STATUS] |= INT_TIME0;
}

void BusPC1000::setIrqTimer1() {
    ioReg[IO_INT_STATUS] |= INT_TIME1;
}

int BusPC1000::dspStat() {
    int value = 0;
    if (dspSleep)
        value |= DSP_SLEEP_FLAG;
    /*********** 
	if (!dspSleep && sound->busy())
		value |= 0x30;*/
    bool sound_busy(void);
    if(!dspSleep && sound_busy()){
        value |= 0x30;
    }
	value |= 0x40;
    return value;
}

int BusPC1000::dspRetData() {
	int ret=dspData;
	if (ret==0x5a)
		dspData=0xff;
	else
		dspData=0;
	return ret;
}

void BusPC1000::dspCmd(int cmd) {
    //Global.log("DSP_CMD " + Integer.toHexString(cmd));
    switch (cmd) {
        case 0x8000: //SLEEP
            dspSleep = true;
            break;
		case 0xd001:
			dspData = 0x5a;
			break;
    }
}

boolean BusPC1000::keyDown(int x, int y) {
    if (x < 10 && y < 6) {
        int v = keyMap[y * 10 + x];
        if (v != 0) {
            key_posi[v >> 8] = v & 0xff;
            if ((((v >> 8) == 6 && (v & 0x7f) != 0) || v == 0x701) && ioReg[IO_CLOCK_CTRL] == 0xf0) {
                return false;
            }
        }
    }
    return true;
}

boolean BusPC1000::keyDown2(int y,int x) {
    assert(y>=0 &&y<8);
    assert(x>=0 &&x<8);
    int v = (7-y)*256;
    v+=1<<x;
    //printf("keycode= %04x\n",v);
    if (v != 0) {
        key_posi[v >> 8] = v & 0xff;
        if ((((v >> 8) == 6 && (v & 0x7f) != 0) || v == 0x701) && ioReg[IO_CLOCK_CTRL] == 0xf0) {
            return false;
        }
    }
    return true;
}

void BusPC1000::keyUp() {
    for (int i = 0; i < 8; i++) {
        key_posi[i] = 0;
    }
}

int BusPC1000::readPort3() {
    int p0Dir = ioReg[IO_ZP_BSW] & 0xf0;
	int p1Dir = ioReg[IO_PORT1_DIR] & 3;
    int b0 = 1;
    if (p0Dir == 0xf0) {
        //port0是输出
        int port0 = ioReg[O_PORT0];
        switch (port0) {
            case 0xfe: //字典
                b0 = ((key_posi[6] & 1) != 0) ? 0 : 1;
                break;
            case 0xfd: //名片
                b0 = ((key_posi[6] & 2) != 0) ? 0 : 1;
                break;
            case 0xfb: //计算
                b0 = ((key_posi[6] & 4) != 0) ? 0 : 1;
                break;
            case 0xf7: //提醒（测验）
                b0 = ((key_posi[6] & 8) != 0) ? 0 : 1;
                break;
            case 0xef: //资料（游戏）
                b0 = ((key_posi[6] & 0x10) != 0) ? 0 : 1;
                break;
            case 0xdf: //时间
                b0 = ((key_posi[6] & 0x20) != 0) ? 0 : 1;
                break;
            case 0xbf: //网络
                b0 = ((key_posi[6] & 0x40) != 0) ? 0 : 1;
                break;
			case 0x7f: //放音
                b0 = ((key_posi[6] & 0x80) != 0) ? 0 : 1;
                break;
            case 0xff: //开机
                b0 = ((key_posi[7] & 1) != 0) ? 0 : 1;
                break;
            case 0: //检测所有可开机键
                b0 = ((key_posi[6] & 0xff) != 0 || (key_posi[7] & 1) != 0) ? 0 : 1;
                break;
		}
    }
	if (b0 == 1 && p1Dir == 3) {
		if (ioReg[IO_PORT1] == 0xfe) {
			b0 = ((key_posi[7] & 2) != 0) ? 0 : 1;
		} else if (ioReg[IO_PORT1] == 0xfd) {
			b0 = ((key_posi[7] & 4) != 0) ? 0 : 1;
		} else if ((ioReg[IO_PORT1] & 3) == 0) {
			b0 = ((key_posi[7] & 6) != 0) ? 0 : 1;
		}
	}
    return (ioReg[IO_PORT3] & 0xfe) | b0;
}

int BusPC1000::read_key() {
	int value = ioReg[IO_PORT1];
    switch (value) {
        case 0x80:
            ioReg[IO_PORT0] = key_posi[0];
            break;
        case 0x40:
            ioReg[IO_PORT0] = key_posi[1];
            break;
        case 0x20:
            ioReg[IO_PORT0] = key_posi[2];
            break;
        case 0x10:
            ioReg[IO_PORT0] = key_posi[3];
            break;
        case 8:
            ioReg[IO_PORT0] = key_posi[4];
            break;
        case 4:
            ioReg[IO_PORT0] = key_posi[5];
            break;
    }
	/*int value = 0;
	if ((ioReg[IO_PORT1_DIR] & 0xfc) != 0) {
		if ((ioReg[IO_PORT1]&0x80) && (ioReg[IO_PORT1_DIR]&0x80))
			value|=key_posi[0];
		if ((ioReg[IO_PORT1]&0x40) && (ioReg[IO_PORT1_DIR]&0x40))
			value|=key_posi[1];
		if ((ioReg[IO_PORT1]&0x20) && (ioReg[IO_PORT1_DIR]&0x20))
			value|=key_posi[2];
		if ((ioReg[IO_PORT1]&0x10) && (ioReg[IO_PORT1_DIR]&0x10))
			value|=key_posi[3];
		if ((ioReg[IO_PORT1]&0x8) && (ioReg[IO_PORT1_DIR]&0x8))
			value|=key_posi[4];
		if ((ioReg[IO_PORT1]&0x4) && (ioReg[IO_PORT1_DIR]&0x4))
			value|=key_posi[5];
		if(ioReg[IO_PORT1]&0xfc)
		ioReg[IO_PORT0]=value;
	} else {
		return ioReg[IO_PORT0];
	}*/
	return value;
}

bool initRecord=false;
int reclen;
byte* recfile;

int BusPC1000::record() {
	if (!initRecord) {
		initRecord=true;
		//////////////////recfile=loadBin(IDR_RECWAV,reclen);
        reclen=10000;////////////hack
        recfile=new byte[reclen];/////////////////hack
	}
	int v = recfile[((cpu->getTotalCycles() - recTick + recTotal) / 12 * 8000 / 3686400) % reclen];
	if (ioReg[IO_DAC_DATA]>v)
		v=0x80;
	else
		v=0;
    return (ioReg[IO_GENERAL_STATUS] & 0x7f) | v;
}

boolean BusPC1000::nmiEnable() {
    return (ioReg[O_INT_ENABLE] & 0x10) == 0;
}

boolean BusPC1000::timeBaseEnable() {
    return (ioReg[O_INT_ENABLE] & 8) == 0 && (ioReg[IO_GENERAL_CTRL] & 0xf) > 0;
}
