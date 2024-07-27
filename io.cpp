#include "comm.h"
#include "mem.h"
#include "state.h"
#include "io.h"
#include "nand.h"
#include "dsp.h"

extern nc1020_states_t nc1020_states;

Dsp dsp;

extern unsigned char test_data[];
extern unsigned char test_data[];
extern unsigned char test_data[];


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

struct BeeperSignal{
    long long cycle;
    int value;
}last_beeper{0};

//deque<BeeperSignal> beeper_signal;
//long long last_beeper_cycle=0;
deque<signed short> sound_stream;
void init_io(){
    for (uint32_t i=0; i<IO_LIMIT; i++) {
		io_read[i] = ReadXX;
		io_write[i] = WriteXX;
	}
	io_read[0x06] = Read06;
	io_read[0x3B] = Read3B;
	io_read[0x3F] = Read3F;
	io_write[0x00] = Write00;
	io_write[0x05] = Write05;
	io_write[0x06] = Write06;
	io_write[0x08] = Write08;
	io_write[0x09] = Write09;
	io_write[0x0A] = Write0A;
	io_write[0x0D] = Write0D;
	io_write[0x0F] = Write0F;
	io_write[0x20] = Write20;
	//io_write[0x23] = Write23;
	io_write[0x3F] = Write3F;

}

uint8_t IO_API ReadXX(uint8_t addr){
	if(addr==0x18&&0){
		return ram_io[0x18]&0xbc;
	}
	if(addr==0x29) {
        return read_nand();
	}

    //printf("read unknow IO %02x ,value=%02x\n",addr, ram_io[addr]);

    if(addr==0x03){
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

void manipulate_beeper(int a){
            long long current_cycle=nc1020_states.previous_cycles+nc1020_states.cycles;
            long long samples_start=last_beeper.cycle*AUDIO_HZ/CYCLES_SECOND;
            long long samples_end=current_cycle*AUDIO_HZ/CYCLES_SECOND;
            //printf("%lld, %d  %lld %lld\n",current_cycle -last_beeper.cycle, nc1020_states.cycles, samples_start,samples_end);
            last_beeper.cycle=current_cycle;

            for(int i=0;i<(samples_end-samples_start);i++){
                sound_stream.push_back(8000*last_beeper.value);
            }
            last_beeper.value=a;
}

void manipulate_beeper2(){
    long long current_cycle=nc1020_states.previous_cycles+nc1020_states.cycles;
   // printf("<%lld>\n",current_cycle);
    manipulate_beeper(last_beeper.value);
}
void IO_API WriteXX(uint8_t addr, uint8_t value){
	if(addr==0x29) {
        return nand_write(value);
    }

    if(addr==0x30){
        if (value==0x80 || value==0x40){
            dsp.reset();
        }
    }

    if(addr==0x18){
        int a= value>>7;
        if(a==0) a=-1;

        if (a!=last_beeper.value){
            long long current_cycle=nc1020_states.previous_cycles+nc1020_states.cycles;
            //printf("%lld %lld, %d!!!!!!!!!!!\n",current_cycle, last_beeper.cycle, a);
        }
        if(true)
        {
            manipulate_beeper(a);
        }
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
    
    if(addr==0x32) {
      //fprintf(stderr,"0x%02x,",value);
      //printf("<w %02x>",value);
      //return;
    }
    else if(addr==0x33){
      //fprintf(stderr,"0x%02x,\n",value);
      //printf("[w %02x]\n",value);
      extern string udp_msg;
      dsp.write(value, ram_io[0x32]);
      if(value==0x14) {
        //udp_msg="dump 0280 100";
      }
      //return;
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



