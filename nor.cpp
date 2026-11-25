#include "comm.h"
#include "ram.h"
#include "state.h"
#include "mem.h"
#include <cassert>
#include "nor.h"

extern nc2k_states_t nc2k_states;
extern WqxRom nc2k_rom;

uint8_t nor_buff[1024*1024];
uint8_t* nor_banks[0x20];

static uint8_t& fp_step = nc2k_states.fp_step;
static uint8_t& fp_type = nc2k_states.fp_type;


//0x28,0x0a ---->2600
//0xd0,0x07 ---->2000
//0xfc,0x03 ---->1020
//'J' --->简体
uint8_t nor_info_block0[]={
0xbd,0xf0,0xd4,0xb6,0xbc,0xfb,'N','C',0xd0,0x07,'J',0x01,0x02,0x03,0x04,0x01,0x01,0x01,0x01,
0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,

//0xbd,0xf0,0xd4,0xb6,0xbc,0xfb,
};
uint8_t nor_info_block[0x200];
enum NOR_CMD{
    NONE=0,
    SW_ID=1,
    BYTE_PROGRAM=2,
    BLOCK_OR_MASS_ERASE=3,
    INFO_BYTE_PROGRAM=4,
    INFO_OR_BMASS_ERASE=5,
    INFO_READ=6,
    POLL_STATUS=7,
    PAGE_PROGRAM=8,
    INFO_PAGE_PROGRAM=9
};

void InitNorInfoBlock(){
    memset(nor_info_block, 0xff, sizeof(nor_info_block));
    assert(sizeof(nor_info_block0) <= sizeof(nor_info_block));
    memcpy(nor_info_block, nor_info_block0, sizeof(nor_info_block0));
    if(nc1020mode){
        nor_info_block[8]=0xfc;
        nor_info_block[9]=0x03;
    }
}
void LoadNor(){
	assert(sizeof(nor_buff) >= NOR_SIZE);
	uint8_t* temp_buff = (uint8_t*)malloc(NOR_SIZE);
	FILE* file = fopen(nc2k_rom.norFlashPath.c_str(), "rb");
	if(file==0){
		printf("nor file [%s] not exist!\n",nc2k_rom.norFlashPath.c_str());
		exit(-1);
	}
    if(0){
        int sz=fread(temp_buff, 1, NOR_SIZE, file);
        int cnt=0;
        fprintf(stderr,"<<sz=%d>>\n",sz);
        for(int i=0;i<sz;i++){
            if(i%264>=7 && i%264<263){
                nor_buff[cnt++]=temp_buff[i];
            }
            //if(cnt>=32768)break;
        }
        fprintf(stderr,"<<cnt=%d>>\n",cnt);
    }else{
        fread(temp_buff, 1, NOR_SIZE, file);
        if(nor_read_format== PHYSICAL_ORDER){
            ProcessBinaryLinear(nor_buff, temp_buff, NOR_SIZE);
        }else if(nor_read_format== WQX2KUTIL){
            ProcessBinaryRev(nor_buff, temp_buff, NOR_SIZE);
        }
        else assert(false);
    }

    if(patch_nc1020tw_nor){
        void try_fix_nc1020tw_baddump();
        try_fix_nc1020tw_baddump();
    }

	free(temp_buff);
	fclose(file);
}

void SaveNor(string file)
{
    if(file.empty()) { file = nc2k_rom.norFlashPath;}
    else file+=".nor";
	uint8_t* temp_buff = (uint8_t*)malloc(NOR_SIZE);
	FILE* fp = fopen(file.c_str(), "wb");
    if(!fp) {
        printf("failed to open file [%s] for writing!\n", file.c_str());
        exit(-1);
    }
    if(nor_write_format== PHYSICAL_ORDER){
	    ProcessBinaryLinear(temp_buff, nor_buff, NOR_SIZE);
    }
    else if(nor_write_format== WQX2KUTIL){
        ProcessBinaryRev(temp_buff, nor_buff, NOR_SIZE);
    }else assert(false);
	fwrite(temp_buff, 1, NOR_SIZE, fp);
	fflush(fp);
	free(temp_buff);
	fclose(fp);
}

void init_nor(){
    memset(&nor_buff,0xff,NOR_SIZE);
    InitNorInfoBlock();
    LoadNor();
    for (uint32_t i=0; i<num_nor_pages; i++) {
		nor_banks[i] = nor_buff + (0x8000 * i);
	}
}
bool in_nor_range(uint16_t addr){
    uint8_t* page = memmap[addr >> 13];
    if(page< nor_buff || page>= nor_buff +NOR_SIZE){
        return false;
    }
    return true;
}
bool read_nor(uint16_t addr, uint8_t &value){
    // the "read any byte" on datasheet should mean "any byte in nor address"
    if (!in_nor_range(addr)) return false;

    if(fp_type==NOR_CMD::INFO_READ && fp_step ==3){
        //printf("read info_block fp_type=%d fp_step=%d addr=%04x\n",fp_type,fp_step,addr);
        value=nor_info_block[addr & 0x1ff];
        return true;
    }
    if(fp_type == NOR_CMD::SW_ID && fp_step==3){
        if(debug_level>=1) printf("FIXME, got NOR_CMD::SW_ID !!!!!!!! addr=%04x\n",addr);
        //assert(false);
        if(addr==0x8000) {
            if(pc1000mode||nc3000mode){
                value= 0xBF; //from wayback
            }else if(nc2000mode||nc1020mode){
                value= 0xC7;// from datasheet but not sure if it's exactly same datasheet
            }else assert(false);
            return true;
        }else if (addr==0x8001){
            if(pc1000mode||nc1020mode||nc2000mode){
                value = 0xD7;
            }else if(nc3000mode){
                value = 0xD8;
            }else assert(false);
            return true;
        }else {
            value= 0xff;
            if(debug_level>=1) printf("got NOR_CMD::SW_ID read of unknow addr addr=%04x\n",addr);
            return false; //todo: should it count as read in nor or not?
        };
    }
    if(fp_type == NOR_CMD::POLL_STATUS && fp_step==3){
        value=0x88;
        if(debug_level>=1) printf("got NOR_CMD::POLL_STATUS, addr=%04x\n",addr);
        return true;
    }
    if (((fp_type == NOR_CMD::BYTE_PROGRAM || fp_type == NOR_CMD::INFO_BYTE_PROGRAM) && fp_step == 4) ||
		((fp_type == NOR_CMD::BLOCK_OR_MASS_ERASE || fp_type == NOR_CMD::INFO_OR_BMASS_ERASE) && fp_step == 6)) {
		//fp_step = 0; //this doesn't respect datasheet??
        value=0x88;
		return true;
	}
    //if(addr==0x8000||addr==0x4000) printf("[possible read nor bs=%02x roabbs=%02x vol=%02x %04x not handled]\n",ram_io[0],ram_io[0xa],ram_io[0x0d],addr);
    return false;
}

void reset_nor_status(){
    fp_step=0;
    fp_type=NOR_CMD::NONE;
}

void write_nor0(uint16_t addr,uint8_t value){
    //printf("[write nor bs=%02x roabbs=%02x vol=%02x %04x %02x]\n",ram_io[0],ram_io[0xa],ram_io[0x0d],addr,value);

    uint8_t bank_idx = ram_io[0x00];

	if(nc1020mode||nc2000mode||nc3000mode){
		if (bank_idx >= 0x80 && addr>=0x4000 && addr<=0xbfff) {
			if(debug_level>=1) printf("oops, suspicious write to nor, bank_idx=%02x, addr=%04x\n",bank_idx, addr);
			/*
			Peek16(addr) = value;
			if (addr == 0x8000 && value == 0xF0) {
        		fp_step = 0;
			}
			return;*/
		}
	}

    if(nc2000mode||nc3000mode){
		//assert(addr>=0x4000&& addr<=0xbfff);
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
            case 0x70: fp_type = NOR_CMD::POLL_STATUS; break;
            case 0xb0: RATE_LIMIT_LOG(3000,5,"WARN: nor page program received, but it's not implemented"); break;
            case 0xb8: RATE_LIMIT_LOG(3000,5,"WARN: nor info page program received, but it's not implemented"); break;
            //case 0xb0: fp_type = NOR_CMD::PAGE_PROGRAM; break;
            //case 0xb8: fp_type = NOR_CMD::INFO_PAGE_PROGRAM; break;
            //default: printf("no new fp type\n");
        	}
            //printf("new fp type=%d\n",fp_type);
            if (fp_type) {
                /*if (fp_type == NOR_CMD::SW_ID) {
					assert(false);
                    //fp_bank_idx = bank_idx;
                    //fp_bak1 = bank[0x0000];
                    //fp_bak2 = bank[0x0001];
                }*/
                fp_step = 3;
                return;
            }
        }
    } else if (fp_step == 3) {
        /*if (fp_type == NOR_CMD::SW_ID) {
            assert(false);
            if (value == 0xF0) {
                //bank[0x0000] = fp_bak1;
                //bank[0x0001] = fp_bak2;
                fp_step = 0;
                fp_type = 0;
                return;
            }
        } else*/ 
        if (fp_type == NOR_CMD::BYTE_PROGRAM) {
            memmap[addr>>13][addr&0x1fff] &= value;
            fp_step = 4;
            if(pc1000mode||nc3000mode) reset_nor_status();
            return;
        } else if (fp_type == NOR_CMD::INFO_BYTE_PROGRAM) {
            nor_info_block[addr & 0x1ff] &= value;
            RATE_LIMIT_LOG(3000,5,"WARN: info byte program at addr=0x%04x, value=0x%02x", addr, value);
            fp_step = 4;
            if(pc1000mode||nc3000mode) reset_nor_status();
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
            printf("[nor] wanna erase all\n");
        	for (uint32_t i=0; i<num_nor_pages; i++) {
                memset(nor_banks[i], 0xFF, 0x8000);
            }
            if (fp_type == NOR_CMD::INFO_OR_BMASS_ERASE) {
                //if it's BMASS erase info block in addition erase info block
                printf("[nor] WARN: wanna erase infoblock size 512 (BMASS)\n");
                memset(nor_info_block, 0xFF, sizeof(nor_info_block));
            }
            fp_step = 6;
            if(pc1000mode||nc3000mode) reset_nor_status();
            return;
        }
        else if (fp_type == NOR_CMD::BLOCK_OR_MASS_ERASE) {
            if (value == 0x30) {
                if(nc2000mode||nc1020mode){
                    printf("[nor] wanna erase size 2048, addr=0x%04x, bs=0x%02x\n",addr, ram_io[0x00]);
                    //memset(bank + (addr - (addr % 0x800) - 0x4000), 0xFF, 0x800);
                    memset(&memmap[addr>>13][addr&0x1800],0xff,0x800);
                }else if(pc1000mode||nc3000mode){
                    printf("[nor] wanna erase size 4096, addr=0x%04x, bs=0x%02x\n",addr, ram_io[0x00]);
                    memset(&memmap[addr>>13][addr&0x1000],0xff,0x1000);
                }else assert(false);
                fp_step = 6;
                if(pc1000mode||nc3000mode) reset_nor_status();
                return;
            }
        } else if (fp_type == NOR_CMD::INFO_OR_BMASS_ERASE) {
            if (value == 0x30) {
                printf("[nor] WARN: wanna erase infoblock size 512\n");
                memset(nor_info_block, 0xFF, sizeof(nor_info_block));
                fp_step = 6;
                if(pc1000mode||nc3000mode) reset_nor_status();
                return;
            }
        }
    }
    if (value == 0xF0) {
        //printf("writing 0xf0 to addr=%04x\n",addr);
        reset_nor_status();
        return;
    }

    // Mid-command sequence (unlock or erase setup): invalid cycle aborts back to read mode.
    // Latched modes (SW_ID/INFO_READ/POLL_STATUS and post-op READY at step 4/6) only exit on 0xF0.
    bool in_cmd_seq = (fp_step == 1 || fp_step == 2) ||
        ((fp_type == NOR_CMD::BLOCK_OR_MASS_ERASE || fp_type == NOR_CMD::INFO_OR_BMASS_ERASE) &&
         (fp_step == 3 || fp_step == 4 || fp_step == 5));
    if (in_cmd_seq) {
        reset_nor_status();
        RATE_LIMIT_LOG(3000,5,"[nor] ERR: error occurs when operate in flash! addr=%04x value=%02x; fp_step=%d tp_type=%d. reset!!",addr,value,fp_step,fp_type);
    }
    else{
        RATE_LIMIT_LOG(3000,5,"[nor] ERR: error occurs when operate in flash! addr=%04x value=%02x; fp_step=%d tp_type=%d.",addr,value,fp_step,fp_type);
    }
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
