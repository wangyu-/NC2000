#include "comm.h"
#include "state.h"
#include "mem.h"
#include "nor.h"
#include "rom.h"
#include "io.h"

uint8_t* bbs_pages[0x10];
uint8_t* memmap[8];


extern nc1020_states_t nc1020_states;

uint8_t* ram_buff = nc1020_states.ram;
uint8_t* stack = ram_buff + 0x100;
uint8_t* ram_io = ram_buff;
uint8_t* ram_40 = ram_buff + 0x40;
uint8_t* ram00 = ram_buff;
uint8_t* ram02 = ram_buff + 0x2000;
uint8_t* ram_b = ram_buff + 0x4000;
uint8_t* ram04 = ram_buff + 0x6000;


void init_mem(){	
	memmap[0] = ram00;
	super_switch();
}

/*
inline uint8_t & Peek8(uint8_t addr) {
	return ram_buff[addr];
}*/
uint8_t & Peek16(uint16_t addr) {
	return memmap[addr >> 13][addr & 0x1FFF];
}
uint16_t PeekW(uint16_t addr) {
	return Peek16(addr) | (Peek16((uint16_t) (addr + 1)) << 8);
}
uint8_t Load(uint16_t addr) {
	if (addr < IO_LIMIT) {
		return io_read[addr](addr);
	}

	{
		uint8_t value;
		if (read_nor(addr,value)){
			return value;
		}
	}

	{
		bool& wake_up_pending = nc1020_states.pending_wake_up;
		uint8_t& wake_up_key = nc1020_states.wake_up_flags;
		if (addr == 0x45F && wake_up_pending) {
			wake_up_pending = false;
			memmap[0][0x45F] = wake_up_key;
		}
	}
	return Peek16(addr);
}
void Store(uint16_t addr, uint8_t value) {
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
	return write_nor(addr,value);

}

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
	if(enable_debug_switch)printf("tick=%llx pc=%x bs=%x roa_bbs=%x ramb_vol=%x\n",tick, nc1020_states.cpu.reg_pc,bs, roa_bbs , ramb_vol);

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
