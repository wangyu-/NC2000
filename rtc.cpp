#include "comm.h"
#include "state.h"
#include "compare/pc1000bus.h"
#include "mem.h"
#include "rtc.h"

static uint8_t * rtc_reg=nc2k_states.ext_reg;

static bool& time_adjusted=nc2k_states.time_adjusted;

void setTime1000emux() {
    extern BusPC1000 *bus_pc1000;

	const int ADDR_POWER_UP_FLAG = 0x435;
	const int ADDR_WATCH_DOG = 0x468;
	const int ADDR_IDLESEC = 0x471;
    const int ADDR_HOUR = 0x469;
    const int ADDR_YEAR = 0x46c;

    bus_pc1000->write(ADDR_IDLESEC, 0); //设置idlesec为0，禁止自动关机
    int year = bus_pc1000->read(ADDR_YEAR) + 1881;
    if (year == 2000) {
		/*SYSTEMTIME sys;
		GetLocalTime(&sys);
        bus->write(ADDR_YEAR, sys.wYear - 1881);
        bus->write(ADDR_YEAR + 1, sys.wMonth - 1);
        bus->write(ADDR_YEAR + 2, sys.wDay - 1);
        bus->write(ADDR_HOUR, sys.wHour);
        bus->write(ADDR_HOUR + 1, sys.wMinute);
        bus->write(ADDR_HOUR + 2, sys.wSecond * 2);*/
    }
}

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
	rtc_reg[TR_s]++;
	if (rtc_reg[TR_s] == 60) {
      rtc_reg[TR_s] = 0;
      rtc_reg[TR_m] = rtc_reg[TR_m] + 1;
      if (rtc_reg[TR_m] == 60) {
        rtc_reg[TR_m] = 0;
        rtc_reg[TR_h] = rtc_reg[TR_h] + 1;
        if (rtc_reg[TR_h] == 24) {
          rtc_reg[TR_h] = 0;
          rtc_reg[TR_d] = rtc_reg[TR_d] + 1;
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
		rtc_reg[2]=local_time->tm_hour;
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
	rtc_reg[2]=local_time->tm_hour;
	rtc_reg[1]=local_time->tm_min;
	rtc_reg[0]=local_time->tm_sec;
}

void sync_time_cold_start(){
	if(nc2000mode){
		bool is_nc2600_rom(void);
		if(is_nc2600_rom()){
			if(!time_adjusted && Peek16(0x3fa)==0x7a &&rtc_reg[0]==1){
				time_adjusted=1;
				if(enable_auto_time_sync) sync_time_2000();
			}
		}else{
			if(!time_adjusted && Peek16(0x3fa)==0x79 &&rtc_reg[0]==1){
				time_adjusted=1;
				if(enable_auto_time_sync) sync_time_2000();
			}
		}
	}
	if(nc3000mode){
		if(!time_adjusted && Peek16(0x3fa)==0x7b &&rtc_reg[0]==1){
			time_adjusted=1;
			if(enable_auto_time_sync) sync_time_2000();
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
				if(enable_auto_time_sync) {
					sync_time_1020();
				}
			}
			else if(!time_adjusted_phase2 &&time_adjusted && cycles - time_adjusted_cycle > CYCLES_SECOND/100){
				time_adjusted_phase2=1;
				if(enable_auto_time_sync) {
					sync_time_1020();
				}
			}
		}
	}
}
