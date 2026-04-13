#pragma once
#include "comm.h"

extern uint8_t* nor_banks[0x20];
extern uint8_t nor_buff[1024*1024];;

extern uint8_t nor_info_block[0x100];

void SaveNor(string file = "");
bool read_nor(uint16_t addr, uint8_t &value);
bool write_nor(uint16_t addr,uint8_t value);

void init_nor();
