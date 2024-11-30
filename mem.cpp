#include "comm.h"
#include "ram.h"
#include "state.h"
#include "mem.h"
#include "nor.h"
#include "rom.h"
#include "io.h"
#include <cassert>

uint8_t* memmap[8];


extern nc1020_states_t nc1020_states;

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
	{
		bool dummy_io(uint16_t addr, uint8_t &value);
		uint8_t value;
		if(dummy_io(addr, value)){
			return value;
		}
	}
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
	if (page == ram_b/*ramb*/ || page == ram04/*ram04*/ ||page == ram06 ||page ==ram02||page==ram00) {
		//printf("zxczxc\n");
		page[addr & 0x1FFF] = value;
		return;
	}
	if (page == ram08 ||page == ram0a ||page ==ram0c||page==ram0e) {
		page[addr & 0x1FFF] = value;
		return;
	}
	
	if (page == nc1020_states.ext_ram|| page == nc1020_states.ext_ram+0x2000  ||page == nc1020_states.ext_ram+0x4000|| page == nc1020_states.ext_ram+0x6000) {
		//printf("write!!!");
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
extern "C"{
uint8_t Load2(uint16_t addr) {
	return Load(addr);
}
void Store2(uint16_t addr, uint8_t value){
	return Store(addr,value);
}
}

uint8_t* GetBank(uint8_t bank_idx){
	if (pc1000mode){
		if(ram_io[0x0a] &80){
			return nor_banks[bank_idx&0xf];
		}else{
			uint8_t volume_idx = ram_io[0x0D];
			if (volume_idx & 0x01) {
				return rom_volume1[bank_idx];
			}else{
				return rom_volume0[bank_idx];
			}
		}
	}

	uint8_t volume_idx = ram_io[0x0D];
    if (bank_idx < num_nor_pages) {
    	return nor_banks[bank_idx];
    } else if (bank_idx >= 0x80) {
		if(nc3000){
			return NULL;
		}
		if(nc1020mode){
			if (volume_idx & 0x01) {
				return rom_volume1[bank_idx];
			} else if (volume_idx & 0x02) {
				return rom_volume2[bank_idx];
			} else {
				return rom_volume0[bank_idx];
			}
		}

		if(nc2000||nc3000){
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

void SwitchBank_2345(){
	uint8_t bank_idx = ram_io[0x00];

	if(nc3000) {
		if(bank_idx ==0 && ram_io[0x0a]&0x80) {
				memmap[2] = ram04;
    			memmap[3] = ram06;
    			memmap[4] = ram00;
                memmap[5] = ram02;
				return;
		}
		if(bank_idx ==1 && ram_io[0x0a]&0x80) {
				memmap[2] = ram0c;
    			memmap[3] = ram0e;
    			memmap[4] = ram08;
                memmap[5] = ram0a;
				return;
		}
	}
	uint8_t* bank = GetBank(bank_idx);
	if(bank== NULL) return;
	
    memmap[2] = bank;
    memmap[3] = bank + 0x2000;
    memmap[4] = bank + 0x4000;
    memmap[5] = bank + 0x6000;

	if(bank_idx==0){
		memmap[2]=ram04;
		memmap[3]=ram06;
	}


}

uint8_t** GetVolumm(uint8_t volume_idx){
	if(nc1020mode){
		if ((volume_idx & 0x03) == 0x01) {
			return rom_volume1;
		} else if ((volume_idx & 0x03) == 0x03) {
			return rom_volume2;
		} else {
			return rom_volume0;
		}
		assert(false);
	}
	if(pc1000mode){
		if ((volume_idx & 0x01) ) {
			return rom_volume1;
		} else {
			return rom_volume0;
		}
		assert(false);
	}
	assert(false);
}
void Switch0x2000(){
	if(nc1020mode){
		//memmap[1] = (roa_bbs & 0x04 ? ram_b : ram02); // this is wrong???? should be ram_io[0x0d]&0x04
		memmap[1] = (ram_io[0x0d]&0x04 ? ram_b : ram02);
	}
	if(nc2000){
		memmap[1] = (ram_io[0x0d]&0x04 ? ram_b : ram02);
	}
	if(nc3000){
		memmap[1] = ram02;  //todo add RAMS (but it works without RAMS  logic)
	}
}

void SwitchBbsBios_67(){
	uint8_t** candidate_for_bbs;
	uint8_t* bbs_pages[0x10];
	if(nc1020mode||pc1000mode){
		uint8_t volume_idx = ram_io[0x0D];
		candidate_for_bbs = GetVolumm(volume_idx);
	}

	if(nc2000||nc3000){
		candidate_for_bbs = nor_banks;
	}

	for (int i=0; i<4; i++) {
		bbs_pages[i * 4] = candidate_for_bbs[i];
		bbs_pages[i * 4 + 1] = candidate_for_bbs[i] + 0x2000;
		bbs_pages[i * 4 + 2] = candidate_for_bbs[i] + 0x4000;
		bbs_pages[i * 4 + 3] = candidate_for_bbs[i] + 0x6000;
	}

	uint8_t bbs_idx = ram_io[0x0A]&0x0f;
	memmap[6] = bbs_pages[bbs_idx];
	memmap[7] = bbs_pages[1];


	if(bbs_idx ==1){
		if(nc1020mode){
			memmap[6] = ram04;
		}
		if(nc2000){
			memmap[6]=ram04;
		}
		if(nc3000){
			memmap[6]=ram06;
		}
	}
}
void SwitchZP40(){
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
void SwitchCheck(){
	//only for checking, don't put switch code inside
	uint8_t roa_bbs=ram_io[0x0a];
	uint8_t ramb_vol=ram_io[0x0d];
	uint8_t bs=ram_io[0x00];
	if(nc1020mode||pc1000mode){ 
		if(bs<0x80 &&bs>=num_nor_pages) {
			printf("ill bs %x\n",bs);
		}
	}
	if(nc2000||nc3000){  
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
}
void super_switch(){
	///////////if(enable_debug_switch)printf("tick=%llx pc=%x bs=%x roa_bbs=%x ramb_vol=%x\n",tick, nc1020_states.cpu.reg_pc,bs, roa_bbs , ramb_vol);
	SwitchCheck();
	Switch0x2000();
	SwitchBbsBios_67();
	SwitchBank_2345();
	SwitchZP40();
}
