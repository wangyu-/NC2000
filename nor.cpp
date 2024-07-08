#include "comm.h"
#include "state.h"
#include "mem.h"

extern nc1020_states_t nc1020_states;
extern WqxRom nc1020_rom;

uint8_t nor_buff[NOR_SIZE];
uint8_t* nor_banks[0x20];

static uint8_t& fp_step = nc1020_states.fp_step;
static uint8_t& fp_type = nc1020_states.fp_type;
static uint8_t& fp_bank_idx = nc1020_states.fp_bank_idx;
static uint8_t& fp_bak1 = nc1020_states.fp_bak1;
static uint8_t& fp_bak2 = nc1020_states.fp_bak2;
static uint8_t* fp_buff = nc1020_states.fp_buff;


void LoadNor(){
	uint8_t* temp_buff = (uint8_t*)malloc(NOR_SIZE);
	FILE* file = fopen(nc1020_rom.norFlashPath.c_str(), "rb");
	if(nc2000 && file==0){
		printf("nor file [%s] not exist!\n",nc1020_rom.norFlashPath.c_str());
		exit(-1);
	}
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

void init_nor(){
    memset(&nor_buff,0,sizeof(nor_buff));
    LoadNor();
    for (uint32_t i=0; i<num_nor_pages; i++) {
		nor_banks[i] = nor_buff + (0x8000 * i);
	}
}

bool read_nor(uint16_t addr, uint8_t &value){
    if (((fp_step == 4 && fp_type == 2) ||
		(fp_step == 6 && fp_type == 3)) &&
		(addr >= 0x4000 && addr < 0xC000)) {
		fp_step = 0;
        value=0x88;
		return true;
	}
    return false;
}

void write_nor(uint16_t addr,uint8_t value){
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
        	for (uint32_t i=0; i<num_nor_pages; i++) {
				printf("wanna erase all\n");
                memset(nor_banks[i], 0xFF, 0x8000);
            }
            if (fp_type == 5) {
                printf("wanna erase size 256 A\n");
                memset(fp_buff, 0xFF, 0x100);
            }
            fp_step = 6;
            return;
        }
        if (fp_type == 3) {
            if (value == 0x30) {
				printf("wanna erase size 2048\n");
                memset(bank + (addr - (addr % 0x800) - 0x4000), 0xFF, 0x800);
                fp_step = 6;
                return;
            }
        } else if (fp_type == 5) {
            if (value == 0x48) {
                printf("wanna erase size 256 B\n");
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
