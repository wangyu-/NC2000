#pragma once

#include "comm.h"


extern double speed_multiplier;

void reset_cpu_states();
void cpu_run();
void cpu_run2();
void cpu_run_emux();
void cpu_init_emux();
