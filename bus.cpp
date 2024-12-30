#include "bus.h"
#include <memory.h>
#include <stdio.h>
#include "ansi/c6502.h"

typedef long long __int64;

static int keyMap[] = {
        0x701, 0x601, 0x602, 0x604, 0x608, 0x610, 0x620, 0x680, 0x702, 0x704,
        0x000, 0x000, 0x000, 0x001, 0x002, 0x640, 0x004, 0x008, 0x010, 0x020,
        0x201, 0x202, 0x204, 0x208, 0x210, 0x220, 0x240, 0x280, 0x101, 0x110,
        0x301, 0x302, 0x304, 0x308, 0x310, 0x320, 0x340, 0x380, 0x102, 0x120,
        0x401, 0x402, 0x404, 0x408, 0x410, 0x420, 0x440, 0x480, 0x104, 0x140,
        0x501, 0x502, 0x504, 0x508, 0x510, 0x520, 0x540, 0x580, 0x108, 0x180
};

#define BANK_SIZE 0x2000

__int64 recTick=0,lastDac=0,recTotal=0;

BusPC1000::BusPC1000() {
	///////////sound = 0;
	musicEnable = true;
	//////////dsp=new Dsp();
	//////////dsp->reset();
    ram = new byte[8*0x2000];
    xrom = new byte[0x800*0x2000]; //外部rom，16MB
    flash = new byte[64*0x2000]; //flash，512KB
	reset();
	for (int i=0;i<8;i++)
		key_posi[i]=0;
    flashModify = false;
}

void BusPC1000::reset() {
	tm0v=tm0r=tm1v=0;
	timer0run=timer1run=false;
	tmaValue=tmaReload=0;
	dspSleep=isPlayMusic=false;
	dspData=0;
	sstId=sstWrite=sstErase=0;
	musicSample = 0;
	for (int i=0;i<0x80;i++)
		ioReg[i]=0;
	memset(ram,0,8*0x2000);
	bank[0] = ram;
    bank[1] = ram+BANK_SIZE;
    ioReg[IO_BANK_SWITCH] = 0;
    bankSwitch();
    zpBase = 0;
    bank[6] = xrom+BANK_SIZE*2;
    bank[7] = xrom+BANK_SIZE*3;
    bbsTab[0] = xrom+BANK_SIZE*2;
    bbsTab[1] = ram+BANK_SIZE*3;
    bbsTab[2] = xrom;
    bbsTab[3] = xrom+BANK_SIZE*1;
    bbsTab[4] = xrom+BANK_SIZE*6;
    bbsTab[5] = xrom+BANK_SIZE*7;
    bbsTab[6] = xrom+BANK_SIZE*4;
    bbsTab[7] = xrom+BANK_SIZE*5;
    bbsTab[8] = xrom+BANK_SIZE*10;
    bbsTab[9] = xrom+BANK_SIZE*11;
    bbsTab[10] = xrom+BANK_SIZE*8;
    bbsTab[11] = xrom+BANK_SIZE*9;
    bbsTab[12] = xrom+BANK_SIZE*14;
    bbsTab[13] = xrom+BANK_SIZE*15;
    bbsTab[14] = xrom+BANK_SIZE*12;
    bbsTab[15] = xrom+BANK_SIZE*13;
	recTick=lastDac=recTotal=0;
}

void BusPC1000::warmReset() {
	recTick=lastDac=recTotal=0;
    ioReg[IO_TIMER0_VAL] = 1;
    ram[0xcb] = -1;
    ram[0xcc] = -1;
}

void BusPC1000::loadRom(byte* b, int length, int offset) {
	for (int i = 0; i < length; i++) {
        xrom[offset] = b[i];
        offset++;
    }
}

byte* BusPC1000::loadFlash(byte* b, int length, int offset) {
	for (int i = 0; i < length; i++) {
        flash[offset] = b[i];
        offset++;
    }
	return flash;
}

byte* BusPC1000::getScreen() {
	return ram+0x9c0;
}

void BusPC1000::getInfo(char info[]) {
	sprintf(info,"BANK:%02X BSW:%01X INT:%02X",ioReg[IO_BANK_SWITCH],ioReg[IO_BIOS_BSW]&0xf,ioReg[IO_INT_STATUS]);
}

int BusPC1000::read(int address) {
	if (address < 0x40)
        return in(address);
    else if (address < 0x80)
        return ram[address + zpBase];
    else
        return bank[(address & 0xe000) >> 13][address & 0x1fff];
}

int BusPC1000::readIo(int address) {
	if (address < 0x40) {
		if (address == IO_INT_ENABLE)
			return ioReg[O_INT_ENABLE];
		if (address == IO_PORT0)
			return ioReg[O_PORT0];
        return ioReg[address];
	} else if (address < 0x80)
        return ram[address + zpBase];
    else
        return bank[(address & 0xe000) >> 13][address & 0x1fff];
}

void BusPC1000::write(int address, int value) {
	if (address < 0x40) {
        out(address, value);
    } else if (address < 0x80) {
        ram[address + zpBase] = (byte) value;
    } else if (address < 0x4000) {
        bank[address >> 13][address & 0x1fff] = (byte) value;
    } else if ((ioReg[IO_BIOS_BSW] & 0x80) != 0 && ioReg[IO_BANK_SWITCH] < 16 && address < 0xc000) {
        //写flash
        writeFlash(address, value);
    } else if (ioReg[IO_BANK_SWITCH] == 0 && address < 0x8000) {
        bank[address >> 13][address & 0x1fff] = (byte) value;
    } else if ((ioReg[IO_BIOS_BSW] & 0xf) != 1 && address >= 0xc000 && address < 0xe000) {
        //bbs1
        bank[address >> 13][address & 0x1fff] = (byte) value;
    } else {
        //Global.log("write " + Integer.toHexString(address) + " " + Integer.toHexString(value) + " " +
        //        Integer.toHexString(cpu.PC) + " " + ioReg[IO_BANK_SWITCH]);
    }
}

void BusPC1000::writeFlash(int address, int value) {
    if ((sstId == 0) && (sstWrite == 0) && (sstErase == 0) && (address == 0x5555) && (value == 0xaa)) {
        sstId++;
        sstWrite++;
        sstErase++;
    } else if ((sstId == 1) && (sstWrite == 1) && (sstErase == 1) && (address == 0xaaaa) && (value == 0x55)) {
        sstId++;
        sstErase++;
        sstWrite++;
    } else if ((sstWrite == 2) && (address == 0x5555) && (value == 0xa0)) {
        sstWrite++;
        sstId = 0;
        sstErase = 0;
    } else if (sstWrite == 3) {
        bank[address >> 13][address & 0x1fff] &= (byte) value;
        sstWrite = 0;
        flashModify = true;
	} else if ((sstErase == 2) && (address == 0x5555) && (value == 0x80)) {
		sstErase++;
		sstWrite=0;
		sstId=0;
	} else if ((sstErase == 3) && (address == 0x5555) && (value == 0xaa)) {
		sstErase++;
	} else if ((sstErase == 4) && (address == 0xaaaa) && (value == 0x55)) {
		sstErase++;
	} else if ((sstErase == 5) && (value == 0x30)) {
		memset(bank[address >> 13] + (address & 0x1000), 0xff, 0x1000);
		sstErase = 0;
        flashModify = true;
    } else {
        //Global.log("write " + Integer.toHexString(address) + " " + Integer.toHexString(value) + " " +
        //        Integer.toHexString(cpu.PC) + " " + ioReg[IO_BANK_SWITCH]);
		printf("write %x %x %x %x\n",address,value,/*cpu->pc*/0,ioReg[IO_BANK_SWITCH]);
    }
}

int BusPC1000::in(int address) {
    int t;
    switch (address) {
        case IO_INT_STATUS:
            t = ioReg[IO_INT_STATUS];
            ioReg[IO_INT_STATUS] &= 0xc0;
            return t;
        case IO_START_TIMER0:
            timer0run = true;
            return (int) tm0v;
        case IO_STOP_TIMER0:
            timer0run = false;
            return (int) tm0v;
        case IO_START_TIMER1:
            timer1run = true;
            return (int) tm1v;
        case IO_STOP_TIMER1:
            timer1run = false;
            return (int) tm1v;
        case IO_PORT3:
            return readPort3();
        case IO_DSP_STAT:
            return dspStat();
		case IO_DSP_RET_DATA:
			return dspRetData();
		case IO_GENERAL_STATUS:
			return record();
        default:
            return ioReg[address];
    }
}

void BusPC1000::out(int address, int value) {
    switch (address) {
        case IO_INT_ENABLE:
            ioReg[O_INT_ENABLE] = value;
            break;
        case IO_BANK_SWITCH:
            ioReg[IO_BANK_SWITCH] = value;
            bankSwitch();
            break;
        case IO_BIOS_BSW:
            ioReg[IO_BIOS_BSW] = value;
            biosBankSwitch();
            bankSwitch();
            break;
        case IO_ZP_BSW:
            ioReg[IO_ZP_BSW] = value;
            zpBankSwitch();
            break;
        case IO_PORT0:
            ioReg[IO_PORT0] = value;
            ioReg[O_PORT0] = value;
            break;
		case IO_PORT1:
			ioReg[IO_PORT1] = value;
			read_key();
			break;
        case IO_PORT4:
            ioReg[IO_PORT4] = value;
            if (isPlayMusic) {
                musicSample = (value & 0x80) == 0 ? -1 : 1;
            }
            break;
        case IO_PORT_CONFIG:
        case IO_CTV_SELECT:
            ioReg[address] = value;
            checkPlay();
            break;
        case IO_TIMER0_VAL:
            ioReg[IO_TIMER0_VAL] = value;
            tm0v = value + tm0r;
			if (tm0v > 255) {
                tm0v = 255;
			}
            tm0r = 0;
            break;
        case IO_TIMER1_VAL:
            ioReg[IO_TIMER1_VAL] = value;
            tm1v = value;
            break;
        case IO_TIMERA_VAL_L:
            tmaReload = (tmaReload & 0xff00) | value;
            break;
        case IO_TIMERA_VAL_H:
            tmaReload = (tmaReload & 0xff) | (value << 8);
            tmaValue = tmaReload;
            break;
        case IO_DSP_STAT:
            if (value == DSP_RESET_FLAG || value == DSP_WAKEUP_FLAG) {
                dspSleep = false;
				//////////////dsp->reset();
            }
            break;
        case IO_DSP_DATA_HI:
            ioReg[IO_DSP_DATA_HI] = value;
            dspCmd(ioReg[IO_DSP_DATA_HI] * 256 + ioReg[IO_DSP_DATA_LOW]);
			/////////////////dsp->write(value,ioReg[IO_DSP_DATA_LOW]);
            break;
		case IO_DAC_DATA:
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
			break;
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

void BusPC1000::bankSwitch() {
    int bid = ioReg[IO_BANK_SWITCH];
    if ((ioReg[IO_BIOS_BSW] & 0x80) == 0) {
        //b7 :  ROA   : 4000-BFFF ROM RAM select ( 0 for ROM; 1 for RAM )
        int base = (bid + ((ioReg[IO_LCD_SEGMENT] & 1) << 8)) << 2;
        if (bid != 0) {
            bank[2] = xrom+BANK_SIZE*(base + 2);
            bank[3] = xrom+BANK_SIZE*(base + 3);
        } else {
            bank[2] = ram+BANK_SIZE*2;
            bank[3] = ram+BANK_SIZE*3;
        }
        bank[4] = xrom+BANK_SIZE*base;
        bank[5] = xrom+BANK_SIZE*(base + 1);
    } else {
        int base = (bid & 0xf) << 2;
        /*if (bid < 2 && (ioReg[IO_LCD_SEGMENT] & 3) == 0) {
            bank[2] = ram[base + 2];
            bank[3] = ram[base + 3];
            bank[4] = ram[base];
            bank[5] = ram[base + 1];
        } else*/
        {
            bank[2] = flash+BANK_SIZE*(base + 2);
            bank[3] = flash+BANK_SIZE*(base + 3);
            bank[4] = flash+BANK_SIZE*base;
            bank[5] = flash+BANK_SIZE*(base + 1);
        }
    }
}

void BusPC1000::biosBankSwitch() {
    int value = ioReg[IO_BIOS_BSW];
    bank[6] = bbsTab[value & 0xf];
}

void BusPC1000::zpBankSwitch() {
    int value = ioReg[IO_ZP_BSW] & 7;
    int base = 0;
    if (value == 0)
        base = 0x40;
    else if (value < 4)
        base = 0;
    else
        base = 0x200 + (value - 4) * 0x40;
    zpBase = base - 0x40;
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
