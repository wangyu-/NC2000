#include "comm.h"

extern uint8_t rom_buff[ROM_SIZE];

extern uint8_t* rom_volume0[0x100];
extern uint8_t* rom_volume1[0x100];
extern uint8_t* rom_volume2[0x100];

void LoadRom();
void init_rom();
