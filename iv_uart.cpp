#include "comm.h"
#include <cassert>
#include <cstdint>
#include <set>
#include "iv_uart.h"
#include "state.h"
#include <libserialport.h>
#include <sys/types.h>
using namespace std;

int uart_log_level=1;

extern nc2k_states_t nc2k_states;
static uint8_t * ram_io=nc2k_states.ram_io;
static uint8_t * ext_reg=nc2k_states.ext_reg;

static uint8_t bk=0;

uint8_t &RCR0=ext_reg[0x0a];
uint8_t &RCR1=ext_reg[0x0b];

set<uint8_t> iv_set;
void iv_uart_reset(){
    iv_set.clear();
    RCR0=0;
    RCR1=0;
}
void put_iv(uint8_t value){
    assert(value!=IV_NONE);
    iv_set.insert(value);
}
void del_iv(uint8_t value){
    assert(value!=IV_NONE);
    iv_set.erase(value);
}
uint8_t peek_iv(){
    if(iv_set.empty()) return IV_NONE;
    return *iv_set.begin();
}
uint8_t get_iv(){
    if(iv_set.empty()) return IV_NONE;
    uint8_t value=*iv_set.begin();
    //iv_set.erase(iv_set.begin()); //interrupt vectors are not self clear?
    return value;
}

uint8_t read_rcr0(){
    return RCR0;
}
void write_rcr0(uint8_t value){
    if(debug_level>=1){
        printf("write_rcr0 %02x\n",value);
    }
    RCR0=value;
}

uint8_t read_rcr1(){
    return RCR1;
}
void write_rcr1(uint8_t value){
    if(value & RCR1_ALARM) del_iv(IV_ALARM);
    if(value & RCR1_2HZ) del_iv(IV_2HZ);
    if(value & RCR1_SAMPLE) del_iv(IV_SAMPLE);
    //the 3 low bits are self clear
    RCR1= value&0xf8;
}

/*
====================
uart host dev handle
====================
*/

int check(enum sp_return result)
{
    /* For this example we'll just exit on any error by calling abort(). */
    char *error_message;

    switch (result) {
    case SP_ERR_ARG:
        printf("Error: Invalid argument.\n");
        abort();
    case SP_ERR_FAIL:
        error_message = sp_last_error_message();
        printf("Error: Failed: %s\n", error_message);
        sp_free_error_message(error_message);
        abort();
    case SP_ERR_SUPP:
        printf("Error: Not supported.\n");
        abort();
    case SP_ERR_MEM:
        printf("Error: Couldn't allocate memory.\n");
        abort();
    case SP_OK:
    default:
        return result;
    }
}
struct sp_port *uart_port;
void open_serial_port(char *port_name){
    printf("Looking for port %s.\n", port_name);
    check(sp_get_port_by_name(port_name, &uart_port));

    printf("Opening port.\n");
    sp_open(uart_port, SP_MODE_READ_WRITE);

    int my_baudrate = 115200;
    //my_baudrate=230400;
    printf("Setting port to %d 8N1, no flow control.\n", my_baudrate);
    check(sp_set_baudrate(uart_port, my_baudrate));
    check(sp_set_bits(uart_port, 8));
    check(sp_set_parity(uart_port, SP_PARITY_NONE));
    check(sp_set_stopbits(uart_port, 1));
    check(sp_set_flowcontrol(uart_port, SP_FLOWCONTROL_NONE));
}
int is_write_ready(/*struct sp_port *port*/) {
    int waiting = sp_output_waiting(uart_port);
    if (waiting < 0) {
        assert(false);
        return waiting; // Return error code
    }
    return (waiting == 0); 
}

int is_read_ready(/*struct sp_port *port*/) {
    int bytes_waiting = sp_input_waiting(uart_port);
    if (bytes_waiting < 0) {
        assert(false);
        return -1;
    }
    return bytes_waiting>0;
}
int write_one_byte(/*struct sp_port *port*/ uint8_t byte) {
    // Timeout in milliseconds (e.g., 100ms)
    unsigned int timeout_ms = 1;

    printf("write one byte %02x\n",byte);

    if(!is_write_ready()){
        printf("uart  write but not ready\n");
        return -100;
    }
    
    // Pass the address of 'byte' (&byte) and size 1
    int result = sp_blocking_write(uart_port, &byte, 1, timeout_ms);
    assert(result==1);

    if (result == 1) {
        return 1; // Success
    } else if (result == 0) {
        return 0; // Timeout (port was busy/blocked)
    } else {
        return -1; // Error (e.g., disconnected)
    }
}

uint8_t read_one_byte() {
    if(!is_read_ready()){
        printf("uart read but not ready\n");
        return 0xff;
    }
    unsigned int timeout_ms=1;
    char buf[10];
    // We pass the pointer 'byte_out' and ask for 1 byte.
    int result = sp_blocking_read(uart_port, buf, 1, timeout_ms);
    assert(result==1);
    printf("read one byte %02x\n",buf[0]);
    return buf[0];

    if (result == 1) {
        return 1; // Successfully read 1 byte
    } else if (result == 0) {
        return 0; // Timeout: No data arrived in time
    } else {
        return -1; // Error (e.g., port disconnected)
    }
}

void clear_read_buffer(){
    while(is_read_ready()){
        read_one_byte();
    }
}
/*
====================
uart wqx io handle
====================
*/

uint8_t RHR,THR;
uint8_t BSR;
uint8_t CSTOP;
uint8_t GPC;

uint8_t read_3a_inner(){
    if(bk==0){
        return read_one_byte();
        //TODO receive data
    } else if(bk==1){
        return BSR;
    } else if(bk==2){
        return CSTOP;
    } else if(bk==3){
        return GPC;
    }
    assert(false);
}
uint8_t read_3a(){
    uint8_t ret= read_3a_inner();
    if(uart_log_level>=2){
        printf("read_3a(), returned %02x\n",ret);
    }
    return ret;
}
void write_3a(uint8_t value){
    if(uart_log_level>=2){
        printf("write_3a(), value %02x\n",value);
    }
    if(bk==0){
        write_one_byte(value);
        //TODO send data
    }else if(bk==1){
        BSR=value;
        BSR&=0xcf;// 4 5 are zero when read back
        if(uart_log_level>=1){
            printf("write BSR=%02x baudrate=%x\n",value,value &7);
        }
    }else if(bk==2){
        CSTOP=value;
    } else if(bk==3){
        GPC=value;
        //extern uint8_t P05;
        //P05&=~GPC;
    }else assert(false);
}

uint8_t LSR,LCR;
uint8_t IRCR;
uint8_t CSTART;
uint8_t RESERVED;

uint8_t read_3b_inner(){
    if(bk==0){//LSR
        //handle shift and transmit register emtpy
        //handle rxRDY

        //lda LSReg
        //and #10011110b
        //bne wait_empty_err

        /*
0000   A5 3D      L0000     LDA $3D
0002   29 FC                AND #$FC
0004   85 3D                STA $3D
0006   A5 3B                LDA $3B
0008   29 07                AND #$07
000A   D0 09                BNE L0015
000C   A5 3B                LDA $3B
000E   29 CB                AND #$CB
0010   C9 CB                CMP #$CB
0012   D0 EC                BNE L0000
0014   60                   RTS
0015   A9 01      L0015     LDA #$01
0017   60                   RTS
                            .END
        */
        uint8_t ret=0;
        if(is_write_ready()) ret|=0x60;
        if(is_read_ready()) ret|=0x01;
        return ret;
    }else if(bk==1){
        return IRCR;
    }else if(bk==2){
        return CSTART;
    }else if(bk==3){
        return RESERVED;
    }
    assert(false);
}
uint8_t read_3b(){
    uint8_t ret= read_3b_inner();
    if(uart_log_level>=2){
        printf("read_3b(), returned %02x\n",ret);
    }
    return ret;
}
void write_3b(uint8_t value){
    if(uart_log_level>=2){
        printf("write_3b(), value %02x\n",value);
    }
    if(bk==0){
        LCR=value;
    }else if(bk==1){
        //irda control register
        IRCR=value;
    }else if(bk==2){
        CSTART=value;
    }else if(bk==3){
        RESERVED=value;
    }else{
        assert(false);
    }
    //ram_io[0x3b]=value;
}


uint8_t MCR;
uint8_t MSR;
uint8_t TMR;
uint8_t P05;

uint8_t read_3c_inner(){
    if(bk==0){
        if(MCR &0x10){
            //lda     #00000010b
            //sta     MCReg           ;clear

            // handle "clear"?

            // handle fifo reach trigger level
        }
        MCR&=0xef;
        if(is_read_ready()){
            MCR|=0x10;
        }
        return MCR;
    } else if(bk==1){
        return MSR;
    } else if(bk==2){
        return TMR;
    } else if(bk==3){
        //output value are read back
        //how to handle input value?
        return P05;
    }
    assert(false);
}
uint8_t read_3c(){
    uint8_t ret= read_3c_inner();
    if(uart_log_level>=2){
        printf("read_3c(), returned %02x\n",ret);
    }
    return ret;
}
void write_3c(uint8_t value){
    if(uart_log_level>=2){
        printf("write_3c(), value %02x\n",value);
    }
    if(bk==0){
        MCR=value;
    } else if(bk==1){
        MSR=value;
    } else if(bk==2){
        TMR=value;
        if(value &0x20){
            //UART enable
        }else{
            printf("uart disable\n");
            clear_read_buffer();
            //UART disable
        }
    } else if(bk==3){
        //save output value for read back
        P05=value&GPC;
    }
    else assert(false);
}

uint8_t IVR; //only for UCE bit
uint8_t FCR;
uint8_t IER;
//uint8_t BK_ONLY;
uint8_t read_3d_inner(){
    if(bk==0) {
        return get_iv()<<3|(IVR&0x04)|bk;
    }
    else if(bk==1){
        return FCR|bk;
    }
    else if(bk==2){
        return IER|bk;
    }
    else if(bk==3){
        return bk;
    }
    assert(false);
}
void write_3d(uint8_t value){
    if(uart_log_level>=2){
        printf("write_3d(), value %02x\n",value);
    }
    uint8_t new_bk=value &3;
    value&=0xfc;
    if(bk==0){
        //UCE: enable UART clock
        IVR=value &0x4;
        if(value&0x4){
            //TODO enable uart clock
        }else{
            //TODO disable uart clock
        }

        //TODO how to handle IV write?
    }
    else if(bk==1){
        if(value & 0x10){
            if(uart_log_level>=1) printf("RFRST\n");
            //TODO RFRST
        }
        if(value & 0x20){
            if(uart_log_level>=1) printf("TFRST\n");
            //TODO TFRST
        }
        if(value & 0x80){
            if(uart_log_level>=1) printf("BKRT\n");
        }
        if(value & 0xc0){
            if(uart_log_level>=1) printf("FIFO trigger not zero !!! value= %x\n",value>>6);
        }
        // 3 is zero when read back
        // 45 should be zero when read back as well??
        FCR=value&0xc8;
    }
    else if(bk==2){
        if(value!=0){
            if(uart_log_level>=1) printf("IER not zero!!! value= %2x\n",value);
        }
        IER=value;
    }
    else if(bk==3){
        //no action
    }
    else{
        assert(false);
    }
    bk=new_bk;
}

uint8_t read_3d(){
    uint8_t ret= read_3d_inner();
    if(uart_log_level>=2){
        printf("read_3d(), returned %02x\n",ret);
    }
    return ret;
}

