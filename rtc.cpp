#include "comm.h"
#include "state.h"
#include "mem.h"
#include "rtc.h"

extern nc2k_states_t nc2k_states;

static uint8_t * rtc_reg=nc2k_states.ext_reg;

static bool& time_adjusted=nc2k_states.time_adjusted;

// from nc3000lee1.1 version
bool chk_ar()
{
	bool alm=0;
	if((rtc_reg[AR_h]&0x80))
		{
		alm=1;
		if((rtc_reg[AR_h]&0x1f)!=(rtc_reg[TR_h]&0x1f))
			return	0;
		}
	if((rtc_reg[AR_m]&0x80))
		{
		alm=1;
		if((rtc_reg[AR_m]&0x3f)!=(rtc_reg[TR_m]&0x3f))
			return	0;
		}
	if((rtc_reg[AR_s]&0x80))
		{
		alm=1;
		if((rtc_reg[AR_s]&0x3f)!=(rtc_reg[TR_s]&0x3f))
			return	0;
		}
	return	alm;
}

void bumpRTC(){
	rtc_reg[TR_s] = (rtc_reg[TR_s] & 0x3f) + 1;
	if (rtc_reg[TR_s] >= 60) {
      rtc_reg[TR_s] = 0;
      rtc_reg[TR_m] = (rtc_reg[TR_m] & 0x3f) + 1;
      if (rtc_reg[TR_m] >= 60) {
        rtc_reg[TR_m] = 0;
        uint8_t hour = (rtc_reg[TR_h] & 0x1f) + 1;
        if (hour >= 24) {
          hour = 0;
          uint16_t day = (((rtc_reg[TR_h] >> 6) & 0x03) << 8) | rtc_reg[TR_d];
          day = (day + 1) & 0x3ff;
          rtc_reg[TR_d] = day & 0xff;
          rtc_reg[TR_h] = ((day >> 8) << 6) | (rtc_reg[TR_h] & 0x20) | hour;
        } else {
          rtc_reg[TR_h] = (rtc_reg[TR_h] & 0xe0) | hour;
        }
      }
    }
}

int get_adjusted_year(int year){
	if(year<1932) {
		printf("invalid year %d, use 1932 instead\n",year);
		return 1932;
	}
	if(year<2032) return year;
	printf("current year %d is too large for wqx, adjusted to %d\n",year, 2004+ (year-2004)%28);
	return 2004+ (year-2004)%28;
}

void sync_time_2000(){
		printf("sync_time() called\n");
		time_t current_time = time(NULL);
		struct tm *local_time = localtime(&current_time);
		int adjusted_year=get_adjusted_year(local_time->tm_year + 1900);
		Store(0x3fa, adjusted_year + 0x7a - 2003);
		Store(0x3fb, local_time->tm_mon);
		Store(0x3fc, local_time->tm_mday-1);
		//Store(0x3fd, local_time->tm_wday);
		rtc_reg[2]=(rtc_reg[2]&0xc0)|(local_time->tm_hour&0x1f);
		rtc_reg[1]=local_time->tm_min;
		rtc_reg[0]=local_time->tm_sec;
}

void sync_time_1020(){
	if(nc1020tw_mode) return;
	printf("sync_time() called\n");
	time_t current_time = time(NULL);
	struct tm *local_time = localtime(&current_time);
	int adjusted_year=get_adjusted_year(local_time->tm_year + 1900);
	Store(0x472, adjusted_year + 0x7a - 2003);
	Store(0x473, local_time->tm_mon);
	Store(0x474, local_time->tm_mday-1);
	//Store(0x3fd, local_time->tm_wday);
	rtc_reg[2]=(rtc_reg[2]&0xc0)|(local_time->tm_hour&0x1f);
	rtc_reg[1]=local_time->tm_min;
	rtc_reg[0]=local_time->tm_sec;
}

// sync time if the rtc value stays at the factory default
void sync_time_if_factory_value(){
	if(nc2000mode){
		bool is_nc2600_rom(void);
		if(is_nc2600_rom()){
			if(!time_adjusted && Peek16(0x3fa)==0x7a &&rtc_reg[0]==1){
				time_adjusted=1;
				sync_time_2000();
			}
		}else{
			if(!time_adjusted && Peek16(0x3fa)==0x79 &&rtc_reg[0]==1){
				time_adjusted=1;
				sync_time_2000();
			}
		}
	}
	if(nc3000mode){
		if(!time_adjusted && Peek16(0x3fa)==0x7b &&rtc_reg[0]==1){
			time_adjusted=1;
			sync_time_2000();
		}	
	}
	if(nc1020mode){
		if(!nc1020tw_mode){
			bool & time_adjusted_phase2=nc2k_states.nc1020_time_adjusted_phase2;
			u64_t & time_adjusted_cycle=nc2k_states.nc1020_time_adjusted_cycle;
            static u64_t& cycles = nc2k_states.cycles;
			if(!time_adjusted && Peek16(0x472)==0x79 /*&&rtc_reg[0]==1*/){
				time_adjusted=1;
				time_adjusted_cycle=cycles;
				sync_time_1020();
			}
			else if(!time_adjusted_phase2 &&time_adjusted && cycles - time_adjusted_cycle > CYCLES_SECOND/100){
				time_adjusted_phase2=1;
				sync_time_1020();
			}
		}
	}
}
