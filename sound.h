#pragma once
#include "comm.h"

void init_audio();

void reset_dsp();
void write_data_to_dsp(uint8_t, uint8_t);

void beeper_on_io_write(int );
void post_cpu_run_sound_handling();
