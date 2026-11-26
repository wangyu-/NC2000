#pragma once
#include <cstdint>

const uint16_t IO_LIMIT = 0x40;

int io_v2_read(int address);
void io_v2_write(int address, int value);

void setIrqTimeBase();
bool nmiEnable();
bool timeBaseEnable();

void nc2k_warm_reset();
void nc2k_cold_reset();

int cks_to_speed_scaledown(int cks);
