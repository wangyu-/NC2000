#include "comm.h"

extern char nand[65536+64][528];

uint8_t read_nand();
void nand_write(uint8_t);

void read_nand_file();

void write_nand_file();
