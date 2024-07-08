#pragma once

#include "comm.h"

/*
===================
emulator api
===================
*/
void Initialize();
void Reset();
void SetKey(uint8_t, bool);
void RunTimeSlice(uint32_t, bool);
bool CopyLcdBuffer(uint8_t*);
void LoadNC1020();
void SaveNC1020();
