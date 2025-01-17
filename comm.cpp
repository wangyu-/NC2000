#include "comm.h"

string inject_code;
uint64_t tick=0;

bool enable_dyn_debug=false;
bool enable_debug_nand=false;

bool enable_debug_switch=false;
bool enable_debug_pc=false;
bool enable_oops=false;
bool enable_inject=false;

bool wanna_inject=false;
bool injected=false;

WqxRom nc1020_rom = {
    //.romPath = "./obj_lu.bin",
    //.norFlashPath = "./nc1020.fls",
    //.statesPath = "./nc1020.sts", //not used at all
};


void rom_switcher(){
    if(nc1020mode){
        nc1020_rom.romPath = "./obj_lu.bin";
        nc1020_rom.norFlashPath = "./nc1020.fls";
    }
    if(nc2000mode){
        nc1020_rom.nandFlashPath = "./phy_ggvsimformat_2000.nand";
        nc1020_rom.norFlashPath = "./phy_ggvsimformat_2000.nor";  

        if (nc2000_use_2600_rom){
                nc1020_rom.nandFlashPath = "./phy_ggvsimformat_2600.nand";
                nc1020_rom.norFlashPath = "./phy_ggvsimformat_2600.nor";  
                if(nc2600_nandmagic_ggvsim){
                    nc1020_rom.nandFlashPath = "./2600nand.bin";
                    nc1020_rom.norFlashPath = "./2600nor.bin";  
                }

        }
    }

    if(nc3000mode){
        nc1020_rom.nandFlashPath = "./nc3000.nand";
        nc1020_rom.norFlashPath = "./nc3000.nor"; 
    }

    if(pc1000mode){
        nc1020_rom.romPath = "./cc800/brom.bin";
        nc1020_rom.norFlashPath = "./cc800/pc1000emux.fls"; 
    }
}

void ProcessBinaryRev(uint8_t* dest, uint8_t* src, uint32_t size){
	uint32_t offset = 0;
    while (offset < size) {
        memcpy(dest + offset + 0x4000, src + offset, 0x4000);
        memcpy(dest + offset, src + offset + 0x4000, 0x4000);
        offset += 0x8000;
    }
}

void ProcessBinaryLinear(uint8_t* dest, uint8_t* src, uint32_t size){
	uint32_t offset = 0;
    while (offset < size) {
        memcpy(dest + offset , src + offset, 0x4000);
        memcpy(dest + offset + 0x4000, src + offset + 0x4000, 0x4000);
        offset += 0x8000;
    }
}


void read_file(string name,vector<char> &v){
    FILE *f = fopen(name.c_str(), "rb");
    if(f==0) {
        printf("open file %s fail!\n",name.c_str());
        exit(-1);
    }
    fseek(f, 0, SEEK_END);
    int fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  /* same as rewind(f); */
    v.resize(fsize);
    fread(&v[0], fsize, 1, f);
    fclose(f);
}

int read_file_noexit(string name,vector<char> &v){
    FILE *f = fopen(name.c_str(), "rb");
    if(f==0) {
        printf("open file %s fail!\n",name.c_str());
        return -1;
    }
    fseek(f, 0, SEEK_END);
    int fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  /* same as rewind(f); */
    v.resize(fsize);
    fread(&v[0], fsize, 1, f);
    fclose(f);
    return 0;
}

