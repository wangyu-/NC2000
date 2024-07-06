#pragma once 
#include "comm.h"

extern uint8_t* ram_buff;
extern uint8_t* stack;
extern uint8_t* ram_io;
extern uint8_t* ram_40;
extern uint8_t* ram00;
extern uint8_t* ram02;
extern uint8_t* ram_b;
extern uint8_t* ram04;

extern uint8_t* memmap[8];
extern uint8_t* bbs_pages[0x10];

void init_mem();

uint8_t & Peek16(uint16_t addr);
uint16_t PeekW(uint16_t addr);
uint8_t Load(uint16_t addr);
void Store(uint16_t addr, uint8_t value);

void super_switch();
