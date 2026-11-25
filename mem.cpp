#include "comm.h"
#include "ram.h"
#include "state.h"
#include "mem.h"
#include "nor.h"
#include "rom.h"
#include "io.h"
#include "io_new.h"
#include <cassert>

#include "compare/pc1000bus.h"

extern BusPC1000 *bus_pc1000;
extern nc2k_states_t nc2k_states;

uint8_t* memmap[8];
// Unmapped BMI multiplexed ADBUS (MC0/1): open bus holds last AD[7:0] low-byte address (addr & 0xFF).
static uint8_t unmapped_page_adbus[0x2000];
// Unmapped parallel bus (non-ADBUS, e.g. SPDC1064): floating data bus (0xFF).
static uint8_t unmapped_page_float[0x2000];


void init_mem(){	
	for (int i = 0; i < 0x2000; i++) {
		unmapped_page_adbus[i] = i & 0xFF;
		unmapped_page_float[i] = 0xFF;
	}
	memmap[0] = ram00;
	super_switch(); //will segfault if commented out, since some legacy code is access the memmap
}

static inline bool is_unmapped_ramb_hi(uint16_t addr) {
	// BFI_4M (p.13): single SPR4096 (CF[6:3]=0000, CF7=1) only has 4KB SRAM at 2000-2FFF.
	// 3000-3FFF requires a 2nd SPR4096 (CF[6:3]=0001, e.g. nc1020tw); otherwise it is unmapped ADBUS.
	return addr >= 0x3000 && addr < 0x4000
		&& memmap[1] == ram_b
		&& !(nc1020mode && nc1020tw_mode);
}

//this is a simplified version of memory access without side effected (e.g. io, nor)
uint8_t & Peek16(uint16_t addr) {
	if(addr<0x40){
		return ram_io[addr];
	}else if(addr<0x80){
		return ram_40[addr-0x40];
	}
	if (is_unmapped_ramb_hi(addr)) {
		return unmapped_page_adbus[addr & 0x1FFF];
	}
	return memmap[addr >> 13][addr & 0x1FFF];
}
uint16_t PeekW(uint16_t addr) {
	return Peek16(addr) | (Peek16((uint16_t) (addr + 1)) << 8);
}
uint8_t Load(uint16_t addr) {
	{
		bool dummy_io_for_read(uint16_t addr, uint8_t &value);
		uint8_t value;
		if(dummy_io_for_read(addr, value)){
			return value;
		}
	}

	{
		uint8_t value;
		if (read_nor(addr,value)){
			return value;
		}
	}

	if (addr < IO_LIMIT) {
		if(io_version== IO_V1){
			return io_read[addr](addr);
		} else if (io_version == IO_V2) {
			return io_v2_read(addr);
		} else if (io_version == IO_EMUX) {
			return bus_pc1000->in(addr);
		}else{
			assert(false);
		}
	}

	if (addr <0x80) {
		return ram_40[addr-0x40];
	}

	/*if(nc1020mode)
	{
		bool& wake_up_pending = nc1020_states.pending_wake_up;
		uint8_t& wake_up_key = nc1020_states.wake_up_flags;
		if (addr == 0x45F && wake_up_pending) {
			wake_up_pending = false;
			memmap[0][0x45F] = wake_up_key;
		}
	}*/
	if (is_unmapped_ramb_hi(addr)) {
		return unmapped_page_adbus[addr & 0x1FFF];
	}
	return memmap[addr >> 13][addr & 0x1FFF];
}
void Store(uint16_t addr, uint8_t value) {
	{
		bool dummy_io_for_write(uint16_t addr, uint8_t value);
		if(dummy_io_for_write(addr, value)){
			return;
		}
	}

	// write to nor_flash address space.
    // there must select a nor_bank.
	if (write_nor(addr,value)){
		return;
	}

	if (addr < IO_LIMIT) {
		if(io_version== IO_V1){
			io_write[addr](addr, value);
		}else if (io_version == IO_V2) {
			io_v2_write(addr, value);
		}else if (io_version == IO_EMUX) {
			bus_pc1000->out(addr, value);
		}else {
			assert(false);
		}

		return;
	}

	if (addr <0x80) {
		ram_40[addr-0x40] =value;
		return;
	}

	if (addr < 0x2000) {
		memmap[0][addr & 0x1FFF] = value;
		return;
	}
	if (is_unmapped_ramb_hi(addr)) {
		return;
	}
	uint8_t* page = memmap[addr >> 13];
	if (page == nc2k_states.ram_b/*ramb*/ || page == nc2k_states.ram_b2/*ramb2*/) {
		page[addr & 0x1FFF] = value;
		return;
	}
	if (page >=nc2k_states.ram && page<nc2k_states.ram+ sizeof(nc2k_states.ram) ) {
		page[addr & 0x1FFF] = value;
		return;
	}
	
	if (page >= nc2k_states.ext_ram && page < nc2k_states.ext_ram+sizeof(nc2k_states.ext_ram)) {
		//printf("write!!!");
		page[addr & 0x1FFF] = value;
		return;
	}
	if (addr >= 0xE000) {
		return;
	}


}
extern "C"{
uint8_t CLoad(uint16_t addr) {
	return Load(addr);
}
void CStore(uint16_t addr, uint8_t value){
	return Store(addr,value);
}
}

uint8_t* GetBank(uint8_t bank_idx){

	if((ram_io[0x0D]&0x3)!=0) {
		if(debug_level>=99) printf("vol=%02x!!!!!!!!!!!!\n",ram_io[0x0D]&0x3);
	}

	uint8_t volume_idx = ram_io[0x0D] & 0x03;
	bool roa = ram_io[0x0a] & 0x80;

	if (nc3000mode) {
		// SPDC1064: XM0CF=000 maps XM0CEZ to ROA=0, Vol=00, Bank=00h-FFh.
		// 1MB NOR (MA0-MA19) wraps around across 00h-FFh.
		if (!roa && volume_idx == 0) {
			return nor_banks[bank_idx % num_nor_pages];
		}
		return NULL;
	}

    if (bank_idx < num_nor_pages) { 
		if(bank_idx >= 0x10 && nc1020mode && nc1020tw_mode){
			//printf("bidx=%02x\n",bank_idx);
		}

		if(roa || volume_idx != 0){
			if(debug_level>=2) printf("oops, GetBank bank_idx=%02x roa=%d vol=%d\n",bank_idx, roa, volume_idx);
			return NULL;
		}
    	return nor_banks[bank_idx];
    } else if (bank_idx >= 0x80) {
		if(nc1020mode){
			if(roa){
				return NULL;
			}
			if (volume_idx == 0x00) {
				return rom_volume0[bank_idx];
			} else if (volume_idx == 0x01) {
				return rom_volume1[bank_idx];
			} else if (volume_idx == 0x02) {
				return rom_volume2[bank_idx];
			}
			return NULL;
		}

		if(nc2000mode){
			//printf("<%x\n>",bank_idx);
			//assert(bank_idx==0x80);
			// SPDC1024: XM1CF=00011b maps XM1CEZ to Vol=0, ROA=1, Bank=80h-FFh (32KB ext_ram wraps around).
			if(roa && volume_idx == 0){
				return nc2k_states.ext_ram;
			}
			return NULL;
		}
    }else{
		if(debug_level>=1) printf("oops GetBank bank_idx=%02x invalid roa_bbs=%02x \n",bank_idx, ram_io[0x0a]);
	}
    return NULL;
}

void SwitchBank_2345(){
	uint8_t bank_idx = ram_io[0x00];
	uint8_t volume_idx = ram_io[0x0D] & 0x03;

	if(nc3000mode && volume_idx == 0) {
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

	if(pc1000mode){
		uint8_t* bank;
		if(ram_io[0x0a]&0x80){
			bank = nor_banks[bank_idx&0xf];
		}else{
			if (ram_io[0x0d]&0x1){
				bank = rom_volume1[bank_idx];
			}
			else{
				bank = rom_volume0[bank_idx];
			}	
		}

		if(bank_idx!=0 || (ram_io[0x0a]&0x80)){
			memmap[2] = bank + 0x4000;
			memmap[3] = bank + 0x6000;
		}else {
			if(enable_assert_for_wqx_software) assert((ram_io[0x0d]&0x3) ==0 || (ram_io[0x0d]&0x3) ==1);
			if (ram_io[0x0d]&0x1){
				memmap[2] = nor_banks[0] + 0x4000;
				memmap[3] = nor_banks[0] + 0x6000;
			}else {
				memmap[2] = ram04;
				memmap[3] = ram04;
			}
		}

		memmap[4] = bank + 0x0000;
		memmap[5] = bank + 0x2000;
		return; //pc1000mode nerver run to below
	}



	uint8_t* bank = GetBank(bank_idx);
	if(bank_idx<0x80 && ram_io[0x0a]&0x80){
		if(debug_level>=2)printf("oops, bank_idx=%02x ram_io[0x0a]&0x80 is true\n",bank_idx);
	}
	if(bank == NULL) {
		uint8_t* unmapped = nc3000mode ? unmapped_page_float : unmapped_page_adbus;
		memmap[2] = unmapped;
		memmap[3] = unmapped;
		memmap[4] = unmapped;
		memmap[5] = unmapped;
	} else {
		memmap[2] = bank + 0x4000;
		memmap[3] = bank + 0x6000;
		memmap[4] = bank + 0x0000;
		memmap[5] = bank + 0x2000;
	}

	if(nc3000mode){
		if(bank_idx==0 && volume_idx==0){
			memmap[2]=ram04;
			memmap[3]=ram06;
		}
	}
	if(nc1020mode||nc2000mode){
		if(bank_idx==0 && volume_idx==0){
			memmap[2]=ram04;
			memmap[3]=ram04;
		}
	}

	if(nc1020mode){
		/*if(bank_idx==0x80){     //this is incorrect for sure, dictionary result got corrupted
				memmap[2] = nor_banks[0] + 0x4000;
				memmap[3] = nor_banks[0] + 0x6000;
		}*/

	}

	if(patch_table_experiment){
		void handle_patch_table_experiment();
		handle_patch_table_experiment();
	}
}

uint8_t** GetVolumm(uint8_t volume_idx){
	if(nc2000mode||nc1020mode||nc3000mode){
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
	uint8_t volume_idx = ram_io[0x0d] & 0x03;
	bool ramb = ram_io[0x0d] & 0x04;
	if(nc1020mode || nc2000mode){
		if(!ramb){
			memmap[1] = ram02;
		} else if(volume_idx == 0){
			memmap[1] = ram_b;
		} else {
			// CE_R1..CE_R3: no chip on Vol 1..3 -> MC0/1 ADBUS open bus
			memmap[1] = unmapped_page_adbus;
		}
	}
	if(nc3000mode){
		memmap[1] = ram02;  //todo add RAMS (but it works without RAMS  logic)
	}
	if(pc1000mode){
		memmap[1] = ram02;
	}
}

void SwitchBbsBios_67(){
	uint8_t volume_idx = ram_io[0x0D] & 0x03;
	bool exc = ram_io[0x1A] & 0x80;

	if((nc2000mode || nc1020mode || nc3000mode) && exc && volume_idx != 0){
		// EXC=1 & Vol=1..3 (RAMV=0): C000-DFFF (incl. BBS=1) and E000-FFFF map to external Vol 1..3 (unmapped)
		uint8_t* unmapped = nc3000mode ? unmapped_page_float : unmapped_page_adbus;
		memmap[6] = unmapped;
		memmap[7] = unmapped;
		return;
	}

	uint8_t** candidate_for_bbs;
	uint8_t* bbs_pages[0x10];
	if(pc1000mode){
		candidate_for_bbs = GetVolumm(volume_idx);
	}

	if(nc2000mode||nc3000mode||nc1020mode){
		candidate_for_bbs = nor_banks;
	}

	for (int i=0; i<4; i++) {
		bbs_pages[i * 4] = candidate_for_bbs[i]+0x4000;
		bbs_pages[i * 4 + 1] = candidate_for_bbs[i] + 0x6000;
		bbs_pages[i * 4 + 2] = candidate_for_bbs[i] + 0x0000;
		bbs_pages[i * 4 + 3] = candidate_for_bbs[i] + 0x2000;
	}

	uint8_t bbs_idx = ram_io[0x0A]&0x0f;
	memmap[6] = bbs_pages[bbs_idx];
	memmap[7] = bbs_pages[1];


	if(bbs_idx ==1){
		if(nc1020mode){
			memmap[6] = ram04;
		}
		if(nc2000mode){
			memmap[6]=ram04;
		}
		if(nc3000mode){
			memmap[6]=ram06;
		}

		if(pc1000mode){
			if(ram_io[0x0d]&0x01){
				memmap[6] = nor_banks[0]+0x6000;
			}else{
				memmap[6] = ram04;
			}
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
			//////printf("ill bs %x\n",bs);
		}
	}
	if(nc1020mode||nc2000mode||nc3000mode){  
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
			if(enable_oops){
				printf("oops3!\n");
				printf("oops, (ramb_vol&0x03)!=0 ramb_vol=%x\n",ramb_vol);
			}
			//assert(false);
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
