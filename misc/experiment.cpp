#include "comm.h"
#include "state.h"
#include "nor.h"
#include "mem.h"

extern nc2k_states_t nc2k_states;

static int &patch_idx=nc2k_states.patch_idx;
static unsigned char *patch_table=nc2k_states.patch_table;

//only for experiment, no effect unless enabled 'patch_table_experiment'
void write_0x3e_0x3f_patchtable_helper(int address,int value){ 
    assert(address==0x3e||address==0x3f);
    assert(nc1020mode||nc2000mode||nc3000mode);
    if(address==0x3e){
        if(value>=0x10 &&value<=0x1f){
            if(enable_assert_for_wqx_software) assert(value==0x10);
            patch_idx=value;
            if(debug_level>=1) printf("[io_v2_write] 0x3e value=%02x\n",value);
            //enable_dyn_debug_next_n=100;
        }
        else{
            patch_idx=0;
        }
    }
    if(address==0x3f){
        if(patch_idx>=0x10 &&patch_idx<=0x1f){
            patch_table[patch_idx]=value;
            patch_idx++;
        }
    }
}

//experiment hacking code, not really needed, only for hardware authentic.
//  disabled by deault. need rewrite
void handle_patch_table_experiment(){
	unsigned char *patch_table=nc2k_states.patch_table;
	int bank_idx = ram_io[0x00];
	if(nc1020mode &&!nc1020tw_mode){
		if(true) {
			static bool patched=false;
			if(bank_idx==0x9d  &&  patch_table[0x1e]==0x9d&&  patch_table[0x1f]==0xa0 && !patched){
				patched=true;
				printf("!!!!aaaa\n");
				for(int i=0;i<256;i++){
					printf("%02x ",memmap[5][i]);
				}
				printf("\n");
				
				//assert(memcmp(memmap[5],nor_buff+491520,0x1000)==0);
				for(int i=0;i<256;i++){
					printf("%02x ",*(nor_buff+491520 +i));
				}
				printf("\n");
				memcpy(memmap[5],nor_buff+491520,0x1000);
			}
		}
		if(true)
		{
			static bool patched=false;
			if(bank_idx==0x8f && patch_table[0x1c]==0x8f&&  patch_table[0x1d]==0x50 && !patched){
				patched=true;
				printf("!!!!bbb\n");
				for(int i=0;i<256;i++){
					printf("%02x ",memmap[2][i+0x1000]);
				}
				printf("\n");
				
				for(int i=0;i<256;i++){
					printf("%02x ",*(nor_buff+495616 +i));
				}
				printf("\n");
				//assert(memcmp(memmap[2]+0x1000,nor_buff+495616,0x1000)==0);

				memcpy(memmap[2]+0x1000,nor_buff+495616,0x1000);
			}
		}
		if(true){
			static bool patched=false;
			if(bank_idx==0x8f && patch_table[0x1a]==0x8f&&  patch_table[0x1b]==0xb0 && !patched){
				patched=true;
				printf("!!!!ccc\n");
				for(int i=0;i<256;i++){
					printf("%02x ",memmap[5][i+0x1000]);
				}
				printf("\n");
				
				
				
				for(int i=0;i<256;i++){
					printf("%02x ",*(nor_buff+499712 +i));
				}
				printf("\n");
				//assert(memcmp(memmap[5]+0x1000,nor_buff+499712,0x1000)==0);


				memcpy(memmap[5]+0x1000,nor_buff+499712,0x1000);
			}
		}
	}

	if(nc1020tw_mode && bank_idx==0x90 && ((ram_io[0x0d]&0x3) ==0)){
		if(true)
		{
			static bool patched=false;
			if(patch_table[0x1e]==0x90&&  patch_table[0x1f]==0x40 && !patched){
				patched=true;
				printf("!!!!aaaa\n");
				for(int i=0;i<256;i++){
					printf("%02x ",memmap[2][i]);
				}
				printf("\n");
				

				
				for(int i=0;i<256;i++){
					printf("%02x ",*(nor_buff+491520+512*1024 +i));
				}
				printf("\n");
				//assert(memcmp(memmap[2],nor_buff+491520+512*1024,0x1000)==0);

				memcpy(memmap[2],nor_buff+491520+512*1024,0x1000);
			}
		}
		if(true)
		{
			static bool patched=false;
			if(patch_table[0x1c]==0x90&&  patch_table[0x1d]==0x50 && !patched){
				patched=true;
				printf("!!!!bbb\n");
				for(int i=0;i<256;i++){
					printf("%02x ",memmap[2][i+0x1000]);
				}
				printf("\n");
				
				
				
				for(int i=0;i<256;i++){
					printf("%02x ",*(nor_buff+495616+512*1024 +i));
				}
				printf("\n");
				//assert(memcmp(memmap[2]+0x1000,nor_buff+495616+512*1024,0x1000)==0);

				memcpy(memmap[2]+0x1000,nor_buff+495616+512*1024,0x1000);
			}
		}
		if(true){
			static bool patched=false;
			if(patch_table[0x1a]==0x90&&  patch_table[0x1b]==0xb0 && !patched){
				patched=true;
				printf("!!!!ccc\n");
				for(int i=0;i<256;i++){
					printf("%02x ",memmap[5][i+0x1000]);
				}
				printf("\n");
				
				
				
				for(int i=0;i<256;i++){
					printf("%02x ",*(nor_buff+499712+512*1024 +i));
				}
				printf("\n");
				//assert(memcmp(memmap[5]+0x1000,nor_buff+499712+512*1024,0x1000)==0);


				memcpy(memmap[5]+0x1000,nor_buff+499712+512*1024,0x1000);
			}
		}
	}	
}

//only used to run the bad dumped nc1020tw rom, not needed for normal rom. disabled by default
void try_fix_nc1020tw_baddump(){
    //this rom is a bad dump, some bytes are erased to 00. 
    //here doing dynamic patch to fix it
    if(nc1020tw_mode){
        nor_buff[32758]=0xe0; //fix boot vector
        
        nor_buff[0x00]=0x60; // nor header
        nor_buff[0x01]=0xea;

        nor_buff[0x02]=0x06; // int $c201
        nor_buff[0x03]=0xd2;

        nor_buff[0x04]=0x48; // int $c202
        nor_buff[0x05]=0xd2;

        nor_buff[0x06]=0xa9; // int $c203
        nor_buff[0x07]=0xd1;

        nor_buff[0x0a]=0xb6; // fix int $c205
        nor_buff[0x0b]=0xc0;

        nor_buff[0x0c]=0xd5; // fix int $c206
        nor_buff[0x0d]=0xc6;

        nor_buff[0x0e]=0x61; // fix int $c207
        nor_buff[0x0f]=0xc7;

        assert(nor_buff[16390]==0x13);
        nor_buff[16391]=0xc2;
        nor_buff[16392]=0xd3;
        nor_buff[16393]=0xc2;
        nor_buff[16394]=0xb5;
        assert(nor_buff[16395]==0xc2);


        int addr=28638;
        assert(nor_buff[addr]==0x48);
        nor_buff[addr++]=0x48;
        assert(nor_buff[addr]==0x29);
        nor_buff[addr++]=0x29;
        nor_buff[addr++]=0x3f;
        nor_buff[addr++]=0x0a;
        nor_buff[addr++]=0xaa;
        nor_buff[addr++]=0xbd;
        nor_buff[addr++]=0x2f;
        nor_buff[addr++]=0xf4;
        nor_buff[addr++]=0x8d;
        nor_buff[addr++]=0xb2;
        nor_buff[addr++]=0x04;
        nor_buff[addr++]=0xa9;
        nor_buff[addr++]=0x00;
        nor_buff[addr++]=0x38;
        nor_buff[addr++]=0xed;
        nor_buff[addr++]=0xb2;
        nor_buff[addr++]=0x04;
        nor_buff[addr++]=0x8d;
        nor_buff[addr++]=0xb2;
        nor_buff[addr++]=0x04;

    }
}
