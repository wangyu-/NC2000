#include "comm.h"
#include "cpu.h"
#include "NekoDriverIO.h"
#include "nand.h"
#include "state.h"
#include "ram.h"
#include "dsp_new/dsp_cmd.h"
#include "mem.h"
#include <cassert>
#include "CC800IOName.h"
#include "iv_uart.h"
#include "io_new.h"
#include "rtc.h"
#include "timer_ab.h"

extern nc2k_states_t nc2k_states;

const int INT_TIME_BASE = 8;

unsigned char &inner_interrupt_control=nc2k_states.inner_interrupt_control;

static uint8_t * rtc_reg=nc2k_states.ext_reg;
static uint8_t * ext_reg=nc2k_states.ext_reg;

static unsigned char* ioReg=nc2k_states.ram_io;

uint8_t &lcdon=nc2k_states.lcdon;

void setIrqTimeBase() {
    ioReg[io01_int_status] |= INT_TIME_BASE;
}

bool nmiEnable() {
    return (inner_interrupt_control & 0x10) == 0;
}

bool timeBaseEnable() {
    return (ioReg[io04_general_ctrl] & 0xf)!=0;
}

int io_v2_read(int address) {
    if(nc2000mode&&log_all_dsp_io&& address>=0x30 && address<=0x33){
        printf("[io_v2_read] address=%02x\n",address);
    }
    if(nc1020mode||nc2000mode||nc3000mode||pc1000mode){
        if(address==0x04) return Read04StopTimer0(address);
        if(address==0x05) return Read05StartTimer0(address);
        if(address==0x06) return Read06StopTimer1(address);
        if(address==0x07) return Read07StartTimer1(address);

        if(address==0x08){
            if(cpu->PC>=0x44c2 &&cpu->PC<=0x44c4) {
                //printf("<<pc=%04x>>\n",cpu->PC);
                //enable_key_debug_once=1;
                //return 0x01;
            }
            return ReadPort0(address);
        }
        if(address==0x09){
            return ReadPort1(address);
        }
        if(address==0x10) return timer_ab_read_10_tmal();
        if(address==0x11) return timer_ab_read_11_tmah();
        if(address==0x12) return timer_ab_read_12_tmbl();
        if(address==0x13) return timer_ab_read_13_tmbh();
        if(address==0x18){
            uint8_t val = Read18Port4(address);
            if (nc2000mode && !nand_is_ready()) {
                val &= ~0x20;
            }
            return val;
        }
    }
    if(nc1020mode||nc2000mode){
        if(address== 0x1c){
            int battery_detect_level= ram_io[0x1c]&0x1f;
            //basic电源管理认为12是满电，lav电源检测认为11是满电
            if(battery_detect_level>=battery_level){
                return ram_io[0x1c]|0x20;
            }
            else{
                return ram_io[0x1c] &~0x20;
            }
        }
    }
    if(nc1020mode||nc2000mode||nc3000mode){
        if(address==0x0c){
            //pc1000's 0x0c has different definition, if used it won't boot
            return Read0CGeneralStatus(address);
        }
        if(address==0x3a) return read_3a();
        if(address==0x3b) return read_3b();
        if(address==0x3c) return read_3c();
        if(address==0x3d) return read_3d();

        if(address==0x3f){
            uint8_t idx= ioReg[0x3e];
            if(idx==0x0a) return read_rcr0();
            if(idx==0x0b) return read_rcr1();
            return rtc_reg[idx];
        }
    }
    if(nc1020mode||pc1000mode) {
        switch(address){
            case 0x20:
                return dsp_read_status();
            case 0x21:
                return dsp_read_data();
        }
    }
    if(nc3000mode){
        if(address==0x39) {
            return read_nand();
        }
        if(address==0x1c){
            int battery_detect_level= ram_io[0x1c]&0x1f;
            uint8_t ret = (battery_detect_level>=battery_level_nc3000) ? (ram_io[0x1c]|0x20) : (ram_io[0x1c]&~0x20);
            //if without the 0x80 lava4grey has graphic glitch
            return ret|0x80;
        }
        if(address==0x1e){
            return Read1EPort6(address);
        }
    } 
    if(nc2000mode){
        if(address==0x29) {
            return read_nand();
        }
        switch(address){
            case 0x30:
                return dsp_read_status();
            case 0x31:
                return dsp_read_data();
        }
    }
    switch (address) {
        case io01_int_status://0x01
        {
            int t;
            t = ioReg[io01_int_status];
            ioReg[io01_int_status] &= 0xc0;
            return t;
        }
        default:
            return ioReg[address];
    }
}

int cks_to_speed_scaledown(int cks){
    int ret=0;
    if(nc2000mode||nc1020mode||pc1000mode){
        switch(cks){
            case 0: ret=8; break;
            case 1: ret=4; break;
            case 2: ret=2; break;
            case 3: ret=1; break;
            case 4: ret=64; break;
            case 5: ret=32; break;
            case 6: ret=16; break;
            case 7: ret=int_inf; break; 
            default:assert(false);
        }
    }else if(nc3000mode){ //the definition is not same as spdc1024
        switch(cks){
            case 0: ret=32; break;
            case 1: ret=4; break;
            case 2: ret=2; break;
            case 3: ret=1; break;
            case 4: ret=512; break;
            case 5: ret=256; break;
            case 6: ret=64; break;
            case 7: ret=int_inf; break;
            default: assert(false);
        }
    }else{
        assert(false); //unsupported mode
    }
    return ret;
}
void io_v2_write(int address, int value) {
    if(nc2000mode&&log_all_dsp_io&&address>=0x30 && address<=0x33){
        printf("[io_v2_write] address=%02x value=%02x\n",address,value);
    }
    if(address==0x1a){
        if(debug_level>=1 && (value &0x80)){
            printf("[io_v2_write] 0x1a value=%02x\n",value);
            printf("[io_v2_write] oops!!!!!! 0x1a value&80  is true\n");
        }
        if(enable_assert_for_wqx_software) assert((value &0x80)==0);
    }

    if(nc2000mode){
        if(address==0x29) {
            return nand_write(value);
        }
    }
    if(nc3000mode){
        if(address==0x39) {
            return nand_write(value);
        } 
        if(address==0x1e){
            /* port 6 drives the NC3000's other four keyboard column lines */
            return Write1EPort6(address, value);
        }
    }

    if(nc2000mode||nc3000mode){
        if(false){
            if(address==0x32) {
                printf("<w %02x>",value);
            }
            if(address==0x33){
                printf("[w %02x]\n",value);
            }
        }
        switch(address){
            case 0x30:
                dsp_write_ctrl(value);
                return;
            case 0x33:
                dsp_write_data_high(value);
                return;
            case 0x32:
                dsp_write_data_low(value);
                return;
        }
    }
    if(nc1020mode||pc1000mode){
        if(false){
            if(address==0x22) {
                printf("<w %02x>",value);
            }
            if(address==0x23){
                printf("[w %02x]\n",value);
            }
        }
        switch (address) {
            case 0x20://0x20
                dsp_write_ctrl(value);
                return;
            case 0x23://0x23
                dsp_write_data_high(value);
                return;
            case 0x22://0x22
                dsp_write_data_low(value);
                return;
        }
    }
    if(nc1020mode||nc2000mode||nc3000mode||pc1000mode){
        if(address==0x04){
            Write04GeneralCtrl(address,value);
            if(debug_level>=2) printf("Write04GeneralCtrl %02x TBC=%02x\n",value,value&0xf);
            //Write09Port1(0x09, ram_io[0x09]);//reapply after PTYPE changed??
            return;
        }
        if(address==0x05){
            uint8_t cks=value>>5;
            uint8_t cps=value&0x07;
            lcdon=(value>>3)&1;
            if(debug_level>=3) printf("Write05ClockCtrl %02x cks=%d cps=%d lcdon=%d\n",value,cks,cps,lcdon);
            if (cks!=ram_io[0x05]>>5){
                if(cks==0x7) {
                    if(nc2k_states.woke_by_interrupt==false) {
                        printf("clk off\n");
                    }else if(debug_level>=1) {
                        RATE_LIMIT_LOG(5000,1,"clk off after woke by interrupt");
                    }
                    nc2k_states.woke_by_interrupt=false;
                }
                if(enable_debug_cks) {
                    printf("<cks=%d scaledown=%d>\n",cks,cks_to_speed_scaledown(cks));
                }
            }
            //purposely do not return
        }
        if(address==0x06){
            return Write06LCDStartAddr(address, value);
        }
        if(address==0x08){
            return Write08Port0(address, value);
        }
        if(address==0x09){
            return Write09Port1(address,value);
        }
        if(address==0x0b){
            if(debug_level>=2)printf("writing %d in 0x0b bit0\n", value&0x1);
            return Write0BPort3LCDStartAddr(address,value);
        }
        if(address==0x0c){
            return Write0CTimer01Control(address,value);
        }
        if(address==0x0d){
            if(debug_level>=1){
                uint8_t lcd_seg_new = value>>3;
                uint8_t lcd_seg_old = ioReg[0x0d]>>3;
                if(lcd_seg_old != lcd_seg_new){
                    printf("LCD segment conf changed from %02x to %02x\n", lcd_seg_old, lcd_seg_new);
                }
            }
            ioReg[0x0d] = value;
            super_switch();
            return;
        }
        if(address==0x0f){
            return Write0FZPBankPort0Dir(address,value);
        }
        if(address==0x10){
            timer_ab_write_10_tmal(value);
            return;
        }
        if(address==0x11){
            timer_ab_write_11_tmah(value);
            return;
        }
        if(address==0x12){
            timer_ab_write_12_tmbl(value);
            return;
        }
        if(address==0x13){
            timer_ab_write_13_tmbh(value);
            return;
        }
        if(address==0x15){
            return Write15Dir1(address, value);
        }
        if(address==0x18){
            return Write18Port4(address, value);
        }
        /*
        if(address==0x20){
            return Write20JG(address, value);
        }
        if(address==0x23){
            return Write23(address,value);
        }*/
    }
    if(nc1020mode||nc2000mode||nc3000mode){
        if(address==0x3e||address==0x3f){
            if (patch_table_experiment) {
			    void write_0x3e_0x3f_patchtable_helper(int address,int value);
                write_0x3e_0x3f_patchtable_helper(address,value);
            }
        }

        if(address==0x3a) return write_3a(value);
        if(address==0x3b) return write_3b(value);
        if(address==0x3c) return write_3c(value);
        if(address==0x3d) return write_3d(value);

        if(address==0x3f){
            int index=ioReg[0x3e];
            if(debug_level>=1){
                if(index==0x25){
                    printf("write to 0x3f idx=0x25 %02x\n",value);
                }
                if(index==0x26){
                    printf("write to 0x3f idx=0x26 %02x\n",value);
                }
            }
            if(debug_level>=1){
                if(index==0x24){
                    printf("write to 0x3f idx=0x24 %02x\n",value);
                }
            }
            ioReg[0x3f]=value;
            if(index==0x04) return; // TR_ms is read-only
            if(index<AR_h+1){
                //0b highest bit is WP (write protection)   
                if(rtc_reg[0x0b]&0x80) return;
            }            
            if(index==0x0a) return write_rcr0(value);
            if(index==0x0b) return write_rcr1(value);
            rtc_reg[index]=value;
            if(index == 0x24) {//port0 pull high/low control
                UpdateKeypadRegisters();
            }
            if(index == 0x25) {//XM0CF: bit 7 is RAMV
                super_switch();
            }
            if(nc3000mode){
                if(index == 0x20){//P06 direction control
                    UpdateKeypadRegisters();
                }
            }
            return;
            //return Write3F(address,value);
        }
    }
    switch (address) {
        case io00_bank_switch://0x00
            ioReg[io00_bank_switch] = value;
            super_switch();
            /////////////bankSwitch();
            return;
        case io01_int_enable://0x01
            inner_interrupt_control = value;
            return;
        case io0A_bios_bsw://0x0a
            ioReg[io0A_bios_bsw] = value;
            super_switch();
            /////////////biosBankSwitch();
            /////////////bankSwitch();
            return;
        case 0x19:
            ioReg[0x19] = value;
            if (nc3000mode) super_switch();
            return;
        case 0x1a:
            ioReg[0x1a] = value;
            super_switch();
            return;
        default:
            ioReg[address] = value;
    }
}
