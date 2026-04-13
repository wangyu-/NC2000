#include "comm.h"

extern uint8_t &RCR0;
extern uint8_t &RCR1;

extern int uart_log_level;
extern bool uart_advance;

const int RCR0_ALARM= 0x02;
const int RCR0_2HZ=   0x01;

const int RCR1_SAMPLE= 0x04;
const int RCR1_ALARM= 0x02;
const int RCR1_2HZ=   0x01;

const int IV_2HZ = 0x00;
const int IV_SAMPLE= 0x01;
const int IV_ALARM= 0x02;
const int IV_NONE= 0x1f;
void clear_iv();
void put_iv(uint8_t value);
void del_iv(uint8_t value);
uint8_t peek_iv();
uint8_t get_iv();

uint8_t read_rcr0();
void write_rcr0(uint8_t value);

inline uint32_t get_sample_hz(){
    uint32_t high=RCR0>>4;
    if(high==0||high==0xf) return 0;
    return 1<<(high-1);
}

uint8_t read_rcr1();
void write_rcr1(uint8_t value);

uint8_t read_3a();
void write_3a(uint8_t value);

uint8_t read_3b();
void write_3b(uint8_t value);

uint8_t read_3c();
void write_3c(uint8_t value);

uint8_t read_3d();
void write_3d(uint8_t value);

void open_serial_port(char *port_name);
