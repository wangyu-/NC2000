#pragma once

extern unsigned int & speed_scaledown;

int io_v2_read(int address);
void io_v2_write(int address, int value);

bool setTimerA();
void setIrqTimeBase();
bool nmiEnable();
bool timeBaseEnable();

void nc2k_state_warm_reset();
void nc2k_state_cold_reset();
