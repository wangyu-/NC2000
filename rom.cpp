#include "comm.h"

extern WqxRom nc2k_rom;

uint8_t rom_buff[24*1024*1024];

uint8_t* rom_volume0[0x100];
uint8_t* rom_volume1[0x100];
uint8_t* rom_volume2[0x100];

void LoadRom(const string romPath){
	assert(sizeof(rom_buff) >= ROM_SIZE);

	int rom_size=-1;

	/*if(nc1020mode){
		rom_size=ROM_SIZE;
	}*/
	if(pc1000mode|| nc1020mode) {
		//for pc1000, becausing of remapping, the file size is not equal to sizeof(rom_buff)
		rom_size=0x8000*128*3;
	}

	uint8_t* temp_buff = (uint8_t*)malloc(rom_size);
	FILE* file = fopen(romPath.c_str(), "rb");
	if(file==0) {
        printf("file %s not exist!\n",romPath.c_str());
        exit(-1);
    }
	fseek(file, 0, SEEK_END);
	int fsize = ftell(file);
	fseek(file, 0, SEEK_SET);

	if(fsize!=rom_size){
		printf("rom size wrong, <expected=%d, actual=%d>; probably your rom is in an old format, try to download a newer one\n",rom_size,fsize);
		exit(-1);
	}

	fread(temp_buff, 1, rom_size, file);
	ProcessBinaryLinear(rom_buff, temp_buff, rom_size);
	free(temp_buff);
	fclose(file);
}

void hack1_save_nc1020_12m_rom(){
	FILE* file = fopen("tw1020/hack.rom", "wb");
	for(int j=0;j<0x80;j++){
		fwrite(rom_volume0[128+j], 0x8000, 1, file);
	}
		for(int j=0;j<0x80;j++){
		fwrite(rom_volume1[128+j], 0x8000, 1, file);
	}
		for(int j=0;j<0x80;j++){
		fwrite(rom_volume2[128+j], 0x8000, 1, file);
	}
	fclose(file);
}

void init_rom(){
    memset(&rom_buff,0xff,ROM_SIZE);
	LoadRom(nc2k_rom.romPath);
	if(nc1020mode){
		if(true){
			for (int i = 0; i < 128; i++) {
				//rom_volume0[i  ]=
				rom_volume0[i + 128 ] = rom_buff + (0x8000 * i);

				rom_volume1[i + 128 ] = rom_buff + (0x8000 * (128 + i));
				//rom_volume1[i  ]=rom_volume0[i  ];
				rom_volume2[i + 128 ] = rom_buff + (0x8000 * (256 + i));
				//rom_volume2[i  ]=rom_volume0[i  ];
			}
			//memcpy(rom_volume0[0x90]+0x4000, nor_buff +491520,0x1000);
		}
		else{//old code for 24m rom from ggv sim
			for (uint32_t i=128; i<256; i++) {
				rom_volume0[i] = rom_buff + (0x8000 * i);
				rom_volume1[i] = rom_buff + (0x8000 * (0x100 + i));
				rom_volume2[i] = rom_buff + (0x8000 * (0x200 + i));
			}
		}
	}


	if(pc1000mode){
		for (int i = 0; i < 128; i++) {
        	// 0~128 
        	rom_volume0[i] = (unsigned char*)rom_buff + i * 0x8000;
        	rom_volume1[i] = (unsigned char*)rom_buff + i * 0x8000;

        	rom_volume0[i + 128] = (unsigned char*)rom_buff + (i + 128) * 0x8000;
        	rom_volume1[i + 128] = (unsigned char*)rom_buff + (i + 256) * 0x8000;
    	}
	}

}
