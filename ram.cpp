#include "ram.h"
#include "state.h"

extern nc1020_states_t nc1020_states;

uint8_t* ram_buff = nc1020_states.ram;
uint8_t* stack = ram_buff + 0x100;
uint8_t* ram_io = nc1020_states.ram_io;
uint8_t* ram_40 = ram_buff + 0x40;
uint8_t* ram00 = ram_buff;
uint8_t* ram02 = ram_buff + 0x2000;
uint8_t* ram04 = ram_buff + 0x4000;
uint8_t* ram06 = ram_buff + 0x6000;
uint8_t* ram08 = ram_buff + 0x8000;
uint8_t* ram0a = ram_buff + 0x8000+ 0x2000;
uint8_t* ram0c = ram_buff + 0x8000+ 0x4000;
uint8_t* ram0e = ram_buff + 0x8000+ 0x6000;

uint8_t* ram_b = nc1020_states.ram_b;
