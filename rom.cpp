#include "comm.h"

extern WqxRom nc1020_rom;

uint8_t rom_buff[ROM_SIZE];

uint8_t* rom_volume0[0x100];
uint8_t* rom_volume1[0x100];
uint8_t* rom_volume2[0x100];

void LoadRom(const string romPath){
	uint8_t* temp_buff = (uint8_t*)malloc(ROM_SIZE);
	FILE* file = fopen(romPath.c_str(), "rb");
	fread(temp_buff, 1, ROM_SIZE, file);
	ProcessBinaryGGVSIM(rom_buff, temp_buff, ROM_SIZE);
	free(temp_buff);
	fclose(file);
}

void init_rom(){
    memset(&rom_buff,0,sizeof(rom_buff));
    for (uint32_t i=0; i<num_rom_pages/3; i++) {
		rom_volume0[i] = rom_buff + (0x8000 * i);
		rom_volume1[i] = rom_buff + (0x8000 * (0x100 + i));
		rom_volume2[i] = rom_buff + (0x8000 * (0x200 + i));
	}
	if(nc1020){
		LoadRom(nc1020_rom.romPath);
	}
}
