#include "comm.h"
#include "cpu.h"
#include "NekoDriverIO.h"
#include "nand.h"
#include "state.h"
#include "ram.h"
#include "dsp/dsp.h"
#include "mem.h"
#include "io.h"
#include <cassert>
#include "CC800IOName.h"
#include "iv_uart.h"
#include "io_new.h"

extern nc2k_states_t nc2k_states;
extern Dsp dsp;

static int &dspRetData=nc2k_states.dspRetData;
static bool &dspTrans=nc2k_states.dspTrans;
static bool &dspSleep=nc2k_states.dspSleep;

static int &tmaValue=nc2k_states.tmaValue;
static int &tmaReload=nc2k_states.tmaReload;

const int IO_TIMERA_VAL_L = 0x10;
const int IO_TIMERA_VAL_H = 0x11;
const int IO_TIMERAB_CTRL = 0x14;

const int INT_TIME_BASE = 8;

unsigned char &inner_interrupt_control=nc2k_states.inner_interrupt_control;

unsigned int &speed_scaledown=nc2k_states.speed_scaledown;

static uint8_t * rtc_reg=nc2k_states.ext_reg;
static uint8_t * ext_reg=nc2k_states.ext_reg;

static unsigned char* ioReg=nc2k_states.ram_io;

uint8_t &lcdon=nc2k_states.lcdon;

static int &patch_idx=nc2k_states.patch_idx;
static unsigned char *patch_table=nc2k_states.patch_table;

/////d0应该是未定指令
////不处理这个有声读物会死机
bool &dsp_0xd0=nc2k_states.dsp_0xd0;

int &dsp_0x7001_0x7002=nc2k_states.dsp_0x7001_0x7002;

//bool dsp_0x91_volume_adjust=false;

bool &dsp_data_feeded_but_hasnt_fetched=nc2k_states.dsp_data_feeded_but_hasnt_fetched;
unsigned char &dsp_data_low=nc2k_states.dsp_data_low;

// dsp functions adapted from pc1000emux
int dsp31read_RetData() {
    //dsp_0x7001_0x7002=0;   //looks like it doesn't matter whether reset on read

    //dsp_0xd0=0; //if this is reset, then 有声读物 for nc2000 will stuck
    //dspTrans shound't be reset 

    if(dspRetData==-1){ // indicates no data to return
        return 0xff;
    }
    int ret=dspRetData;
    dspRetData=-1;
    
	/*if (dspRetData==0x5a)  //old code, looks like no need to do special handling
    {
        //printf("!!!0x5a!!!!\n");
        dspRetData=0xff;
    }else{
        dspRetData=0x00;
    }*/

	return ret;
}


int dsp30read_Stat() {
    const int DSP_SLEEP_FLAG = 0x80;
    const int DSP_RETURN_DATA_READY_FLAG = 0x40;
    const int DSP_DATA_HASNT_FETCHED_FLAG = 0x30;

    int value = 0;

    if (dspSleep)
        value |= DSP_SLEEP_FLAG;

    bool sound_busy(void);
    if(!dspSleep && sound_busy()){
        if(dsp_data_feeded_but_hasnt_fetched){
            value |= DSP_DATA_HASNT_FETCHED_FLAG;
        }
    }else{
        // simulate data has been consumed by dsp
        // maybe better to do in a more real-time fashion, but seems like doing here is sufficient
        dsp_data_feeded_but_hasnt_fetched = false; 
    }

    if(dsp_0xd0){ // looks like as long as dsp_0xd0 is set, it should always consider as ready. other wise 有声读物 nc2000 will stuck
        value |= DSP_RETURN_DATA_READY_FLAG;
    }

    if(dsp_0x7001_0x7002){
        //if(dspRetData!=-1){ //if use the if, it also works.  to keep consistent with dsp_0xd0, don't use if for now
            value |= DSP_RETURN_DATA_READY_FLAG;
        //}
    }

    if(dspTrans){
        if(dspRetData!=-1){  // if omit this if, it also seems to work for all existing codes
	        value |= DSP_RETURN_DATA_READY_FLAG;
        }
    }
    /* 
    if(dsp_0x91_volume_adjust==true){ //a hack that doesn't need any more
        value|=DSP_SLEEP_FLAG; //??why
        dsp_0x91_volume_adjust=false;
    }*/
    if(debug_level>=3) {
        printf("dspStat() return %02x\n",value);
    }
    return value;
}

void dspCmd(int high, int low) {
    int cmd= high * 256 + low;
    if(cmd==0xffff){
        if(dspTrans){
            if(debug_level>=1) printf("[dsp] get out of dsp trans because of 0xffff\n");
            dspTrans=false;
        }
    }

    if(cmd== 0x8000){ //SLEEP
            if(debug_level>=2)printf("[dsp] got cmd 0x8000, enable dsp sleep\n");
            dspSleep = true;
    }

    if(dspTrans){ // if in dspTrans mode, then shouldn't hanlde below commands
        return ;
    }

    if(high==0xe0){
        if(debug_level>=1) printf("[dsp] got 0xe0 cmd %04x\n", cmd);
    }

    if(high==0x91){
        if(debug_level>=1) printf("[dsp] got 0x91 cmd %04x\n", cmd);
    }
    

	if(high==0xd0 ){
        dspRetData = 0x5a;     // after 0xd001, dsp ret will return 0x5a and 0xff in turn
        if(debug_level>=1){
            printf("[DSP] got dsp cmd %02x %02x\n",high,dsp_data_low);
            if(cmd==0xd000 && debug_level>=2) printf("[DSP] got d000\n"); // this is seen in 报时 and calculator
            if(cmd!=0xd001 && cmd!=0xd000) printf("[DSP] oops, got d0 but not d001 or d000 %04x!!!!!!!!!\n",cmd);
        }
        dsp_0xd0=1;
    }else if(high >0x60){
        dsp_0xd0=0;
    }

    /*if(high==0x91){
        //if(debug_level>=1) printf("[DSP] got dsp cmd %02x %02x\n",value,dsp_data_low);
        dsp_0x91_volume_adjust=true;
    } else if(high >=0x60){
        dsp_0x91_volume_adjust=false;
    }*/

    if(high==0x70){
        if(dsp_data_low==0x01 ||dsp_data_low==0x02){
            dsp_0x7001_0x7002=1;
            dspRetData=0x06;
        }else{
            dsp_0x7001_0x7002=0;
        }
    }else if(high >=0x60) {
        dsp_0x7001_0x7002=0;
    }

    if(cmd==0x7004){
        if(debug_level>=1)printf("[dsp] got cmd 0x7004, enable dsp trans\n");
        //enable_dyn_debug_next_n=100;
        dspTrans=true;
        //dsp_0x7001_0x7002=0; //need to clear buf previous logic already covered
    } 
}

void dsp30write_reset_wake(int value) {
    const int DSP_WAKEUP_FLAG = 0x80;
    const int DSP_RESET_FLAG = 0x40;
    if (value & DSP_WAKEUP_FLAG || value & DSP_RESET_FLAG) {
        dspSleep = false;
        dsp.reset();
    }
    if (value & DSP_RESET_FLAG ) {
        dspRetData = -1;
        dsp_data_feeded_but_hasnt_fetched = false;
        
        if(dspTrans){ // if set to false, 有声读物 for nc2000 will stuck on quit
            //for old code, if reset it stucks for sure
            //but looks like for now it's fine
            if(debug_level>=1) printf("[dsp] got cmd %02x, get out of dsp trans\n",value);
            dspTrans=false;
        }

        //for dsp_0x7001_0x7002 and dsp_0xd0, whether it reset or not doesn't matter for existing code
        //looks like resetting make more sense
        dsp_0x7001_0x7002=0;
        dsp_0xd0=0;
    }
}
void dsp33write_cmd_data(int value){
    //ioReg[0x33] = value;  //shouldn't be read back
    dspCmd(value , dsp_data_low);
    if(dspTrans){
        dspRetData = dsp_data_low;
        //in dspTrans mode, the data shouldn't be pass to dsp.write()
        return ;
    }

    bool sound_busy(void);
    if(!dspSleep && sound_busy()){
        //if(dsp.dspMode==4 || ((dsp.dspMode==1 ||dsp.dspMode==2) && value <0x60))         //for existing code, whether has this code doesn't matter
        dsp_data_feeded_but_hasnt_fetched=true;
    }
    //for robustness, only pass the value that dsp.write() knows
    if(value==0xa0  ||(value&0xc0)|| dsp.dspMode==4 || ((dsp.dspMode==1 ||dsp.dspMode==2) && value <0x60)){
        dsp.write(value,dsp_data_low);
    }else{
        if(dsp.dspMode!=0){
            if(debug_level>=1) printf("[DSP] got dsp cmd %02x %02x\n",value,dsp_data_low);
        }else{
            if(debug_level>=2) printf("[DSP] got dsp cmd %02x %02x in mode 0\n",value,dsp_data_low);
        }
    }
}

//timerA from pc1000emux
// 实现的不全，缺TMACT
bool setTimerA() {
    int temp = ioReg[IO_TIMERAB_CTRL] >> 4;
    if (temp != 0) {
        tmaValue += (256 >> temp);
        if (tmaValue >= 0x10000) {
            tmaValue = tmaReload;
            if(debug_level>=1) printf("timer A interrupt triggered");
            if ((inner_interrupt_control & 1) != 0){
                ioReg[io01_int_status] |= 1;
                return true;
            }
        }
    }
    return false;
}
//todo timerB

void setIrqTimeBase() {
    ioReg[io01_int_status] |= INT_TIME_BASE;
}

bool nmiEnable() {
    return (inner_interrupt_control & 0x10) == 0;
}

bool timeBaseEnable() {
    //if(nc1020mode||nc2000mode||nc3000mode){
        //////////if((ioReg[O_INT_ENABLE] & 8)) return false;  //not correct, b3 is EXIE1, not for timebase
        /*
        // todo fix this
        if (this->field_0x96d4ac != '\0') {
            return true;
        }*/
        return (ioReg[io04_general_ctrl] & 0xf)!=0;
    //}
    //assert(false);
}

int io_v2_read(int address) {
    if(nc2000mode&&log_all_dsp_io&& address>=0x30 && address<=0x33){
        printf("""[io_v2_read] address=%02x\n",address);
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
        if(address==0x18){
            return Read18Port4(address);//not important? seems like hotlink only
        }
    }
    if(nc1020mode||nc2000mode){
        if(address== 0x1c){
            int battery_detect_level= ram_io[0x1c]&0x1f;
            //basic电源管理认为12是满电，lav电源检测认为11是满电
            if(battery_detect_level>=battery_level){
                return ram_io[0x1c]|32;
            }
            else{
                return ram_io[0x1c] &~32;
            }
        }
    }
    if(nc1020mode||nc2000mode||nc3000mode){
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
                return dsp30read_Stat();
            case 0x21:
                return dsp31read_RetData();
        }
    }
    if(nc3000mode){
        if(address==0x39) {
            return read_nand();
        }
        if(address==0x1e){
            return ReadPort6EX(address);
        }
    } 
    if(nc2000mode){
        if(address==0x29) {
            return read_nand();
        }
        switch(address){
            case 0x30:
                return dsp30read_Stat();
            case 0x31:
                return dsp31read_RetData();
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

void io_v2_write(int address, int value) {
    if(nc2000mode&&log_all_dsp_io&&address>=0x30 && address<=0x33){
        printf("[io_v2_write] address=%02x value=%02x\n",address,value);
    }
    if(address==0x1a){
        if(debug_level>=1 ){
            printf("[io_v2_write] 0x1a value=%02x\n",value);
            printf("[io_v2_write] oops!!!!!! 0x1a value&80  is true\n");
        }
        if(enable_assert) assert((value &0x80)==0);
    }
    if(address==0x3e){
        if(value>=0x10 &&value<=0x1f){
            if(enable_assert) assert(value==0x10);
            patch_idx=value;
            if(debug_level>=1) printf("[io_v2_write] 0x3e value=%02x\n",value);
            //enable_dyn_debug_next_n=100;
        }
        else{
            patch_idx=0;
        }
    }
    if(address==0x3f){
        if(patch_idx>=0x10 &&patch_idx<=0x1f){
            patch_table[patch_idx]=value;
            patch_idx++;
        }
    }
    if(nc3000mode){
        if(address==0x05){
            uint8_t cks=value>>5;
            if(debug_level>=2) {
                printf("cks set to %d\n",cks);
            }
            if (cks!=ram_io[0x05]>>5){
                //the defintion is not same as spdc1024
                switch(cks){
                    case 0: speed_scaledown=32;break;
                    case 1: speed_scaledown=4;break;
                    case 2: speed_scaledown=2;break;
                    case 3: speed_scaledown=1;break;
                    case 4: speed_scaledown=512;break;
                    case 5: speed_scaledown=256;break;
                    case 6: speed_scaledown=64;break;
                    case 7: printf("clk off\n");speed_scaledown=int_inf;break;
                    default:assert(false);
                }
                //printf("<cks=%d scaledown=%d>\n",cks,speed_scaledown);
            }
             //purposely not return
        }
        if(address==0x39) {
            return nand_write(value);
        } 
    }
    if(nc2000mode||nc1020mode||pc1000mode) {
        if(address==0x05){
            
            uint8_t cks=value>>5;
            uint8_t cps=value&0x07;
            lcdon=(value>>3)&1;
            if(debug_level>=3) printf("Write05ClockCtrl %02x cks=%d cps=%d lcdon=%d\n",value,cks,cps,lcdon);
            if (cks!=ram_io[0x05]>>5){
                switch(cks){
                    case 0: speed_scaledown=8;break;
                    case 1: speed_scaledown=4;break;
                    case 2: speed_scaledown=2;break;
                    case 3: speed_scaledown=1;break;
                    case 4: speed_scaledown=64;break;
                    case 5: speed_scaledown=32;break;
                    case 6: speed_scaledown=16;break;
                    case 7: printf("clk off\n");speed_scaledown=int_inf;break; 
                    default:assert(false);
                }
                /*if(nc1020mode && cks==7){ //if accidentally closed during get, at least save what has already been got
                    extern deque<char> queue_for_write;
                    bool dummy_io_for_write(uint16_t addr, uint8_t value);
                    if(queue_for_write.size()){
                        dummy_io_for_write(0x3fff,  0);
                    }
                }*/
                if(enable_debug_cks) printf("<cks=%d scaledown=%d>\n",cks,speed_scaledown);
            }
            //purposely not return
        }
        if(address==0x29) {
            return nand_write(value);
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
                dsp30write_reset_wake(value);
                return;
            case 0x33:
                dsp33write_cmd_data(value);
                return;
            case 0x32:
                dsp_data_low=value;
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
                dsp30write_reset_wake(value);
                return;
            case 0x23://0x23
                dsp33write_cmd_data(value);
                return;
            case 0x22://0x22
                dsp_data_low=value;
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
            return Write05ClockCtrl(address, value);
        }
        if(address==0x06){
            return Write06LCDStartAddr(address, value);
        }
        if(address==0x07){
            if(debug_level>=2)printf("write07PortConfig %02x xt=%02x\n",value,value&0x7);
            return Write07PortConfig(address,value);//not important? seems like only hotlink inside
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
            ioReg[0x0d] = value;
            super_switch();
            return;
        }
        if(address==0x0f){
            return Write0F(address,value);
        }
        if(address==0x15){
            return Write15Dir1(address, value);
        }
        if(address==0x18){
            return Write18Port4(address, value);
        }
        if(address==0x19){
            return Write19CkvSelect(address, value);//not important? seems like only hotlink inside
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
            if(index<7){
                //drop invalid value
                if((signed char)rtc_reg[0x0b]<0) return;
            }            
            if(index==0x0a) return write_rcr0(value);
            if(index==0x0b) return write_rcr1(value);
            rtc_reg[index]=value;
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
        case IO_TIMERA_VAL_L://0x10
            tmaReload = (tmaReload & 0xff00) | value;
            return;
        case IO_TIMERA_VAL_H://0x11
            tmaReload = (tmaReload & 0xff) | (value << 8);
            tmaValue = tmaReload;
            return;
        default:
            ioReg[address] = value;
    }
}
