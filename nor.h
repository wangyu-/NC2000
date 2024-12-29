#include "comm.h"

extern uint8_t* nor_banks[0x20];
extern uint8_t nor_buff[NOR_SIZE];

void SaveNor();

bool read_nor(uint16_t addr, uint8_t &value);
bool write_nor(uint16_t addr,uint8_t value);

void init_nor();
