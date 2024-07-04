#include "nc1020.h"
#include <string>
#include <cassert>
#include <cstdio>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <deque>
#include "disassembler.h"
using namespace std;

bool enable_dyn_debug=false;
bool enable_debug_nand=false;
const uint32_t num_nor_pages =0x10+uint32_t(nc1020)*0x10;
//const uint32_t num_nor_pages =0x20;
const uint32_t num_rom_pages =0x300;

const bool enable_debug_switch=false;
 bool enable_debug_pc=false;
const bool enable_oops=false;
bool do_inject=false;

string inject_code;

uint64_t tick=0;
namespace wqx {
    using std::string;
    
    // cpu cycles per second (cpu freq).
    const uint32_t CYCLES_SECOND = 5120000*10; //multiple by 10, tmp solution to fix speed
    const uint32_t TIMER0_FREQ = 2;
    const uint32_t TIMER1_FREQ = 0x100;
    // cpu cycles per timer0 period (1/2 s).
    const uint32_t CYCLES_TIMER0 = CYCLES_SECOND / TIMER0_FREQ;
    // cpu cycles per timer1 period (1/256 s).
    const uint32_t CYCLES_TIMER1 = CYCLES_SECOND / TIMER1_FREQ;
    // speed up
    const uint32_t CYCLES_TIMER1_SPEED_UP = CYCLES_SECOND / TIMER1_FREQ / 20;
    // cpu cycles per ms (1/1000 s).
    const uint32_t CYCLES_MS = CYCLES_SECOND / 1000;
    
    static const uint32_t ROM_SIZE = 0x8000 * num_rom_pages;
    static const uint32_t NOR_SIZE = 0x8000 * num_nor_pages;
    
    static const uint16_t IO_LIMIT = 0x40;
#define IO_API
    typedef uint8_t (IO_API *io_read_func_t)(uint8_t);
    typedef void (IO_API *io_write_func_t)(uint8_t, uint8_t);
    
    const uint16_t NMI_VEC = 0xFFFA;
    const uint16_t RESET_VEC = 0xFFFC;
    const uint16_t IRQ_VEC = 0xFFFE;
    
    const uint32_t VERSION = 0x06;

typedef struct {
	uint16_t reg_pc;
	uint8_t reg_a;
	uint8_t reg_ps;
	uint8_t reg_x;
	uint8_t reg_y;
	uint8_t reg_sp;
} cpu_states_t;

typedef struct {
	uint32_t version;
	cpu_states_t cpu;
	uint8_t ram[0x8000];
	uint8_t ext_ram[0x8000];
	uint8_t ext_ram2[0x8000];

	uint8_t bak_40[0x40];

	uint8_t clock_data[80];
	uint8_t clock_flags;

	uint8_t jg_wav_data[0x20];
	uint8_t jg_wav_flags;
	uint8_t jg_wav_idx;
	bool jg_wav_playing;

	uint8_t fp_step;
	uint8_t fp_type;
	uint8_t fp_bank_idx;
	uint8_t fp_bak1;
	uint8_t fp_bak2;
	uint8_t fp_buff[0x100];

	bool slept;
	bool should_wake_up;
	bool pending_wake_up;
	uint8_t wake_up_flags;

	bool timer0_toggle;
	uint32_t cycles;
	uint32_t timer0_cycles;
	uint32_t timer1_cycles;
	bool should_irq;

	uint32_t lcd_addr;
	uint8_t keypad_matrix[8];
} nc1020_states_t;

static WqxRom nc1020_rom;

static uint8_t rom_buff[ROM_SIZE];
static uint8_t nor_buff[NOR_SIZE];

static uint8_t* rom_volume0[0x100];
static uint8_t* rom_volume1[0x100];
static uint8_t* rom_volume2[0x100];

static uint8_t* nor_banks[0x20];
static uint8_t* bbs_pages[0x10];

static uint8_t* memmap[8];
static nc1020_states_t nc1020_states;

static uint32_t& version = nc1020_states.version;

static uint16_t& reg_pc = nc1020_states.cpu.reg_pc;
static uint8_t& reg_a = nc1020_states.cpu.reg_a;
static uint8_t& reg_ps = nc1020_states.cpu.reg_ps;
static uint8_t& reg_x = nc1020_states.cpu.reg_x;
static uint8_t& reg_y = nc1020_states.cpu.reg_y;
static uint8_t& reg_sp = nc1020_states.cpu.reg_sp;

static uint8_t* ram_buff = nc1020_states.ram;
static uint8_t* stack = ram_buff + 0x100;
static uint8_t* ram_io = ram_buff;
static uint8_t* ram_40 = ram_buff + 0x40;
static uint8_t* ram00 = ram_buff;
static uint8_t* ram02 = ram_buff + 0x2000;
static uint8_t* ram_b = ram_buff + 0x4000;
static uint8_t* ram04 = ram_buff + 0x6000;

static uint8_t* clock_buff = nc1020_states.clock_data;
static uint8_t& clock_flags = nc1020_states.clock_flags;

static uint8_t* jg_wav_buff = nc1020_states.jg_wav_data;
static uint8_t& jg_wav_flags = nc1020_states.jg_wav_flags;
static uint8_t& jg_wav_index = nc1020_states.jg_wav_idx;
static bool& jg_wav_playing = nc1020_states.jg_wav_playing;

static uint8_t* bak_40 = nc1020_states.bak_40;
static uint8_t& fp_step = nc1020_states.fp_step;
static uint8_t& fp_type = nc1020_states.fp_type;
static uint8_t& fp_bank_idx = nc1020_states.fp_bank_idx;
static uint8_t& fp_bak1 = nc1020_states.fp_bak1;
static uint8_t& fp_bak2 = nc1020_states.fp_bak2;
static uint8_t* fp_buff = nc1020_states.fp_buff;

static bool& slept = nc1020_states.slept;
static bool& should_wake_up = nc1020_states.should_wake_up;
static bool& wake_up_pending = nc1020_states.pending_wake_up;
static uint8_t& wake_up_key = nc1020_states.wake_up_flags;

static bool& should_irq = nc1020_states.should_irq;
static bool& timer0_toggle = nc1020_states.timer0_toggle;
static uint32_t& cycles = nc1020_states.cycles;
static uint32_t& timer0_cycles = nc1020_states.timer0_cycles;
static uint32_t& timer1_cycles = nc1020_states.timer1_cycles;

static uint8_t* keypad_matrix = nc1020_states.keypad_matrix;
static uint32_t& lcd_addr = nc1020_states.lcd_addr;
//static uint32_t lcd_addr = 0x9c3;

static io_read_func_t io_read[0x40];
static io_write_func_t io_write[0x40];

uint8_t* GetBank(uint8_t bank_idx){
	uint8_t volume_idx = ram_io[0x0D];
    if (bank_idx < num_nor_pages) {
    	return nor_banks[bank_idx];
    } else if (bank_idx >= 0x80) {

		if(nc1020){
        if (volume_idx & 0x01) {
        	return rom_volume1[bank_idx];
        } else if (volume_idx & 0x02) {
        	return rom_volume2[bank_idx];
        } else {
        	return rom_volume0[bank_idx];
        }
		}

		if(nc2000){
			//printf("<%x\n>",bank_idx);
			//assert(bank_idx==0x80);
			return nc1020_states.ext_ram;

			/*
			if(bank_idx%2==0)
			return nc1020_states.ext_ram;
			else
			return nc1020_states.ext_ram2; */
		}
    }
    return NULL;
}

void SwitchBank(){
	uint8_t bank_idx = ram_io[0x00];
	uint8_t* bank = GetBank(bank_idx);
	if(nc2000){
		if(bank== NULL) return;
		//assert(bank!=NULL);
	}
    memmap[2] = bank;
    memmap[3] = bank + 0x2000;
    memmap[4] = bank + 0x4000;
    memmap[5] = bank + 0x6000;
}

uint8_t** GetVolumm(uint8_t volume_idx){
	if ((volume_idx & 0x03) == 0x01) {
		return rom_volume1;
	} else if ((volume_idx & 0x03) == 0x03) {
		return rom_volume2;
	} else {
		return rom_volume0;
	}
}

void SwitchVolume(){
	if(nc1020){
	uint8_t volume_idx = ram_io[0x0D];
    uint8_t** volume = GetVolumm(volume_idx);
    for (int i=0; i<4; i++) {
        bbs_pages[i * 4] = volume[i];
        bbs_pages[i * 4 + 1] = volume[i] + 0x2000;
        bbs_pages[i * 4 + 2] = volume[i] + 0x4000;
        bbs_pages[i * 4 + 3] = volume[i] + 0x6000;
    }
    bbs_pages[1] = ram04;
    memmap[7] = volume[0] + 0x2000;
    uint8_t roa_bbs = ram_io[0x0A];
    memmap[1] = (roa_bbs & 0x04 ? ram_b : ram02); // this is wrong???? should be ram_io[0x0d]&0x04
    memmap[6] = bbs_pages[roa_bbs & 0x0F];
	}

	if(nc2000){
		memmap[7] = nor_banks[0]+0x6000 -0x4000;
		bool ramb=  (ram_io[0x0d]&0x04);
		if(ramb){
			memmap[1]=ram_b;
		}else{
			memmap[1]=ram02;
		}
		uint8_t bbs = ram_io[0x0A]&0xf;
		if (bbs==1) {
			memmap[6]=ram04;
		}else if (bbs==0){
			memmap[6]=nor_banks[0]+0x4000  -0x4000;
		}else if (bbs==2){
			memmap[6]=nor_banks[0]+0x8000  -0x4000;
		}else if (bbs==3) {
			memmap[6]=nor_banks[0]+0xa000  -0x4000;
		}else {
			memmap[6]=nor_banks[bbs/4]+0x2000* (bbs%4);
		}
	}

}

void super_switch(){
	uint8_t roa_bbs=ram_io[0x0a];
	uint8_t ramb_vol=ram_io[0x0d];
	uint8_t bs=ram_io[0x00];
	if(enable_debug_switch)printf("tick=%llx pc=%x bs=%x roa_bbs=%x ramb_vol=%x\n",tick, reg_pc,bs, roa_bbs , ramb_vol);

	if(nc1020){
		if(bs<0x80 &&bs>=num_nor_pages) {
			printf("ill bs %x\n",bs);
		}
	}
	if(nc2000){
		//assert(bs<0x80);
		if(bs<0x80 &&bs>=num_nor_pages) {
			//printf("ill bs %x ; ",bs);
			//printf("tick=%llx pc=%x bs=%x roa_bbs=%x ramb_vol=%x\n",tick, reg_pc,bs, roa_bbs , ramb_vol);
		}
		if(bs>=0x80) {
			//if(bs!=0x80) {printf("<%x>\n",bs);}
			//assert(bs==0x80);
		}
		if(bs>=0x80 && !(roa_bbs&0x80)){
			if(enable_oops)printf("oops1!\n");
			//assert(false);
		}
		if(bs<0x80 && (roa_bbs&0x80)){
			if(enable_oops)printf("oops2!\n");
			//assert(false);
		}
		if((ramb_vol&0x03)!=0){
			if(enable_oops)printf("oops3!\n");
			assert(false);
		}
	}

	SwitchVolume();
	SwitchBank();
	uint8_t value= ram_io[0xf];
	value&=0x7;
	if(value!=0) {
		//assert(false);
	}
	if(value==0) {
		ram_40=ram_buff+0x40;
	}else if(value==1||value==2||value==3){
		ram_40=ram_buff;
	}else{
		uint8_t off=value-4;
		ram_40=ram_buff+0x200+0x40*off;
	}
}

void GenerateAndPlayJGWav(){

}

/*
uint8_t* GetPtr40(uint8_t index){
    if (index < 4) {
        return ram_io;
    } else {
        return ram_buff + ((index) << 6);
    }
}*/

uint8_t & Peek16(uint16_t addr);

deque<char> nand_cmd;
int nand_read_cnt=0;
char nand_ori[65536][528];
char nand[65536+64][528];
char nand_spare[65536+64][16];
uint8_t IO_API ReadXX(uint8_t addr){
	if(addr==0x18&&0){
		if(nand_cmd.size()==6 && nand_cmd[0]==0x60) {
			return ram_io[0x18]|0x20;
		}
		return ram_io[0x18]&0xbc;
	}
	if(addr==0x29) {
		//printf("tick=%lld, read %x  %02x\n",tick, addr, ram_io[addr]);
					uint8_t roa_bbs=ram_io[0x0a];
		uint8_t ramb_vol=ram_io[0x0d];
		uint8_t bs=ram_io[0x00];
		uint16_t p=reg_pc-4;

		if(nand_cmd.size()==0) {
			printf("oops! no nand cmd\n");
			return 0xff;
		}
		assert(nand_cmd.size()>0);

		if(nand_cmd[0]==0x70 && nand_cmd.size()==1) {
			return 0x40;
		}

		if(nand_cmd[0]!=0x0 && nand_cmd[0]!=0x1 &&nand_cmd[0]!=0x60 &&nand_cmd[0]!=0x50){
			printf("<<%x>>!!!\n",(unsigned char)nand_cmd[0]);
			for(int i=0;i<nand_cmd.size();i++){
				printf("<%x>",(unsigned char)nand_cmd[i]);
			}
			printf("\n");
			if(nand_cmd[0]==0x60){
				return 0;	
			}
			assert(false);
			return 0;
		}

		
		unsigned char cmd=nand_cmd[0];
		if(cmd ==0 ||cmd==1||cmd==0x50){
			if(nand_cmd.size()!=5 ){
				printf("oops cmd size!=5\n");
				assert(false);
				for(int i=0;i<nand_cmd.size();i++){
					printf("<%x>",(unsigned char)nand_cmd[i]);
				}
				printf("\n");
				return 0;
			}

			unsigned char low=nand_cmd[1];
			unsigned char mid=nand_cmd[2];
			unsigned char high=nand_cmd[3];

			uint32_t pos=high*256u+mid;

			if(nand_cmd.size()==5&&false){
				printf("[%x %x]",low,high);
				
				for(int i=0;i<nand_cmd.size();i++){
					printf("<%x>",(unsigned char)nand_cmd[i]);
				}
				printf("\n");
				exit(-1);
				//printf("<%x;%x,%x:%x,%d>", final, pos, low,cmd,nand_read_cnt);
			}
			
			unsigned int x=pos;
			unsigned int y=low;
			if(cmd==0x1) y+=256u;
			if(cmd==0x50) y+=512u;
			unsigned int final= pos*528u+ y +nand_read_cnt;
			//final-=32*1024;
			if(nand_read_cnt==0 && enable_debug_nand){
				printf("[%x %x]",low,high);
				
				for(int i=0;i<nand_cmd.size();i++){
					printf("<%x>",(unsigned char)nand_cmd[i]);
				}
				printf("<%x;%x,%x:%x,%d>\n", final, pos, low,cmd,nand_read_cnt);
			}

			char *p=&nand[0][0];
			uint8_t result=p[final];
			//if(final<0) return 0x00;
			//uint8_t result=nand[pos][low+off+nand_read_cnt];
			nand_read_cnt++;
			return result;
		}

		//printf("bs=%x roa_bbs=%x pc=%x  %x %x %x %x \n",ram_io[0x00], ram_io[0x0a], reg_pc,  Peek16(p), Peek16(p+1),Peek16(p+2),Peek16(p+3));
		if(cmd==0x60){
			unsigned char low=nand_cmd[1];
			unsigned char mid=nand_cmd[2];
			unsigned char high=nand_cmd[3];

			//assert(false);
			//return 0x40;
			//if(xxx==1) return 0x40;
			unsigned int final= (mid*256u+low)*528u;
			/*
			if(nand_read_cnt%100==0){
				printf("<cmd60> %d\n",nand_read_cnt);
				for(int i=0;i<nand_cmd.size();i++){
					printf("<%x>",(unsigned char)nand_cmd[i]);
				}
			}*/
			nand_read_cnt++;
			char *p=&nand[0][0];
			printf("final=%x\n",final);
			printf("erase!!! %x tick=%lld pc=%x %x\n",final,tick,reg_pc, ram_io[0x00]);
			//assert(false);

			//final-=32*1024;
			memset(p+final,0xff,32*528);
			return 0x40;
		}

		assert(false);
	}
	return ram_io[addr];
}

uint8_t IO_API Read06(uint8_t addr){
	return ram_io[addr];
}

uint8_t IO_API Read3B(uint8_t addr){
    if (!(ram_io[0x3D] & 0x03)) {
        return clock_buff[0x3B] & 0xFE;
    }
    return ram_io[addr];
}

uint8_t IO_API Read3F(uint8_t addr){
    uint8_t idx = ram_io[0x3E];
    return idx < 80 ? clock_buff[idx] : 0;
}



void inject(){

	for(int i=0;i<8;i++){
		for(int j=0;j<=0xFE;j+=2){
			ram_b[0x100*i+j]=j;
			ram_b[0x100*i+j+1]=i;
		}
	}

	for(int i=0;i<16;i++){
		printf("<%x>",ram_b[i]);
	}
	printf("\n");

	memcpy(nc1020_states.ext_ram, inject_code.c_str(), inject_code.size());
	ram_io[0x00]=0x80;
	ram_io[0x0a]=0x80;
	//Peek16(0xe3)=0x40;
	//ram_io[0x0d]&=0x5d;
	super_switch();
	//Peek16(0x280)=0xFF;
	//Peek16(0x281)=0xFF;
	//Peek16(0x282)=0xFF;
	//Peek16(0x283)=0xFF;
	//Peek16(0xe3)=0x40;
	//Peek16(0xe4)=0xb2;
	//nc1020_states.ext_ram[0x17]=0x58;
	reg_pc=0x4018;
}


bool wanna_inject=false;


unsigned long last_tick=0;
void IO_API WriteXX(uint8_t addr, uint8_t value){
	if(addr==0x29) {
		//printf("tick=%lld, write %x  %02x\n",tick, addr, value);
			uint8_t roa_bbs=ram_io[0x0a];
		uint8_t ramb_vol=ram_io[0x0d];
		uint8_t bs=ram_io[0x00];
		uint16_t p=reg_pc-4;


		//for test only
		/*
		if(value!=0xff&& nand_read_cnt!=0) {
			nand_cmd.clear();
			nand_read_cnt=0;
		}*/

	
		/*if(value==0xff &&false) {nand_cmd.clear();
			if(nand_read_cnt){
				//printf("\n");
			}
			nand_read_cnt=0; 
		}
		else {*/
		
		{
			//if(tick-last_tick>=9) {
			if(tick-last_tick>=40) {    // some magic timeout,  which should be removed after the 018h IO is emulated
				if(nand_read_cnt!=0&& enable_debug_nand) {
					printf("<nand reads %d>\n",nand_read_cnt);
				}
				assert(nand_cmd.size()<10);
				nand_cmd.clear();
				nand_read_cnt=0;
				if(value==0xff) goto out;
			}
			last_tick=tick;
			nand_cmd.push_back(value);

			if(nand_cmd.size()>6) {

				
				/*printf("program!! ");
				for(int i=0;i<nand_cmd.size();i++){
					printf("<%02x>",(unsigned char)nand_cmd[i]);
				}
				printf("%d\n",(int)nand_cmd.size());*/
				if(nand_cmd.size()==535 && nand_cmd[nand_cmd.size()-1]==0x10){
					assert(nand_cmd[0]==0 && (unsigned char)nand_cmd[1]==0x80);
					unsigned char low=nand_cmd[2];
					unsigned char mid=nand_cmd[3];
					unsigned char high=nand_cmd[4];

					uint32_t pos=high*256u+mid;

					unsigned int x=pos;
					unsigned int y=low;
					unsigned int final= pos*528u+ y;
					char *p=&nand[0][0];
					printf("program!!!! final=%x\n",final);

					for(int i=0;i<528;i++){
						p[final+i]=nand_cmd[6+i];
					}
					//assert(false);
					//printf("erase!!! %x tick=%lld pc=%x %x\n",final,tick,reg_pc, ram_io[0x00]);
					//assert(false);

					//final-=32*1024;
					//memset(p+final,0xff,32*528);
					//return 0x40;
					//assert(false);
					nand_cmd.clear();
				}
			}
			
			//if(nand_cmd.size()==6) nand_cmd.pop_front();
		}
		out:;
		if(enable_debug_nand) printf("write $29 %x\n",value);
		//if(nand_cmd.size()==1&& nand_cmd[0]==0xff) nand_cmd.clear();
		//printf("bs=%x roa_bbs=%x pc=%x  %x %x %x %x \n",ram_io[0x00], ram_io[0x0a], reg_pc,  Peek16(p), Peek16(p+1),Peek16(p+2),Peek16(p+3));
		//if(do_inject) wanna_inject=true;
	}
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
		printf("%x\n",lcd_addr);
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

void AdjustTime(){
    if (++ clock_buff[0] >= 60) {
        clock_buff[0] = 0;
        if (++ clock_buff[1] >= 60) {
            clock_buff[1] = 0;
            if (++ clock_buff[2] >= 24) {
                clock_buff[2] &= 0xC0;
                ++ clock_buff[3];
            }
        }
    }
}

bool IsCountDown(){
    if (!(clock_buff[10] & 0x02) ||
        !(clock_flags & 0x02)) {
        return false;
    }
    return (
        ((clock_buff[7] & 0x80) && !(((clock_buff[7] ^ clock_buff[2])) & 0x1F)) ||
        ((clock_buff[6] & 0x80) && !(((clock_buff[6] ^ clock_buff[1])) & 0x3F)) ||
        ((clock_buff[5] & 0x80) && !(((clock_buff[5] ^ clock_buff[0])) & 0x3F))
        );
}

/**
 * ProcessBinary
 * encrypt or decrypt wqx's binary file. just flip every bank.
 */
void ProcessBinary(uint8_t* dest, uint8_t* src, uint32_t size){
	uint32_t offset = 0;
    while (offset < size) {
        memcpy(dest + offset + 0x4000, src + offset, 0x4000);
        memcpy(dest + offset, src + offset + 0x4000, 0x4000);
        offset += 0x8000;
    }
}

void SimpleCopy(uint8_t* dest, uint8_t* src, uint32_t size){
	uint32_t offset = 0;
    while (offset < size) {
        memcpy(dest + offset, src + offset, 0x4000);
        //memcpy(dest + offset, src + offset + 0x4000, 0x4000);
        offset += 0x4000;
    }
}


void LoadRom(){
	uint8_t* temp_buff = (uint8_t*)malloc(ROM_SIZE);
	FILE* file = fopen(nc1020_rom.romPath.c_str(), "rb");
	fread(temp_buff, 1, ROM_SIZE, file);
	ProcessBinary(rom_buff, temp_buff, ROM_SIZE);
	free(temp_buff);
	fclose(file);
}

void LoadNor(){
	uint8_t* temp_buff = (uint8_t*)malloc(NOR_SIZE);
	FILE* file = fopen(nc1020_rom.norFlashPath.c_str(), "rb");
	fread(temp_buff, 1, NOR_SIZE, file);
	ProcessBinary(nor_buff, temp_buff, NOR_SIZE);
	free(temp_buff);
	fclose(file);
}

void SaveNor(){
	uint8_t* temp_buff = (uint8_t*)malloc(NOR_SIZE);
	FILE* file = fopen(nc1020_rom.norFlashPath.c_str(), "wb");
	ProcessBinary(temp_buff, nor_buff, NOR_SIZE);
	fwrite(temp_buff, 1, NOR_SIZE, file);
	fflush(file);
	free(temp_buff);
	fclose(file);
}

/*
inline uint8_t & Peek8(uint8_t addr) {
	return ram_buff[addr];
}*/
uint8_t & Peek16(uint16_t addr) {
	return memmap[addr >> 13][addr & 0x1FFF];
}
inline uint16_t PeekW(uint16_t addr) {
	return Peek16(addr) | (Peek16((uint16_t) (addr + 1)) << 8);
}
inline uint8_t Load(uint16_t addr) {
	if (addr < IO_LIMIT) {
		return io_read[addr](addr);
	}
	if (((fp_step == 4 && fp_type == 2) ||
		(fp_step == 6 && fp_type == 3)) &&
		(addr >= 0x4000 && addr < 0xC000)) {
		fp_step = 0;
		return 0x88;
	}
	if (addr == 0x45F && wake_up_pending) {
		wake_up_pending = false;
		memmap[0][0x45F] = wake_up_key;
	}
	return Peek16(addr);
}
inline void Store(uint16_t addr, uint8_t value) {
	if (addr < IO_LIMIT) {
		io_write[addr](addr, value);
		return;
	}
	if (addr < 0x4000) {
		Peek16(addr) = value;
		return;
	}
	uint8_t* page = memmap[addr >> 13];
	if (page == ram_b/*ramb*/ || page == ram04/*ram04*/  ||page ==ram02||page==ram00) {
		page[addr & 0x1FFF] = value;
		return;
	}
	
	if (page == nc1020_states.ext_ram|| page == nc1020_states.ext_ram+0x2000  ||page == nc1020_states.ext_ram+0x4000|| page == nc1020_states.ext_ram+0x6000) {
		//printf("write!!!");
		//assert(false);
		page[addr & 0x1FFF] = value;
		return;
	}
	if (addr >= 0xE000) {
		return;
	}

    // write to nor_flash address space.
    // there must select a nor_bank.

    uint8_t bank_idx = ram_io[0x00];

	if(nc2000){
		if (bank_idx >= 0x80 && addr>=0x4000 && addr<=0xbfff) {
			printf("write2!!!");
			/*
			Peek16(addr) = value;
			if (addr == 0x8000 && value == 0xF0) {
        		fp_step = 0;
			}
			return;*/
		}
	}

	assert(bank_idx<num_nor_pages);
	if(nc2000){
		//assert(addr>=0x4000&& addr<=0xbfff);
	}
    if (bank_idx >= num_nor_pages) {
        return;
    }

    uint8_t* bank = nor_banks[bank_idx];

    if (fp_step == 0) {
        if (addr == 0x5555 && value == 0xAA) {
            fp_step = 1;
        }
        return;
    }
    if (fp_step == 1) {
        if (addr == 0xAAAA && value == 0x55) {
        	fp_step = 2;
            return;
        }
    } else if (fp_step == 2) {
        if (addr == 0x5555) {
        	switch (value) {
        	case 0x90: fp_type = 1; break;
        	case 0xA0: fp_type = 2; break;
        	case 0x80: fp_type = 3; break;
        	case 0xA8: fp_type = 4; break;
        	case 0x88: fp_type = 5; break;
        	case 0x78: fp_type = 6; break;
        	}
            if (fp_type) {
                if (fp_type == 1) {
					assert(false);
                    fp_bank_idx = bank_idx;
                    fp_bak1 = bank[0x4000];
                    fp_bak2 = bank[0x4001];
                }
                fp_step = 3;
                return;
            }
        }
    } else if (fp_step == 3) {
        if (fp_type == 1) {
            if (value == 0xF0) {
                bank[0x4000] = fp_bak1;
                bank[0x4001] = fp_bak2;
                fp_step = 0;
                return;
            }
        } else if (fp_type == 2) {
            bank[addr - 0x4000] &= value;
            fp_step = 4;
            return;
        } else if (fp_type == 4) {
            fp_buff[addr & 0xFF] &= value;
            fp_step = 4;
            return;
        } else if (fp_type == 3 || fp_type == 5) {
            if (addr == 0x5555 && value == 0xAA) {
                fp_step = 4;
                return;
            }
        }
    } else if (fp_step == 4) {
        if (fp_type == 3 || fp_type == 5) {
            if (addr == 0xAAAA && value == 0x55) {
                fp_step = 5;
                return;
            }
        }
    } else if (fp_step == 5) {
		
        if (addr == 0x5555 && value == 0x10) {
        	for (uint32_t i=0; i<0x20; i++) {
				printf("wanna erase 111\n");
                memset(nor_banks[i], 0xFF, 0x8000);
            }
            if (fp_type == 5) {
                memset(fp_buff, 0xFF, 0x100);
            }
            fp_step = 6;
            return;
        }
        if (fp_type == 3) {
            if (value == 0x30) {
				printf("wanna erase 222\n");
                memset(bank + (addr - (addr % 0x800) - 0x4000), 0xFF, 0x800);
                fp_step = 6;
                return;
            }
        } else if (fp_type == 5) {
            if (value == 0x48) {
                memset(fp_buff, 0xFF, 0x100);
                fp_step = 6;
                return;
            }
        }
    }
    if (addr == 0x8000 && value == 0xF0) {
        fp_step = 0;
        return;
    }
    printf("error occurs when operate in flash!");
}

void Initialize(WqxRom rom) {

	std::ifstream inject_bin("shoot.bin");
	std::stringstream buffer;
	buffer << inject_bin.rdbuf();
	printf("<size=%lu>\n",buffer.str().size());
	inject_code=buffer.str();


	std::ifstream nand_bin("2600nand.bin");
	std::stringstream nand_buffer;
	nand_buffer << nand_bin.rdbuf();
	printf("<size=%lu>\n",nand_buffer.str().size());

	memset(nand_ori,0,sizeof(nand_ori));
	char *p0= &nand_ori[0][0];
	memcpy(p0,&nand_buffer.str()[0],nand_buffer.str().size());


	srand(0);
	for(int i=0;i<65536;i++) {
		char *ppp=nand[i];
		for(int j=0;j<512;j++){
			ppp[j]=rand();

		}
	}

	printf("<nand_ori_size=%lu>\n",sizeof(nand_ori));
	memset(nand[0],0xff,64*528);
	//memset(nand_ori,0xff,sizeof(nand_ori));
	char *p=&nand[0][0];
	//memset(p+0x6200,0,32*12);
	//memcpy(p+0x6380,zifu,sizeof(zifu));

	//memcpy(p+0x4000,font8x12,sizeof(font8x12));

	for(int i=0;i<65536;i++){
		memcpy(nand[i+64],nand_ori[i],528);
		//memcpy(p+i,p0+i+16*1024,16*1024);
	}

	for(int i=0;i<65536;i++){
		memcpy(nand_spare[i+64],nand_ori[i]+512,16);
	}
	
	memcpy(nand+0x200,"ggv nc2000",strlen("ggv nc2000"));
	//memset(p+0x400,0xff,512);
	//memset(p+0x5be00,0xff,512);
	


    nc1020_rom = rom;
	for (uint32_t i=0; i<num_rom_pages/3; i++) {
		rom_volume0[i] = rom_buff + (0x8000 * i);
		rom_volume1[i] = rom_buff + (0x8000 * (0x100 + i));
		rom_volume2[i] = rom_buff + (0x8000 * (0x200 + i));
	}

	for (uint32_t i=0; i<num_nor_pages; i++) {
		nor_banks[i] = nor_buff + (0x8000 * i);
	}
	for (uint32_t i=0; i<0x40; i++) {
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
	io_write[0x23] = Write23;
	io_write[0x3F] = Write3F;

	//LoadRom();
//#ifdef DEBUG
//	FILE* file = fopen((nc1020_dir + "/wqxsimlogs.bin").c_str(), "rb");
//	fseek(file, 0L, SEEK_END);
//	uint32_t file_size = ftell(file);
//	uint32_t insts_count = (file_size - 8) / 8;
//	debug_logs.insts_count = insts_count;
//	debug_logs.logs = (log_rec_t*)malloc(insts_count * 8);
//	fseek(file, 0L, SEEK_SET);
//	fread(&debug_logs.insts_start, 4, 1, file);
//	fseek(file, 4L, SEEK_SET);
//	fread(&debug_logs.peek_addr, 4, 1, file);
//	fseek(file, 8L, SEEK_SET);
//	fread(debug_logs.logs, 8, insts_count, file);
//	fclose(file);
//#endif
}

void ResetStates(){
	version = VERSION;

	memset(ram_buff, 0, 0x8000);
	memset(ram_b,0xff,0x1000);

	memset(nc1020_states.ext_ram,0,0x8000);

	memmap[0] = ram00;
	//////memmap[2] = ram_b;   // ???? no  effect
	super_switch();
	//SwitchVolume();

	memset(keypad_matrix, 0, 8);

	memset(clock_buff, 0, 80);
	clock_flags = 0;

	timer0_toggle = false;

	memset(jg_wav_buff, 0, 0x20);
	jg_wav_flags = 0;
	jg_wav_index = 0;

	should_wake_up = false;
	wake_up_pending = false;

	memset(fp_buff, 0, 0x100);
	fp_step = 0;

	should_irq = false;

	cycles = 0;
	reg_a = 0;
	reg_ps = 0x24;
	reg_x = 0;
	reg_y = 0;
	reg_sp = 0xFF;
	reg_pc = PeekW(RESET_VEC);
	timer0_cycles = CYCLES_TIMER0;
	timer1_cycles = CYCLES_TIMER1;

//#ifdef DEBUG
//	executed_insts = 0;
//	debug_done = false;
//#endif
}

void Reset() {
	LoadNor();
	ResetStates();
}

void LoadStates(){
	ResetStates();
	FILE* file = fopen(nc1020_rom.statesPath.c_str(), "rb");
	if (file == NULL) {
		return;
	}
	fread(&nc1020_states, 1, sizeof(nc1020_states), file);
	fclose(file);
	if (version != VERSION) {
		return;
	}
	//SwitchVolume();
	super_switch();
}

void SaveStates(){
	FILE* file = fopen(nc1020_rom.statesPath.c_str(), "wb");
	fwrite(&nc1020_states, 1, sizeof(nc1020_states), file);
	fflush(file);
	fclose(file);
}

void LoadNC1020(){
	LoadNor();
	LoadStates();
}

void SaveNC1020(){
	SaveNor();
	SaveStates();
}

void SetKey(uint8_t key_id, bool down_or_up){
	uint8_t row = key_id % 8;
	uint8_t col = key_id / 8;
	uint8_t bits = 1 << col;
	if (key_id == 0x0F) {
		bits = 0xFE;
	}
	if (down_or_up) {
		keypad_matrix[row] |= bits;
	} else {
		keypad_matrix[row] &= ~bits;
	}

	if (down_or_up) {
		if (slept) {
			if (key_id >= 0x08 && key_id <= 0x0F && key_id != 0x0E) {
				switch (key_id) {
				case 0x08: wake_up_key = 0x00; break;
				case 0x09: wake_up_key = 0x0A; break;
				case 0x0A: wake_up_key = 0x08; break;
				case 0x0B: wake_up_key = 0x06; break;
				case 0x0C: wake_up_key = 0x04; break;
				case 0x0D: wake_up_key = 0x02; break;
				case 0x0E: wake_up_key = 0x0C; break;
				case 0x0F: wake_up_key = 0x00; break;
				}
				should_wake_up = true;
				wake_up_pending = true;
				slept = false;
			}
		} else {
			if (key_id == 0x0F) {
				slept = true;
			}
		}
	}
}

bool CopyLcdBuffer(uint8_t* buffer){
	if (lcd_addr == 0) return false;
	uint32_t off=0;
	if(nc2000) off=400;
	memcpy(buffer, ram_buff + lcd_addr+off, 1600);

	if(nc2000){
		memcpy(buffer, ram_buff + 0x19c0, 1600);
	}
	return true;
}
bool injected=false;
void RunTimeSlice(uint32_t time_slice, bool speed_up) {
	uint32_t end_cycles = time_slice * CYCLES_MS;
	/*
	register uint32_t cycles = wqx::cycles;
	register uint16_t reg_pc = wqx::reg_pc;
	register uint8_t reg_a = wqx::reg_a;
	register uint8_t reg_ps = wqx::reg_ps;
	register uint8_t reg_x = wqx::reg_x;
	register uint8_t reg_y = wqx::reg_y;
	register uint8_t reg_sp = wqx::reg_sp;*/

	while (cycles < end_cycles) {
		tick++;
		if(tick%10000000==0) printf("tick=%lld\n",tick);
//#ifdef DEBUG
//		if (executed_insts == 2792170) {
//			printf("debug start!\n");
//		}
//		if (executed_insts >= debug_logs.insts_start &&
//			executed_insts < debug_logs.insts_start + debug_logs.insts_count) {
//			log_rec_t& log = debug_logs.logs[executed_insts - debug_logs.insts_start];
//			string debug_info;
//			if (log.reg_pc != reg_pc) {
//				debug_info += " pc ";
//			}
//			if (log.reg_a != reg_a) {
//				debug_info += " a ";
//			}
//			if (log.reg_ps != reg_ps) {
//				debug_info += " ps ";
//			}
//			if (log.reg_x != reg_x) {
//				debug_info += " x ";
//			}
//			if (log.reg_y != reg_y) {
//				debug_info += " y ";
//			}
//			if (log.reg_sp != reg_sp) {
//				debug_info += " sp ";
//			}
//			if (debug_logs.peek_addr != -1) {
//				if (log.peeked != Peek((uint16_t)debug_logs.peek_addr)) {
//					debug_info += " mem ";
//				}
//			} else {
//				if (log.peeked != Peek(reg_pc)) {
//					debug_info += " op ";
//				}
//			}
//			if (debug_info.length()) {
//				printf("%d: %s\n", executed_insts, debug_info.c_str());
//				exit(-1);
//			}
//		}
//		if (executed_insts >= debug_logs.insts_start + debug_logs.insts_count) {
//			printf("ok\n");
//		}
//#endif
		//if(tick>8525000) wanna_inject=true;
		if(do_inject&& tick>11000000) wanna_inject=true;
		if(wanna_inject&& !injected){
			inject();
			printf("injected!!!");
			wanna_inject=false;
			injected=true;
		}

		//if(tick>=6012850) enable_debug_pc=true;
		//if(injected && reg_pc==0x2000) enable_debug_pc=true;
		if(enable_debug_pc||enable_dyn_debug){
			unsigned char buf[10];
			buf[0]=Peek16(reg_pc);
			buf[1]=Peek16(reg_pc+1);
			buf[2]=Peek16(reg_pc+2);
			buf[3]=0;
			printf("tick=%lld ",tick /*, reg_pc*/);
			printf("%02x %02x %02x %02x; ",Peek16(reg_pc), Peek16(reg_pc+1),Peek16(reg_pc+2),Peek16(reg_pc+3));
			printf("bs=%02x roa_bbs=%02x ramb=%02x zp=%02x reg=%02x,%02x,%02x,%02x,%03o  pc=%s",ram_io[0x00], ram_io[0x0a], ram_io[0x0d], ram_io[0x0f],reg_a,reg_x,reg_y,reg_sp,reg_ps,disassemble_next(buf,reg_pc).c_str());
			printf("\n");

			//getchar();		
		}
		if(injected && tick%1==0){
			//printf("bs=%x roa_bbs=%x pc=%x  %x %x %x %x \n",ram_io[0x00], ram_io[0x0a], reg_pc,  Peek16(reg_pc), Peek16(reg_pc+1),Peek16(reg_pc+2),Peek16(reg_pc+3));
			//getchar();
		}

		if(injected &&reg_pc==0x2018){
			printf("%x\n",reg_x);
			//cycles++;
			//continue;
		}
		
		switch (Peek16(reg_pc++)) {
		case 0x00: {
			reg_pc++;
			stack[reg_sp--] = reg_pc >> 8;
			stack[reg_sp--] = reg_pc & 0xFF;
			reg_ps |= 0x10;
			stack[reg_sp--] = reg_ps;
			reg_ps |= 0x04;
			reg_pc = PeekW(IRQ_VEC);
			cycles += 7;
		}
			break;
		case 0x01: {
			uint16_t addr = PeekW((Peek16(reg_pc++) + reg_x) & 0xFF);
			reg_a |= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 6;
		}
			break;
		case 0x02: {
		}
			break;
		case 0x03: {
		}
			break;
		case 0x04: {
		}
			break;
		case 0x05: {
			uint16_t addr = Peek16(reg_pc++);
			reg_a |= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 3;
		}
			break;
		case 0x06: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x7C;
			reg_ps |= (tmp1 >> 7);
			tmp1 <<= 1;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			Store(addr, tmp1);
			cycles += 5;
		}
			break;
		case 0x07: {
		}
			break;
		case 0x08: {
			stack[reg_sp--] = reg_ps;
			cycles += 3;
		}
			break;
		case 0x09: {
			uint16_t addr = reg_pc++;
			reg_a |= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0x0A: {
			reg_ps &= 0x7C;
			reg_ps |= reg_a >> 7;
			reg_a <<= 1;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0x0B: {
		}
			break;
		case 0x0C: {
		}
			break;
		case 0x0D: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			reg_a |= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x0E: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x7C;
			reg_ps |= (tmp1 >> 7);
			tmp1 <<= 1;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			Store(addr, tmp1);
			cycles += 6;
		}
			break;
		case 0x0F: {
		}
			break;
		case 0x10: {
			int8_t tmp4 = (int8_t) (Peek16(reg_pc++));
			uint16_t addr = reg_pc + tmp4;
			if (!(reg_ps & 0x80)) {
				cycles += !((reg_pc ^ addr) & 0xFF00) << 1;
				reg_pc = addr;
			}
			cycles += 2;
		}
			break;
		case 0x11: {
			uint16_t addr = PeekW(Peek16(reg_pc));
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc++;
			reg_a |= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 5;
		}
			break;
		case 0x12: {
		}
			break;
		case 0x13: {
		}
			break;
		case 0x14: {
		}
			break;
		case 0x15: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			reg_a |= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x16: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x7C;
			reg_ps |= (tmp1 >> 7);
			tmp1 <<= 1;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			Store(addr, tmp1);
			cycles += 6;
		}
			break;
		case 0x17: {
		}
			break;
		case 0x18: {
			reg_ps &= 0xFE;
			cycles += 2;
		}
			break;
		case 0x19: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc += 2;
			reg_a |= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x1A: {
		}
			break;
		case 0x1B: {
		}
			break;
		case 0x1C: {
		}
			break;
		case 0x1D: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_x) & 0xFF00);
			addr += reg_x;
			reg_pc += 2;
			reg_a |= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x1E: {
			uint16_t addr = PeekW(reg_pc);
			addr += reg_x;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x7C;
			reg_ps |= (tmp1 >> 7);
			tmp1 <<= 1;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			Store(addr, tmp1);
			cycles += 6;
		}
			break;
		case 0x1F: {
		}
			break;
		case 0x20: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			reg_pc--;
			stack[reg_sp--] = reg_pc >> 8;
			stack[reg_sp--] = reg_pc & 0xFF;
			reg_pc = addr;
			cycles += 6;
		}
			break;
		case 0x21: {
			uint16_t addr = PeekW((Peek16(reg_pc++) + reg_x) & 0xFF);
			reg_a &= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 6;
		}
			break;
		case 0x22: {
		}
			break;
		case 0x23: {
		}
			break;
		case 0x24: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x3D;
			reg_ps |= (!(reg_a & tmp1) << 1) | (tmp1 & 0xC0);
			cycles += 3;
		}
			break;
		case 0x25: {
			uint16_t addr = Peek16(reg_pc++);
			reg_a &= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 3;
		}
			break;
		case 0x26: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr);
			uint8_t tmp2 = (tmp1 << 1) | (reg_ps & 0x01);
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >> 7);
			Store(addr, tmp2);
			cycles += 5;
		}
			break;
		case 0x27: {
		}
			break;
		case 0x28: {
			reg_ps = stack[++reg_sp];
			cycles += 4;
		}
			break;
		case 0x29: {
			uint16_t addr = reg_pc++;
			reg_a &= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0x2A: {
			uint8_t tmp1 = reg_a;
			reg_a = (reg_a << 1) | (reg_ps & 0x01);
			reg_ps &= 0x7C;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1) | (tmp1 >> 7);
			cycles += 2;
		}
			break;
		case 0x2B: {
		}
			break;
		case 0x2C: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x3D;
			reg_ps |= (!(reg_a & tmp1) << 1) | (tmp1 & 0xC0);
			cycles += 4;
		}
			break;
		case 0x2D: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			reg_a &= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x2E: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			uint8_t tmp2 = (tmp1 << 1) | (reg_ps & 0x01);
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >> 7);
			Store(addr, tmp2);
			cycles += 6;
		}
			break;
		case 0x2F: {
		}
			break;
		case 0x30: {
			int8_t tmp4 = (int8_t) (Peek16(reg_pc++));
			uint16_t addr = reg_pc + tmp4;
			if ((reg_ps & 0x80)) {
				cycles += !((reg_pc ^ addr) & 0xFF00) << 1;
				reg_pc = addr;
			}
			cycles += 2;
		}
			break;
		case 0x31: {
			uint16_t addr = PeekW(Peek16(reg_pc));
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc++;
			reg_a &= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 5;
		}
			break;
		case 0x32: {
		}
			break;
		case 0x33: {
		}
			break;
		case 0x34: {
		}
			break;
		case 0x35: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			reg_a &= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x36: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			uint8_t tmp1 = Load(addr);
			uint8_t tmp2 = (tmp1 << 1) | (reg_ps & 0x01);
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >> 7);
			Store(addr, tmp2);
			cycles += 6;
		}
			break;
		case 0x37: {
		}
			break;
		case 0x38: {
			reg_ps |= 0x01;
			cycles += 2;
		}
			break;
		case 0x39: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc += 2;
			reg_a &= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x3A: {
		}
			break;
		case 0x3B: {
		}
			break;
		case 0x3C: {
		}
			break;
		case 0x3D: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_x) & 0xFF00);
			addr += reg_x;
			reg_pc += 2;
			reg_a &= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x3E: {
			uint16_t addr = PeekW(reg_pc);
			addr += reg_x;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			uint8_t tmp2 = (tmp1 << 1) | (reg_ps & 0x01);
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >> 7);
			Store(addr, tmp2);
			cycles += 6;
		}
			break;
		case 0x3F: {
		}
			break;
		case 0x40: {
			reg_ps = stack[++reg_sp];
			reg_pc = stack[++reg_sp];
			reg_pc |= stack[++reg_sp] << 8;
			cycles += 6;
		}
			break;
		case 0x41: {
			uint16_t addr = PeekW((Peek16(reg_pc++) + reg_x) & 0xFF);
			reg_a ^= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 6;
		}
			break;
		case 0x42: {
		}
			break;
		case 0x43: {
		}
			break;
		case 0x44: {
		}
			break;
		case 0x45: {
			uint16_t addr = Peek16(reg_pc++);
			reg_a ^= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 3;
		}
			break;
		case 0x46: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x7C;
			reg_ps |= (tmp1 & 0x01);
			tmp1 >>= 1;
			reg_ps |= (!tmp1 << 1);
			Store(addr, tmp1);
			cycles += 5;
		}
			break;
		case 0x47: {
		}
			break;
		case 0x48: {
			stack[reg_sp--] = reg_a;
			cycles += 3;
		}
			break;
		case 0x49: {
			uint16_t addr = reg_pc++;
			reg_a ^= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0x4A: {
			reg_ps &= 0x7C;
			reg_ps |= reg_a & 0x01;
			reg_a >>= 1;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0x4B: {
		}
			break;
		case 0x4C: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			reg_pc = addr;
			cycles += 3;
		}
			break;
		case 0x4D: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			reg_a ^= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x4E: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x7C;
			reg_ps |= (tmp1 & 0x01);
			tmp1 >>= 1;
			reg_ps |= (!tmp1 << 1);
			Store(addr, tmp1);
			cycles += 6;
		}
			break;
		case 0x4F: {
		}
			break;
		case 0x50: {
			int8_t tmp4 = (int8_t) (Peek16(reg_pc++));
			uint16_t addr = reg_pc + tmp4;
			if (!(reg_ps & 0x40)) {
				cycles += !((reg_pc ^ addr) & 0xFF00) << 1;
				reg_pc = addr;
			}
			cycles += 2;
		}
			break;
		case 0x51: {
			uint16_t addr = PeekW(Peek16(reg_pc));
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc++;
			reg_a ^= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 5;
		}
			break;
		case 0x52: {
		}
			break;
		case 0x53: {
		}
			break;
		case 0x54: {
		}
			break;
		case 0x55: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			reg_a ^= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x56: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x7C;
			reg_ps |= (tmp1 & 0x01);
			tmp1 >>= 1;
			reg_ps |= (!tmp1 << 1);
			Store(addr, tmp1);
			cycles += 6;
		}
			break;
		case 0x57: {
		}
			break;
		case 0x58: {
			reg_ps &= 0xFB;
			cycles += 2;
		}
			break;
		case 0x59: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc += 2;
			reg_a ^= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x5A: {
		}
			break;
		case 0x5B: {
		}
			break;
		case 0x5C: {
		}
			break;
		case 0x5D: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_x) & 0xFF00);
			addr += reg_x;
			reg_pc += 2;
			reg_a ^= Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x5E: {
			uint16_t addr = PeekW(reg_pc);
			addr += reg_x;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			reg_ps &= 0x7C;
			reg_ps |= (tmp1 & 0x01);
			tmp1 >>= 1;
			reg_ps |= (!tmp1 << 1);
			Store(addr, tmp1);
			cycles += 6;
		}
			break;
		case 0x5F: {
		}
			break;
		case 0x60: {
			reg_pc = stack[++reg_sp];
			reg_pc |= (stack[++reg_sp] << 8);
			reg_pc++;
			cycles += 6;
		}
			break;
		case 0x61: {
			uint16_t addr = PeekW((Peek16(reg_pc++) + reg_x) & 0xFF);
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a + tmp1 + (reg_ps & 0x01);
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 > 0xFF)
					| (((reg_a ^ tmp1 ^ 0x80) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 6;
		}
			break;
		case 0x62: {
		}
			break;
		case 0x63: {
		}
			break;
		case 0x64: {
		}
			break;
		case 0x65: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a + tmp1 + (reg_ps & 0x01);
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 > 0xFF)
					| (((reg_a ^ tmp1 ^ 0x80) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 3;
		}
			break;
		case 0x66: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr);
			uint8_t tmp2 = (tmp1 >> 1) | ((reg_ps & 0x01) << 7);
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 & 0x01);
			Store(addr, tmp2);
			cycles += 5;
		}
			break;
		case 0x67: {
		}
			break;
		case 0x68: {
			reg_a = stack[++reg_sp];
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0x69: {
			uint16_t addr = reg_pc++;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a + tmp1 + (reg_ps & 0x01);
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 > 0xFF)
					| (((reg_a ^ tmp1 ^ 0x80) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 2;
		}
			break;
		case 0x6A: {
			uint8_t tmp1 = reg_a;
			reg_a = (reg_a >> 1) | ((reg_ps & 0x01) << 7);
			reg_ps &= 0x7C;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1) | (tmp1 & 0x01);
			cycles += 2;
		}
			break;
		case 0x6B: {
		}
			break;
		case 0x6C: {
			uint16_t addr = PeekW(PeekW(reg_pc));
			reg_pc += 2;
			reg_pc = addr;
			cycles += 6;
		}
			break;
		case 0x6D: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a + tmp1 + (reg_ps & 0x01);
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 > 0xFF)
					| (((reg_a ^ tmp1 ^ 0x80) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 4;
		}
			break;
		case 0x6E: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			uint8_t tmp2 = (tmp1 >> 1) | ((reg_ps & 0x01) << 7);
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 & 0x01);
			Store(addr, tmp2);
			cycles += 6;
		}
			break;
		case 0x6F: {
		}
			break;
		case 0x70: {
			int8_t tmp4 = (int8_t) (Peek16(reg_pc++));
			uint16_t addr = reg_pc + tmp4;
			if ((reg_ps & 0x40)) {
				cycles += !((reg_pc ^ addr) & 0xFF00) << 1;
				reg_pc = addr;
			}
			cycles += 2;
		}
			break;
		case 0x71: {
			uint16_t addr = PeekW(Peek16(reg_pc));
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc++;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a + tmp1 + (reg_ps & 0x01);
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 > 0xFF)
					| (((reg_a ^ tmp1 ^ 0x80) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 5;
		}
			break;
		case 0x72: {
		}
			break;
		case 0x73: {
		}
			break;
		case 0x74: {
		}
			break;
		case 0x75: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a + tmp1 + (reg_ps & 0x01);
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 > 0xFF)
					| (((reg_a ^ tmp1 ^ 0x80) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 4;
		}
			break;
		case 0x76: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			uint8_t tmp1 = Load(addr);
			uint8_t tmp2 = (tmp1 >> 1) | ((reg_ps & 0x01) << 7);
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 & 0x01);
			Store(addr, tmp2);
			cycles += 6;
		}
			break;
		case 0x77: {
		}
			break;
		case 0x78: {
			reg_ps |= 0x04;
			cycles += 2;
		}
			break;
		case 0x79: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a + tmp1 + (reg_ps & 0x01);
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 > 0xFF)
					| (((reg_a ^ tmp1 ^ 0x80) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 4;
		}
			break;
		case 0x7A: {
		}
			break;
		case 0x7B: {
		}
			break;
		case 0x7C: {
		}
			break;
		case 0x7D: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_x) & 0xFF00);
			addr += reg_x;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a + tmp1 + (reg_ps & 0x01);
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 > 0xFF)
					| (((reg_a ^ tmp1 ^ 0x80) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 4;
		}
			break;
		case 0x7E: {
			uint16_t addr = PeekW(reg_pc);
			addr += reg_x;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			uint8_t tmp2 = (tmp1 >> 1) | ((reg_ps & 0x01) << 7);
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 & 0x01);
			Store(addr, tmp2);
			cycles += 6;
		}
			break;
		case 0x7F: {
		}
			break;
		case 0x80: {
		}
			break;
		case 0x81: {
			uint16_t addr = PeekW((Peek16(reg_pc++) + reg_x) & 0xFF);
			Store(addr, reg_a);
			cycles += 6;
		}
			break;
		case 0x82: {
		}
			break;
		case 0x83: {
		}
			break;
		case 0x84: {
			uint16_t addr = Peek16(reg_pc++);
			Store(addr, reg_y);
			cycles += 3;
		}
			break;
		case 0x85: {
			uint16_t addr = Peek16(reg_pc++);
			Store(addr, reg_a);
			cycles += 3;
		}
			break;
		case 0x86: {
			uint16_t addr = Peek16(reg_pc++);
			Store(addr, reg_x);
			cycles += 3;
		}
			break;
		case 0x87: {
		}
			break;
		case 0x88: {
			reg_y--;
			reg_ps &= 0x7D;
			reg_ps |= (reg_y & 0x80) | (!reg_y << 1);
			cycles += 2;
		}
			break;
		case 0x89: {
		}
			break;
		case 0x8A: {
			reg_a = reg_x;
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0x8B: {
		}
			break;
		case 0x8C: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			Store(addr, reg_y);
			cycles += 4;
		}
			break;
		case 0x8D: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			Store(addr, reg_a);
			cycles += 4;
		}
			break;
		case 0x8E: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			Store(addr, reg_x);
			cycles += 4;
		}
			break;
		case 0x8F: {
		}
			break;
		case 0x90: {
			int8_t tmp4 = (int8_t) (Peek16(reg_pc++));
			uint16_t addr = reg_pc + tmp4;
			if (!(reg_ps & 0x01)) {
				cycles += !((reg_pc ^ addr) & 0xFF00) << 1;
				reg_pc = addr;
			}
			cycles += 2;
		}
			break;
		case 0x91: {
			uint16_t addr = PeekW(Peek16(reg_pc));
			addr += reg_y;
			reg_pc++;
			Store(addr, reg_a);
			cycles += 6;
		}
			break;
		case 0x92: {
		}
			break;
		case 0x93: {
		}
			break;
		case 0x94: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			Store(addr, reg_y);
			cycles += 4;
		}
			break;
		case 0x95: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			Store(addr, reg_a);
			cycles += 4;
		}
			break;
		case 0x96: {
			uint16_t addr = (Peek16(reg_pc++) + reg_y) & 0xFF;
			Store(addr, reg_x);
			cycles += 4;
		}
			break;
		case 0x97: {
		}
			break;
		case 0x98: {
			reg_a = reg_y;
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0x99: {
			uint16_t addr = PeekW(reg_pc);
			addr += reg_y;
			reg_pc += 2;
			Store(addr, reg_a);
			cycles += 5;
		}
			break;
		case 0x9A: {
			reg_sp = reg_x;
			cycles += 2;
		}
			break;
		case 0x9B: {
		}
			break;
		case 0x9C: {
		}
			break;
		case 0x9D: {
			uint16_t addr = PeekW(reg_pc);
			addr += reg_x;
			reg_pc += 2;
			Store(addr, reg_a);
			cycles += 5;
		}
			break;
		case 0x9E: {
		}
			break;
		case 0x9F: {
		}
			break;
		case 0xA0: {
			uint16_t addr = reg_pc++;
			reg_y = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_y & 0x80) | (!reg_y << 1);
			cycles += 2;
		}
			break;
		case 0xA1: {
			uint16_t addr = PeekW((Peek16(reg_pc++) + reg_x) & 0xFF);
			reg_a = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 6;
		}
			break;
		case 0xA2: {
			uint16_t addr = reg_pc++;
			reg_x = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_x & 0x80) | (!reg_x << 1);
			cycles += 2;
		}
			break;
		case 0xA3: {
		}
			break;
		case 0xA4: {
			uint16_t addr = Peek16(reg_pc++);
			reg_y = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_y & 0x80) | (!reg_y << 1);
			cycles += 3;
		}
			break;
		case 0xA5: {
			uint16_t addr = Peek16(reg_pc++);
			reg_a = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 3;
		}
			break;
		case 0xA6: {
			uint16_t addr = Peek16(reg_pc++);
			reg_x = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_x & 0x80) | (!reg_x << 1);
			cycles += 3;
		}
			break;
		case 0xA7: {
		}
			break;
		case 0xA8: {
			reg_y = reg_a;
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0xA9: {
			uint16_t addr = reg_pc++;
			reg_a = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0xAA: {
			reg_x = reg_a;
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 2;
		}
			break;
		case 0xAB: {
		}
			break;
		case 0xAC: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			reg_y = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_y & 0x80) | (!reg_y << 1);
			cycles += 4;
		}
			break;
		case 0xAD: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			reg_a = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0xAE: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			reg_x = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_x & 0x80) | (!reg_x << 1);
			cycles += 4;
		}
			break;
		case 0xAF: {
		}
			break;
		case 0xB0: {
			int8_t tmp4 = (int8_t) (Peek16(reg_pc++));
			uint16_t addr = reg_pc + tmp4;
			if ((reg_ps & 0x01)) {
				cycles += !((reg_pc ^ addr) & 0xFF00) << 1;
				reg_pc = addr;
			}
			cycles += 2;
		}
			break;
		case 0xB1: {
			uint16_t addr = PeekW(Peek16(reg_pc));
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc++;
			//printf("<%x>\n",addr);
			reg_a = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 5;
		}
			break;
		case 0xB2: {
		}
			break;
		case 0xB3: {
		}
			break;
		case 0xB4: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			reg_y = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_y & 0x80) | (!reg_y << 1);
			cycles += 4;
		}
			break;
		case 0xB5: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			reg_a = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0xB6: {
			uint16_t addr = (Peek16(reg_pc++) + reg_y) & 0xFF;
			reg_x = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_x & 0x80) | (!reg_x << 1);
			cycles += 4;
		}
			break;
		case 0xB7: {
		}
			break;
		case 0xB8: {
			reg_ps &= 0xBF;
			cycles += 2;
		}
			break;
		case 0xB9: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc += 2;
			reg_a = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0xBA: {
			reg_x = reg_sp;
			reg_ps &= 0x7D;
			reg_ps |= (reg_x & 0x80) | (!reg_x << 1);
			cycles += 2;
		}
			break;
		case 0xBB: {
		}
			break;
		case 0xBC: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_x) & 0xFF00);
			addr += reg_x;
			reg_pc += 2;
			reg_y = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_y & 0x80) | (!reg_y << 1);
			cycles += 4;
		}
			break;
		case 0xBD: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_x) & 0xFF00);
			addr += reg_x;
			reg_pc += 2;
			reg_a = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_a & 0x80) | (!reg_a << 1);
			cycles += 4;
		}
			break;
		case 0xBE: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc += 2;
			reg_x = Load(addr);
			reg_ps &= 0x7D;
			reg_ps |= (reg_x & 0x80) | (!reg_x << 1);
			cycles += 4;
		}
			break;
		case 0xBF: {
		}
			break;
		case 0xC0: {
			uint16_t addr = reg_pc++;
			int16_t tmp1 = reg_y - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 2;
		}
			break;
		case 0xC1: {
			uint16_t addr = PeekW((Peek16(reg_pc++) + reg_x) & 0xFF);
			int16_t tmp1 = reg_a - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 6;
		}
			break;
		case 0xC2: {
		}
			break;
		case 0xC3: {
		}
			break;
		case 0xC4: {
			uint16_t addr = Peek16(reg_pc++);
			int16_t tmp1 = reg_y - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 3;
		}
			break;
		case 0xC5: {
			uint16_t addr = Peek16(reg_pc++);
			int16_t tmp1 = reg_a - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 3;
		}
			break;
		case 0xC6: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr) - 1;
			Store(addr, tmp1);
			reg_ps &= 0x7D;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			cycles += 5;
		}
			break;
		case 0xC7: {
		}
			break;
		case 0xC8: {
			reg_y++;
			reg_ps &= 0x7D;
			reg_ps |= (reg_y & 0x80) | (!reg_y << 1);
			cycles += 2;
		}
			break;
		case 0xC9: {
			uint16_t addr = reg_pc++;
			int16_t tmp1 = reg_a - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 2;
		}
			break;
		case 0xCA: {
			reg_x--;
			reg_ps &= 0x7D;
			reg_ps |= (reg_x & 0x80) | (!reg_x << 1);
			cycles += 2;
		}
			break;
		case 0xCB: {
		}
			break;
		case 0xCC: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			int16_t tmp1 = reg_y - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 4;
		}
			break;
		case 0xCD: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			int16_t tmp1 = reg_a - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 4;
		}
			break;
		case 0xCE: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr) - 1;
			Store(addr, tmp1);
			reg_ps &= 0x7D;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			cycles += 6;
		}
			break;
		case 0xCF: {
		}
			break;
		case 0xD0: {
			int8_t tmp4 = (int8_t) (Peek16(reg_pc++));
			uint16_t addr = reg_pc + tmp4;
			if (!(reg_ps & 0x02)) {
				cycles += !((reg_pc ^ addr) & 0xFF00) << 1;
				reg_pc = addr;
			}
			cycles += 2;
		}
			break;
		case 0xD1: {
			uint16_t addr = PeekW(Peek16(reg_pc));
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc++;
			int16_t tmp1 = reg_a - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 5;
		}
			break;
		case 0xD2: {
		}
			break;
		case 0xD3: {
		}
			break;
		case 0xD4: {
		}
			break;
		case 0xD5: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			int16_t tmp1 = reg_a - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 4;
		}
			break;
		case 0xD6: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			uint8_t tmp1 = Load(addr) - 1;
			Store(addr, tmp1);
			reg_ps &= 0x7D;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			cycles += 6;
		}
			break;
		case 0xD7: {
		}
			break;
		case 0xD8: {
			reg_ps &= 0xF7;
			cycles += 2;
		}
			break;
		case 0xD9: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc += 2;
			int16_t tmp1 = reg_a - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 4;
		}
			break;
		case 0xDA: {
		}
			break;
		case 0xDB: {
		}
			break;
		case 0xDC: {
		}
			break;
		case 0xDD: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_x) & 0xFF00);
			addr += reg_x;
			reg_pc += 2;
			int16_t tmp1 = reg_a - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 4;
		}
			break;
		case 0xDE: {
			uint16_t addr = PeekW(reg_pc);
			addr += reg_x;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr) - 1;
			Store(addr, tmp1);
			reg_ps &= 0x7D;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			cycles += 6;
		}
			break;
		case 0xDF: {
		}
			break;
		case 0xE0: {
			uint16_t addr = reg_pc++;
			int16_t tmp1 = reg_x - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 2;
		}
			break;
		case 0xE1: {
			uint16_t addr = PeekW((Peek16(reg_pc++) + reg_x) & 0xFF);
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a - tmp1 + (reg_ps & 0x01) - 1;
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 >= 0)
					| (((reg_a ^ tmp1) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 6;
		}
			break;
		case 0xE2: {
		}
			break;
		case 0xE3: {
		}
			break;
		case 0xE4: {
			uint16_t addr = Peek16(reg_pc++);
			int16_t tmp1 = reg_x - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 3;
		}
			break;
		case 0xE5: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a - tmp1 + (reg_ps & 0x01) - 1;
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 >= 0)
					| (((reg_a ^ tmp1) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 3;
		}
			break;
		case 0xE6: {
			uint16_t addr = Peek16(reg_pc++);
			uint8_t tmp1 = Load(addr) + 1;
			Store(addr, tmp1);
			reg_ps &= 0x7D;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			cycles += 5;
		}
			break;
		case 0xE7: {
		}
			break;
		case 0xE8: {
			reg_x++;
			reg_ps &= 0x7D;
			reg_ps |= (reg_x & 0x80) | (!reg_x << 1);
			cycles += 2;
		}
			break;
		case 0xE9: {
			uint16_t addr = reg_pc++;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a - tmp1 + (reg_ps & 0x01) - 1;
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 >= 0)
					| (((reg_a ^ tmp1) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 2;
		}
			break;
		case 0xEA: {
			cycles += 2;
		}
			break;
		case 0xEB: {
		}
			break;
		case 0xEC: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			int16_t tmp1 = reg_x - Load(addr);
			uint8_t tmp2 = tmp1 & 0xFF;
			reg_ps &= 0x7C;
			reg_ps |= (tmp2 & 0x80) | (!tmp2 << 1) | (tmp1 >= 0);
			cycles += 4;
		}
			break;
		case 0xED: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a - tmp1 + (reg_ps & 0x01) - 1;
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 >= 0)
					| (((reg_a ^ tmp1) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 4;
		}
			break;
		case 0xEE: {
			uint16_t addr = PeekW(reg_pc);
			reg_pc += 2;
			uint8_t tmp1 = Load(addr) + 1;
			Store(addr, tmp1);
			reg_ps &= 0x7D;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			cycles += 6;
		}
			break;
		case 0xEF: {
		}
			break;
		case 0xF0: {
			int8_t tmp4 = (int8_t) (Peek16(reg_pc++));
			uint16_t addr = reg_pc + tmp4;
			if ((reg_ps & 0x02)) {
				cycles += !((reg_pc ^ addr) & 0xFF00) << 1;
				reg_pc = addr;
			}
			cycles += 2;
		}
			break;
		case 0xF1: {
			uint16_t addr = PeekW(Peek16(reg_pc));
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc++;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a - tmp1 + (reg_ps & 0x01) - 1;
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 >= 0)
					| (((reg_a ^ tmp1) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 5;
		}
			break;
		case 0xF2: {
		}
			break;
		case 0xF3: {
		}
			break;
		case 0xF4: {
		}
			break;
		case 0xF5: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a - tmp1 + (reg_ps & 0x01) - 1;
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 >= 0)
					| (((reg_a ^ tmp1) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 4;
		}
			break;
		case 0xF6: {
			uint16_t addr = (Peek16(reg_pc++) + reg_x) & 0xFF;
			uint8_t tmp1 = Load(addr) + 1;
			Store(addr, tmp1);
			reg_ps &= 0x7D;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			cycles += 6;
		}
			break;
		case 0xF7: {
		}
			break;
		case 0xF8: {
			reg_ps |= 0x08;
			cycles += 2;
		}
			break;
		case 0xF9: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_y) & 0xFF00);
			addr += reg_y;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a - tmp1 + (reg_ps & 0x01) - 1;
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 >= 0)
					| (((reg_a ^ tmp1) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 4;
		}
			break;
		case 0xFA: {
		}
			break;
		case 0xFB: {
		}
			break;
		case 0xFC: {
		}
			break;
		case 0xFD: {
			uint16_t addr = PeekW(reg_pc);
			cycles += !!(((addr & 0xFF) + reg_x) & 0xFF00);
			addr += reg_x;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr);
			int16_t tmp2 = reg_a - tmp1 + (reg_ps & 0x01) - 1;
			uint8_t tmp3 = tmp2 & 0xFF;
			reg_ps &= 0x3C;
			reg_ps |= (tmp3 & 0x80) | (!tmp3 << 1) | (tmp2 >= 0)
					| (((reg_a ^ tmp1) & (reg_a ^ tmp3) & 0x80) >> 1);
			reg_a = tmp3;
			cycles += 4;
		}
			break;
		case 0xFE: {
			uint16_t addr = PeekW(reg_pc);
			addr += reg_x;
			reg_pc += 2;
			uint8_t tmp1 = Load(addr) + 1;
			Store(addr, tmp1);
			reg_ps &= 0x7D;
			reg_ps |= (tmp1 & 0x80) | (!tmp1 << 1);
			cycles += 6;
		}
			break;
		case 0xFF: {
		}
			break;
		}
//#ifdef DEBUG
//		if (should_irq && !(reg_ps & 0x04)) {
//			should_irq = false;
//			stack[reg_sp --] = reg_pc >> 8;
//			stack[reg_sp --] = reg_pc & 0xFF;
//			reg_ps &= 0xEF;
//			stack[reg_sp --] = reg_ps;
//			reg_pc = PeekW(IRQ_VEC);
//			reg_ps |= 0x04;
//			cycles += 7;
//		}
//		executed_insts ++;
//		if (executed_insts % 6000 == 0) {
//			timer1_cycles += CYCLES_TIMER1;
//			clock_buff[4] ++;
//			if (should_wake_up) {
//				should_wake_up = false;
//				ram_io[0x01] |= 0x01;
//				ram_io[0x02] |= 0x01;
//				reg_pc = PeekW(RESET_VEC);
//			} else {
//				ram_io[0x01] |= 0x08;
//				should_irq = true;
//			}
//		}
//#else
		if (cycles >= timer0_cycles) {
			timer0_cycles += CYCLES_TIMER0;
			timer0_toggle = !timer0_toggle;
			if (!timer0_toggle) {
				AdjustTime();
			}
			if (!IsCountDown() || timer0_toggle) {
				ram_io[0x3D] = 0;
			} else {
				ram_io[0x3D] = 0x20;
				clock_flags &= 0xFD;
			}
			should_irq = true;
		}
		if (should_irq && !(reg_ps & 0x04)) {
			if(enable_debug_switch||enable_dyn_debug){
				printf("doing irq!\n");
			}
			should_irq = false;
			stack[reg_sp --] = reg_pc >> 8;
			stack[reg_sp --] = reg_pc & 0xFF;
			reg_ps &= 0xEF;
			stack[reg_sp --] = reg_ps;
			reg_pc = PeekW(IRQ_VEC);
			reg_ps |= 0x04;
			cycles += 7;
		}
		if (cycles >= timer1_cycles) {
			if (speed_up) {
				timer1_cycles += CYCLES_TIMER1_SPEED_UP;
			} else {
				timer1_cycles += CYCLES_TIMER1;
			}
			clock_buff[4] ++;
			if (should_wake_up) {
				should_wake_up = false;
				ram_io[0x01] |= 0x01;
				ram_io[0x02] |= 0x01;
				reg_pc = PeekW(RESET_VEC);
			} else {
				ram_io[0x01] |= 0x08;
				should_irq = true;
			}
		}
		if(should_irq && (enable_debug_pc ||enable_dyn_debug)&&false)
			printf("should irq!\n");
//#endif
	}

	cycles -= end_cycles;
	timer0_cycles -= end_cycles;
	timer1_cycles -= end_cycles;

	wqx::reg_pc = reg_pc;
	wqx::reg_a = reg_a;
	wqx::reg_ps = reg_ps;
	wqx::reg_x = reg_x;
	wqx::reg_y = reg_y;
	wqx::reg_sp = reg_sp;
}

}
