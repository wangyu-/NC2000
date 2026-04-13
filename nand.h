#include "comm.h"

extern string nand_magic;

uint8_t read_nand();
void nand_write(uint8_t);

void read_nand0_file();
void read_nand_file();

void write_nand0_file(string file="");
void write_nand_file(string file="");
