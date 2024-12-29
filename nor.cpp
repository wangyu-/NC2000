#include "comm.h"
#include "state.h"
#include "mem.h"

extern nc1020_states_t nc1020_states;
extern WqxRom nc1020_rom;

uint8_t nor_buff[NOR_SIZE];
uint8_t* nor_banks[num_nor_pages];
extern uint8_t* memmap[8];

static uint8_t& fp_step = nc1020_states.fp_step;
static uint8_t& fp_type = nc1020_states.fp_type;
//static uint8_t& fp_bank_idx = nc1020_states.fp_bank_idx;
//static uint8_t& fp_bak1 = nc1020_states.fp_bak1;
//static uint8_t& fp_bak2 = nc1020_states.fp_bak2;
//static uint8_t* fp_buff = nc1020_states.fp_buff;

static uint8_t nor_info_block[0x100]={
0xdb,0xf0,0xd4,0xb6,0xbc,0xfb,'N','C','2','0','0','0',1,1,1,1,1,1,1,1
};
enum NOR_CMD{
    NONE=0,
    SW_ID=1,
    BYTE_PROGRAM=2,
    BLOCK_OR_MASS_ERASE=3,
    INFO_BYTE_PROGRAM=4,
    INFO_OR_BMASS_ERASE=5,
    INFO_READ=6,
};
void LoadNor(){
	uint8_t* temp_buff = (uint8_t*)malloc(NOR_SIZE);
	FILE* file = fopen(nc1020_rom.norFlashPath.c_str(), "rb");
	if(file==0){
		printf("nor file [%s] not exist!\n",nc1020_rom.norFlashPath.c_str());
		exit(-1);
	}
	fread(temp_buff, 1, NOR_SIZE, file);
    if(nor_read_format== PHYSICAL_ORDER){
	    ProcessBinaryLinear(nor_buff, temp_buff, NOR_SIZE);
    }else if(nor_read_format== WQX2KUTIL){
        ProcessBinaryRev(nor_buff, temp_buff, NOR_SIZE);
    }
    else assert(false);
	free(temp_buff);
	fclose(file);
}

void SaveNor(){
	uint8_t* temp_buff = (uint8_t*)malloc(NOR_SIZE);
	FILE* file = fopen(nc1020_rom.norFlashPath.c_str(), "wb");
    if(nor_write_format== PHYSICAL_ORDER){
	    ProcessBinaryLinear(temp_buff, nor_buff, NOR_SIZE);
    }
    else if(nor_write_format== WQX2KUTIL){
        ProcessBinaryRev(temp_buff, nor_buff, NOR_SIZE);
    }else assert(false);
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
bool in_nor_range(uint16_t addr){
    uint8_t* page = memmap[addr >> 13];
    if(page< nor_buff || page>= nor_buff +sizeof(nor_buff)){
        return false;
    }
    return true;
}
bool read_nor(uint16_t addr, uint8_t &value){
    // the "read any byte" on datasheet should mean "any byte in nor address"
    if (!in_nor_range(addr)) return false;

    if(fp_type==NOR_CMD::INFO_READ && fp_step ==3){
        printf("read fp_type=%d fp_step=%d addr=%04x\n",fp_type,fp_step,addr);
        value=nor_info_block[addr%0x100];
        return true;
    }
    if (((fp_type == NOR_CMD::BYTE_PROGRAM && fp_step == 4) ||
		(fp_type == NOR_CMD::BLOCK_OR_MASS_ERASE && fp_step == 6))) {
		//fp_step = 0; //this doesn't respect datasheet??
        value=0x88;
		return true;
	}
    return false;
}


void write_nor0(uint16_t addr,uint8_t value){

    uint8_t bank_idx = ram_io[0x00];

	if(nc2000mode||nc3000mode){
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

    if(nc2000mode||nc3000mode){
		//assert(addr>=0x4000&& addr<=0xbfff);
	}
    if (bank_idx >= num_nor_pages) {
        return;
    }

    //uint8_t* bank = nor_banks[bank_idx];

    bool addr_is_0x5555 = (addr == 0x5555)||(addr == 0xd555);

    if (fp_step == 0) {
        assert(fp_type==NOR_CMD::NONE);
        if (addr_is_0x5555 && value == 0xAA) {
            fp_step = 1;
            return;
        }
    }
    if (fp_step == 1) {
        assert(fp_type==NOR_CMD::NONE);
        if (addr == 0xAAAA && value == 0x55) {
        	fp_step = 2;
            return;
        }
    } else if (fp_step == 2) {
        assert(fp_type==NOR_CMD::NONE);
        if (addr_is_0x5555) {
            fp_type=NOR_CMD::NONE;
        	switch (value) {
        	case 0x90: fp_type = NOR_CMD::SW_ID; break;
        	case 0xA0: fp_type = NOR_CMD::BYTE_PROGRAM ; break;
        	case 0x80: fp_type = NOR_CMD::BLOCK_OR_MASS_ERASE; break;
        	case 0xA8: fp_type = NOR_CMD::INFO_BYTE_PROGRAM; break;
        	case 0x88: fp_type = NOR_CMD::INFO_OR_BMASS_ERASE; break;
        	case 0x78: fp_type = NOR_CMD::INFO_READ; break;
            //default: printf("no new fp type\n");
        	}
            //printf("new fp type=%d\n",fp_type);
            if (fp_type) {
                if (fp_type == NOR_CMD::SW_ID) {
					assert(false);
                    //fp_bank_idx = bank_idx;
                    //fp_bak1 = bank[0x0000];
                    //fp_bak2 = bank[0x0001];
                }
                fp_step = 3;
                return;
            }
        }
    } else if (fp_step == 3) {
        if (fp_type == NOR_CMD::SW_ID) {
            assert(false);
            /*
            if (value == 0xF0) {
                //bank[0x0000] = fp_bak1;
                //bank[0x0001] = fp_bak2;
                fp_step = 0;
                fp_type = 0;
                return;
            }*/
        } else if (fp_type == NOR_CMD::BYTE_PROGRAM) {
            memmap[addr>>13][addr&0x1fff] &= value;
            fp_step = 4;
            return;
        } else if (fp_type == NOR_CMD::INFO_BYTE_PROGRAM) {
            nor_info_block[addr & 0xFF] &= value;
            fp_step = 4;
            return;
        } else if (fp_type == NOR_CMD::BLOCK_OR_MASS_ERASE || fp_type == NOR_CMD::INFO_OR_BMASS_ERASE) {
            if (addr_is_0x5555 && value == 0xAA) {
                fp_step = 4;
                return;
            }
        }
    } else if (fp_step == 4) {
        if (fp_type == NOR_CMD::BLOCK_OR_MASS_ERASE || fp_type == NOR_CMD::INFO_OR_BMASS_ERASE) {
            if (addr == 0xAAAA && value == 0x55) {
                fp_step = 5;
                return;
            }
        }
    } else if (fp_step == 5) {
		assert(fp_type== NOR_CMD::BLOCK_OR_MASS_ERASE||fp_type== NOR_CMD::INFO_OR_BMASS_ERASE);
        if (addr_is_0x5555 && value == 0x10) {
            //if write to 0x5555 then it's MASS_ERASE or BMASS_ERASE
        	for (uint32_t i=0; i<num_nor_pages; i++) {
				printf("wanna erase all\n");
                memset(nor_banks[i], 0xFF, 0x8000);
            }
            if (fp_type == 5) {
                //if it's BMASS erase info block in addition erase info block
                printf("wanna erase infoblock size 256\n");
                memset(nor_info_block, 0xFF, 0x100);
            }
            fp_step = 6;
            return;
        }
        else if (fp_type == BLOCK_OR_MASS_ERASE) {
            if (value == 0x30) {
                if(nc2000mode||nc1020mode){
                    printf("wanna erase size 2048, addr= %04x\n",addr);
                    //memset(bank + (addr - (addr % 0x800) - 0x4000), 0xFF, 0x800);
                    memset(&memmap[addr>>13][addr&0x1800],0xff,0x800);
                }else if(pc1000mode||nc3000mode){
                    printf("wanna erase size 4096, addr= %04x\n",addr);
                    memset(&memmap[addr>>13][addr&0x1000],0xff,0x1000);
                }else assert(false);
                fp_step = 6;
                return;
            }
        } else if (fp_type == INFO_OR_BMASS_ERASE) {
            if (value == 0x30) {
                printf("wanna erase infoblock size 256 B\n");
                memset(nor_info_block, 0xFF, 0x100);
                fp_step = 6;
                return;
            }
        }
    }
    if (value == 0xF0) {
        printf("writing 0xf0 to addr=%04x\n",addr);
        fp_step = 0;
        fp_type = 0;
        return;
    }

    printf("error occurs when operate in flash! addr=%04x value=%02x; fp_step=%d tp_type=%d\n",addr,value,fp_step,fp_type);
}

bool write_nor(uint16_t addr, uint8_t value){
    uint8_t* page = memmap[addr >> 13];
    if(value==0xf0){
        //printf("writing 0xf0 to addr=%04x\n",addr);
    }
    /*if(fp_type==6&&fp_step==3){
        write_nor0(addr,value);
    }*/
    if (!in_nor_range(addr)) return false;
    write_nor0(addr,value);
    return true;
}
