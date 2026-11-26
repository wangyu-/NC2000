#pragma once

const int TR_s=0x0;
const int TR_m=0x01;
const int TR_h=0x02;
const int TR_d=0x03;
const int TR_ms=0x04;
const int AR_s=0x05;
const int AR_m=0x06;
const int AR_h=0x07;
const int RTC_CTRL=0x0a;
const int INT_CLEAR=0x0b;

void bumpRTC();
bool chk_ar();

void sync_time_1020();
void sync_time_2000();

void sync_time_if_factory_value();
