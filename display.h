#pragma once
#include "comm.h"

extern uint8_t lcd_buf[SCREEN_WIDTH * SCREEN_HEIGHT / 8*2];
extern unsigned char *lcd_effect_buffer;

void Render(u64_t);
void init_lcd_stripe();
