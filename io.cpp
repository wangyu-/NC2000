#include "comm.h"
#include "mem.h"
#include "state.h"
#include "io.h"
#include "nand.h"
#include "sound.h"
#include "NekoDriverIO.h"

extern nc1020_states_t nc1020_states;

static bool& slept = nc1020_states.slept;
static bool& should_wake_up = nc1020_states.should_wake_up;

static uint8_t* keypad_matrix = nc1020_states.keypad_matrix;
static uint32_t& lcd_addr = nc1020_states.lcd_addr;

static uint8_t* jg_wav_buff = nc1020_states.jg_wav_data;
static uint8_t& jg_wav_flags = nc1020_states.jg_wav_flags;
static uint8_t& jg_wav_index = nc1020_states.jg_wav_idx;
static bool& jg_wav_playing = nc1020_states.jg_wav_playing;

io_read_func_t io_read[0x40];
io_write_func_t io_write[0x40];

void init_io(){
    for (uint32_t i=0; i<IO_LIMIT; i++) {
		io_read[i] = ReadXX;
		io_write[i] = WriteXX;
	}
    
    // 0x29 has special handle in readXX
    //io_read[0x00] = Read00BankSwitch;  //Read00BankSwitch is noop
    io_read[0x01] = Read01IntStatus;
    io_read[0x04] = Read04StopTimer0;
    io_read[0x05] = Read05StartTimer0;
    //io_read[0x06] = Read06; //nothing special
    io_read[0x06] = Read06StopTimer1;
    io_read[0x07] = Read07StartTimer1;
    io_read[0x08] = ReadPort0;
    io_read[0x09] = ReadPort1;
    io_read[0x18] = Read18Port4;
    io_read[0x3B] = Read3B;//<----------from nc1020
    io_read[0x3F] = Read3F;//<----------from nc1020

    //0x29 30 32 33 has special handle
    io_write[0x00] = Write00;///////don't use Write00BankSwitch here
    io_write[0x01] = Write01IntEnable;
    io_write[0x04] = Write04GeneralCtrl;
    //io_write[0x05] = Write05; // clk, sleep related
    io_write[0x05] = Write05ClockCtrl;
	io_write[0x06] = Write06; // lcd related ; a wrapper of Write06LCDStartAddr
    io_write[0x07] = Write07PortConfig;
    //io_write[0x08] = Write08; // keyboard related
	//io_write[0x09] = Write09; // keyboard related
    io_write[0x08] = Write08Port0; // keyboard related
	io_write[0x09] = Write09Port1; // keyboard related
    io_write[0x0A] = Write0A;///////don't use Write0AROABBS here
    io_write[0x0B] = Write0BPort3LCDStartAddr;
    io_write[0x0C] = Write0CTimer01Control;
    io_write[0x0D] = Write0D;///////don't use Write0DVolumeIDLCDSegCtrl here (lcdwidth is local and not used at all)
	io_write[0x0F] = Write0F;/////// WriteZeroPageBankswitch merged in
    io_write[0x15] = Write15Dir1;
    io_write[0x18] = Write18Port4;
    io_write[0x19] = Write19CkvSelect;
    //io_write[0x20] = Write20;
    io_write[0x20] =Write20JG;
	io_write[0x23] = Write23;//<----------from nc1020
	io_write[0x3F] = Write3F;//<----------from nc1020




}

uint8_t IO_API ReadXX(uint8_t addr){
	if(addr==0x29) {
        return read_nand();
	}

    //printf("read unknow IO %02x ,value=%02x\n",addr, ram_io[addr]);

    //introduced in fa33ed9cb, forgot the reason
    //seems like it's for debug hgp browser
    if(addr==0x03&&false){
        return 0xff;
    }
	return ram_io[addr];
}


uint8_t IO_API Read06(uint8_t addr){
	return ram_io[addr];
}

uint8_t IO_API Read3B(uint8_t addr){
    if (!(ram_io[0x3D] & 0x03)) {
        return nc1020_states.clock_buff[0x3B] & 0xFE;
    }
    return ram_io[addr];
}

uint8_t IO_API Read3F(uint8_t addr){
    uint8_t idx = ram_io[0x3E];
    return idx < 80 ? nc1020_states.clock_buff[idx] : 0;
}

void IO_API WriteXX(uint8_t addr, uint8_t value){
	if(addr==0x29) {
        return nand_write(value);
    }

    if(nc2000){
        if(addr==0x30){
            if (value==0x80 || value==0x40){
                reset_dsp();
            }
        }
    }
    if(nc1020){
        if(addr==0x20){
            if (value==0x80 || value==0x40){
                reset_dsp();
            }
        }
    }

    if(addr==0x18&& false){
        int a= value>>7;
        if(a==0) a=-1;
        beeper_on_io_write(a);
        /*if(beeper_signal.empty()||a!=beeper_signal.back().value) {
            printf("[beeper %d, at %lld]\n",a,nc1020_states.previous_cycles+ nc1020_states.cycles);
            beeper_signal.push_back({nc1020_states.previous_cycles+ nc1020_states.cycles, a});
        }*/
        //printf("<write 0x18 %02x>\n",value);
    }
    
    /*if(addr>=0x30 && addr<=0x3a){
      printf("{z %04x %02x}\n",addr,value);
      return; 
    }*/
    

    if(nc2000){
        if(addr==0x32) {
        //fprintf(stderr,"0x%02x,",value);
        //printf("<w %02x>",value);
        //return;
        }
        if(addr==0x33){
        //fprintf(stderr,"0x%02x,\n",value);
        //printf("[w %02x]\n",value);
        extern string udp_msg;
        write_data_to_dsp(value, ram_io[0x32]);
        if(value==0x14) {
            //udp_msg="dump 0280 100";
        }
        //return;
        } 
    }
    if(nc1020){
        if(addr==0x23){
        //fprintf(stderr,"0x%02x,\n",value);
        //printf("[w %02x]\n",value);
        extern string udp_msg;
        write_data_to_dsp(value, ram_io[0x22]);
        }
    }

    //printf("write unknow IO %02x ,value=%02x\n",addr, value);

    ram_io[addr] = value;
}


// switch bank.
void IO_API Write00(uint8_t addr, uint8_t value){
    uint8_t old_value = ram_io[addr];
    ram_io[addr] = value;
	/*
    if (value != old_value) {
    	SwitchBank();
    }*/
	super_switch();
}

void IO_API Write05(uint8_t addr, uint8_t value){
	uint8_t old_value = ram_io[addr];
	ram_io[addr] = value;
	if ((old_value ^ value) & 0x08) {
		slept = !(value & 0x08);
	}
}


void IO_API Write06(uint8_t addr, uint8_t value){
    if(false){
        ram_io[addr] = value;
        if (!lcd_addr||true) {
            lcd_addr = ((ram_io[0x0C] & 0x03) << 12) | (value << 4);
            printf("lcd_addr=%x\n",lcd_addr);
            if(lcd_addr==0x1380){
                nc1020_states.grey_mode=1;
            }else{
                nc1020_states.grey_mode=0;
            }
        }
        ram_io[0x09] &= 0xFE;
    }
    Write06LCDStartAddr(addr,value);
    extern unsigned short lcdbuffaddr;
    lcd_addr = lcdbuffaddr;
    lcd_addr = 0x1380;

}

void IO_API Write08(uint8_t addr, uint8_t value){
    ram_io[addr] = value;
    ram_io[0x0B] &= 0xFE;
}

// keypad matrix.
void IO_API Write09(uint8_t addr, uint8_t value){
    ram_io[addr] = value;
    switch (value){
    case 0x01: ram_io[0x08] = keypad_matrix[0]; break;
    case 0x02: ram_io[0x08] = keypad_matrix[1]; break;
    case 0x04: ram_io[0x08] = keypad_matrix[2]; break;
    case 0x08: ram_io[0x08] = keypad_matrix[3]; break;
    case 0x10: ram_io[0x08] = keypad_matrix[4]; break;
    case 0x20: ram_io[0x08] = keypad_matrix[5]; break;
    case 0x40: ram_io[0x08] = keypad_matrix[6]; break;
    case 0x80: ram_io[0x08] = keypad_matrix[7]; break;
    case 0:
        ram_io[0x0B] |= 1;
        if (keypad_matrix[7] == 0xFE) {
            ram_io[0x0B] &= 0xFE;
        }
        break;
    case 0x7F:
        if (ram_io[0x15] == 0x7F) {
            ram_io[0x08] = (
                keypad_matrix[0] |
                keypad_matrix[1] |
                keypad_matrix[2] |
                keypad_matrix[3] |
                keypad_matrix[4] |
                keypad_matrix[5] |
                keypad_matrix[6] |
                keypad_matrix[7]
            );
        }
        break;
    }
}

// roabbs
void IO_API Write0A(uint8_t addr, uint8_t value){
    uint8_t old_value = ram_io[addr];
    ram_io[addr] = value;
	/*
    if (value != old_value) {
        memmap[6] = bbs_pages[value & 0x0F];
    }*/
	super_switch();
}

// switch volume
void IO_API Write0D(uint8_t addr, uint8_t value){
	uint8_t old_value = ram_io[addr];
    ram_io[addr] = value;
	/*
    if (value != old_value) {
        SwitchVolume();
    }*/
	super_switch();
}

// zp40 switch
void IO_API Write0F(uint8_t addr, uint8_t value){
	uint8_t old_value = ram_io[addr];
    ram_io[addr] = value;
    //old_value &= 0x07;
    //value &= 0x07;
	/*
    if (value != old_value) {
        uint8_t* ptr_new = GetPtr40(value);
        if (old_value) {
            memcpy(GetPtr40(old_value), ram_40, 0x40);
            memcpy(ram_40, value ? ptr_new : bak_40, 0x40);
        } else {
            memcpy(bak_40, ram_40, 0x40);
            memcpy(ram_40, ptr_new, 0x40);
        }
    }*/
	super_switch();

    //wayback
    rw0f_b4_DIR00 = (value & 0x10) != 0;
    rw0f_b5_DIR01 = (value & 0x20) != 0;
    rw0f_b6_DIR023 = (value & 0x40) != 0;
    rw0f_b7_DIR047 = (value & 0x80) != 0;
}

void IO_API Write20(uint8_t addr, uint8_t value){
    ram_io[addr] = value;
    if (value == 0x80 || value == 0x40) {
        memset(jg_wav_buff, 0, 0x20);
        ram_io[0x20] = 0;
        jg_wav_flags = 1;
        jg_wav_index = 0;
    }
}

void GenerateAndPlayJGWav(){
}

void IO_API Write23(uint8_t addr, uint8_t value){
    ram_io[addr] = value;
    if (value == 0xC2) {
        jg_wav_buff[jg_wav_index] = ram_io[0x22];
    } else if (value == 0xC4) {
        if (jg_wav_index < 0x20) {
            jg_wav_buff[jg_wav_index] = ram_io[0x22];
            jg_wav_index ++;
        }
    } else if (value == 0x80) {
        ram_io[0x20] = 0x80;
        jg_wav_flags = 0;
        if (jg_wav_index) {
            if (!jg_wav_playing) {
                GenerateAndPlayJGWav();
                jg_wav_index = 0;
            }
        }
    }
    if (jg_wav_playing) {
        // todo.
    }
}

// clock.
void IO_API Write3F(uint8_t addr, uint8_t value){
    uint8_t* clock_buff = nc1020_states.clock_buff;
	uint8_t& clock_flags = nc1020_states.clock_flags;
    ram_io[addr] = value;
    uint8_t idx = ram_io[0x3E];
    if (idx >= 0x07) {
        if (idx == 0x0B) {
            ram_io[0x3D] = 0xF8;
            clock_flags |= value & 0x07;
            clock_buff[0x0B] = value ^ ((clock_buff[0x0B] ^ value) & 0x7F);
        } else if (idx == 0x0A) {
            clock_flags |= value & 0x07;
            clock_buff[0x0A] = value;
        } else {
            clock_buff[idx % 80] = value;
        }
    } else {
        if (!(clock_buff[0x0B] & 0x80) && idx < 80) {
            clock_buff[idx] = value;
        }
    }
}



