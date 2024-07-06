#include "comm.h"

const uint16_t IO_LIMIT = 0x40;

extern io_read_func_t io_read[0x40];
extern io_write_func_t io_write[0x40];

void super_switch();
void init_io();

uint8_t IO_API ReadXX(uint8_t addr);
uint8_t IO_API Read06(uint8_t addr);
uint8_t IO_API Read3B(uint8_t addr);
uint8_t IO_API Read3F(uint8_t addr);
void IO_API WriteXX(uint8_t addr, uint8_t value);
//bank switch
void IO_API Write00(uint8_t addr, uint8_t value);
void IO_API Write05(uint8_t addr, uint8_t value);
void IO_API Write06(uint8_t addr, uint8_t value);
void IO_API Write08(uint8_t addr, uint8_t value);
// keypad matrix.
void IO_API Write09(uint8_t addr, uint8_t value);
// roabbs
void IO_API Write0A(uint8_t addr, uint8_t value);
// switch volume
void IO_API Write0D(uint8_t addr, uint8_t value);
// zp40 switch
void IO_API Write0F(uint8_t addr, uint8_t value);
void IO_API Write20(uint8_t addr, uint8_t value);
void IO_API Write23(uint8_t addr, uint8_t value);
// clock.
void IO_API Write3F(uint8_t addr, uint8_t value);
