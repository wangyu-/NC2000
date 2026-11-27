#pragma once
#include "comm.h"

void init_audio();
void shutdown_audio();

void sound_push_dsp_samples(const int16_t* samples, int count);

void beeper_on_io_write(int );
void post_cpu_run_sound_handling();
