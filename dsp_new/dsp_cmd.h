#pragma once

#include <cstdint>

// SPDS104A host bus registers ($20..$23 on PC1000/NC1020, $30..$33 on NC2000/NC3000)
uint8_t dsp_read_status();
uint8_t dsp_read_data();
void dsp_write_ctrl(uint8_t value);
void dsp_write_data_low(uint8_t value);
void dsp_write_data_high(uint8_t value);

// Hardware reset (cold boot / machine reload)
void dsp_hw_reset();

// Advance DSP internal 8 kHz DAC clock by `samples` and push output to host audio sink
void dsp_tick(int samples);

// DSP internal PCM FIFO backpressure status & logging control
bool dsp_is_busy();
void dsp_set_log_level(int level);
