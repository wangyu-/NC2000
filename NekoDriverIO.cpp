//#include "NekoDriver.h"
#include "comm.h"
extern "C" {
#ifdef HANDYPSP
//#include "ANSI/w65c02.h"
#else
#include "ANSI/65C02.h"
#endif
}
#include "cpu.h"
#include "CC800IOName.h"
#include "NekoDriverIO.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "state.h"

#define qDebug(...)

extern nc2k_states_t nc2k_states;
static uint8_t * ext_reg=nc2k_states.ext_reg;

bool &timer0run = nc2k_states.timer0run;
bool &timer1run_tmie = nc2k_states.timer1run_tmie;

// WQXSIM
bool &timer0waveoutstart = nc2k_states.timer0waveoutstart;
int &prevtimer0value = nc2k_states.prevtimer0value;
unsigned short &gThreadFlags = nc2k_states.gThreadFlags;
//unsigned char* gGeneralCtrlPtr;
//unsigned short mayGenralnClockCtrlValue;

// Full MOS IO Ports?
// (I/O) io_zp_bsw
bool &rw0f_b4_DIR00 = nc2k_states.rw0f_b4_DIR00;
bool &rw0f_b5_DIR01 = nc2k_states.rw0f_b5_DIR01;
bool &rw0f_b6_DIR023 = nc2k_states.rw0f_b6_DIR023; // 02 03
bool &rw0f_b7_DIR047 = nc2k_states.rw0f_b7_DIR047; // 04 05 06 07
bool &rw0f_b3_SH = nc2k_states.rw0f_b3_SH;    // Sample & Hold for A/D
BYTE &rw0f_b02_ZB02 = nc2k_states.rw0f_b02_ZB02; // b0..b2 (RESCPUB)

// (O/P) io_general_ctrl
bool &w04_b7_EPOL = nc2k_states.w04_b7_EPOL;   // 外部中断 (P40 OR P41 OR P00) 极性
BYTE &w04_b46_PTYPE = nc2k_states.w04_b46_PTYPE; // Port1 PTYPE0~PTYPE7
BYTE &w04_b03_TBC = nc2k_states.w04_b03_TBC;   // LCD地址线, Timebase时钟

// (O/P) io_port1_dir
// 受限于PTYPE0|5
BYTE &w15_port1_DIR107 = nc2k_states.w15_port1_DIR107;// DIR10~DIR17

// (I/O) 读取时候逐位判断DIR, 确定从ID(matrix更新)还是OL直接读取
// 假设速度, 假设1016的输入比6502的执行速度快很多, 例如延迟在10ns, 则基本可以当作输出延迟+输入延迟在STA执行途中已过去.
// 假设短路, 遇到2个都是输出, 一高一低, matrix连通了他们2者, 则实际因为是导电橡胶联通的, 实际输出高的pmos+导电橡胶+nmos的Rds构成分压网络.
// 因此定出优化规则: 在改变端口方向和写入端口时候, 立刻同步刷新输入数据. 等同于我们加了缓冲. 而处理按键时候, 忽略输出对输出的传导.
// 实际流程既是: 先复制输出状态引脚, 再处理导电橡胶传导.
BYTE &w08_port0_OL = nc2k_states.w08_port0_OL;  // output latch
BYTE &r08_port0_ID = nc2k_states.r08_port0_ID;  // input data

BYTE &w09_port1_OL = nc2k_states.w09_port1_OL;
BYTE &r09_port1_ID = nc2k_states.r09_port1_ID;
static uint8_t * ram_io=nc2k_states.ram_io;
// Temp
static unsigned char *zpioregs=ram_io;

//timer01_u* rw023_timer01val = (timer01_u*)&zpioregs[io02_timer0_val];

BYTE &w0c_b67_TMODESL = nc2k_states.w0c_b67_TMODESL;    // 01一起的计数方式
BYTE &w0c_b45_TM0S = nc2k_states.w0c_b45_TM0S;       // timer0时钟周期, 在TMODE1下接入
BYTE &w0c_b23_TM1S = nc2k_states.w0c_b23_TM1S;       // timer1时钟周期, 在TMODE1下接入
BYTE &w0c_b345_TMS = nc2k_states.w0c_b345_TMS;       // 其他模式下4个bit只有3个用上

int &timer0ticks = nc2k_states.timer0ticks;
int &timer1ticks = nc2k_states.timer1ticks;

BYTE &w01_int_enable = nc2k_states.w01_int_enable;

/* NC3000: port 6 drives the other 4 keyboard column lines */
BYTE &w1e_port6_OL = nc2k_states.w1e_port6_OL;  //added for nc3000
BYTE &r1e_port6_ID = nc2k_states.r1e_port6_ID;  //added for nc3000

BYTE __iocallconv Read05StartTimer0( BYTE ) // 05
{
    // SPDC1016
    qDebug("ggv wanna start timer0");
    //printf("ggv wanna start timer0\n");
    timer0run = true;
    timer0ticks = 0; // reset only on start. not on every write?
    
    return zpioregs[io02_timer0_val]; // follow rulz by GGV
}

BYTE __iocallconv Read04StopTimer0( BYTE ) // 04
{
    // SPDC1016
    qDebug("ggv wanna stop timer0");
    //printf("ggv wanna stop timer0\n");
    //BYTE r = zpioregs[io02_timer0_val];
    timer0run = false;

    return zpioregs[io02_timer0_val];// zpioregs[io04_general_ctrl];
}

BYTE __iocallconv Read07StartTimer1( BYTE ) // 07
{
    // SPDC1016
    qDebug("ggv wanna start timer1");

    if(debug_level>=1)printf("ggv wanna start timer1\n");
    timer1run_tmie = true;
    timer1ticks = 0; // useless in 16bit TMODE2 and 8bit TMODE0
    gThreadFlags &= 0xFFFDu; // Remove 0x02
    return zpioregs[io03_timer1_val];
}

BYTE __iocallconv Read06StopTimer1( BYTE ) // 06
{
    // Stop timer1, and return time1 value
    // SPDC1016
    qDebug("ggv wanna stop timer1");
    if(debug_level>=1)printf("ggv wanna stop timer1\n");
    timer1run_tmie = false;
    gThreadFlags |= 0x02; // Add 0x02
    return zpioregs[io03_timer1_val];
}

bool &lcdoffshift0flag = nc2k_states.lcdoffshift0flag;

// CKS P
// 0   OSC/8  SPEED4
// 1   OSC/4  SPEED5
// 2   OSC/2  SPEED6
// 3   OSC/1  SPEED7
// 4   OSC/64 SPEED1
// 5   OSC/32 SPEED2
// 6   OSC/16 SPEED3
// 7   clock off   
// CPU速度和OSC+CKS, 和CPS都有关系
void __iocallconv Write05ClockCtrl( BYTE write, BYTE value )
{
    // FROM WQXSIM
    // SPDC1016
    // TODO: LCDON/LCDEN separate
    if (zpioregs[io05_clock_ctrl] & 0x8) {
        // old bit3, LCDON
        // Previous LCD on
        if ((value & 0xF) == 0) {
            // new value bit0~3 is 0
            // LCD off, lcd shift clock select bit0~3 is 0
            lcdoffshift0flag = true;
        }
    }
    zpioregs[io05_clock_ctrl] = value;
    (void)write;
}

unsigned short &lcdbuffaddr = nc2k_states.lcdbuffaddr;
unsigned short &lcdbuffaddrmask = nc2k_states.lcdbuffaddrmask;

void __iocallconv Write06LCDStartAddr( BYTE write, BYTE value ) // 06
{
    // value 对应LCD地址的b11~b4
    //unsigned int t = ((zpioregs[io0C_lcd_config] & 0x3) << 12);
    //t = t | (value << 4);
    unsigned short t = (value << 4);
    lcdbuffaddr &= ~0x0FF0; // 去掉中间8bit
    lcdbuffaddr |= t;
    ////printf("ggv wanna change lcdbuf address to 0x%04x in io06\n", lcdbuffaddr & lcdbuffaddrmask);
    zpioregs[io06_lcd_config] = value;
    //lcdbuffaddr = t;
    (void)write;
    // SPDC1016
    // don't know how wqxsim works.
    //zpioregs[io09_port1_data] &= 0xFEu; // remove bit0 of port1 (keypad)
}

void __iocallconv Write0CTimer01Control( BYTE write, BYTE value ) // 0C
{
    unsigned short t = ((value & 0x3) << 12); // lc12~lc13
    //t = t | (zpioregs[io06_lcd_config] << 4); // lc4~lc11
    lcdbuffaddr &= ~0x3000;
    lcdbuffaddr |= t;
    ////printf("ggv wanna change lcdbuf address to 0x%04x in io0C\n", lcdbuffaddr & lcdbuffaddrmask);
    //qDebug("ggv also wanna change timer settings to 0x%02x.", (value & 0xC));
    w0c_b67_TMODESL = value >> 6;
    if (w0c_b67_TMODESL == 1) {
        w0c_b45_TM0S = (value >> 4) & 3;
        w0c_b23_TM1S = (value >> 2) & 3;
    } else {
        // only 3bit
        w0c_b345_TMS = (value >> 3) & 7;
    }
    if(enable_debug_timer) printf("write io0c 0x%02x\n",value);
    zpioregs[io0C_lcd_config] = value;
    (void)write;
}

void __iocallconv Write20JG( BYTE write, BYTE value )
{
    // SPDC1016

    if (value == 0x80u) {
        //memset(dword_44B988, 0, 0x20u);
        //gFixedRAM1_b20 = 0;           // mem[20] change from 80 to 00
        //LOBYTE(mayIO23Index1) = 0;
        //mayIO20Flag1 = 1;
        zpioregs[io20_JG] = 0;
    } else {
        zpioregs[io20_JG] = value;
    }
    (void)write;
}


void __iocallconv Write23Unknow( BYTE write, BYTE value )
{
    // SPDC1023
    // io23 unknown
    //currentdata = tmpAXYValue;    // current mem[23] value
    //if ( tmpAXYValue == 0xC2u )
    //{
    //    // mayIO23Index used in some waveplay routine
    //    dword_4603D4[(unsigned __int8)mayIO23Index1] = gFixedRAM1_b22;
    //}
    //else
    //{
    //    if ( tmpAXYValue == 0xC4u )
    //    {
    //        // for PC1000?
    //        dword_44EA1C[(unsigned __int8)mayIO23Index1] = gFixedRAM1_b22;
    //        LOBYTE(mayIO23Index1) = mayIO23Index1 + 1;
    //    }
    //}
    //if ( gTimer0WaveoutStarted )
    //{
    //    *(_BYTE *)maypTimer0VarA8 = currentdata;
    //    v2 = mayTimer0Var1 + 1;
    //    ++maypTimer0VarA8;
    //    overflowed = mayIO2345Var1 == 7999;
    //    ++mayTimer0Var1;
    //    ++mayIO2345Var1;
    //    if ( overflowed )
    //    {
    //        byte_4603B8[mayIO45Var3x] = 1;
    //        if ( v2 == 8000 )
    //            WriteWaveout(&pwh);
    //        mayIO2345Var1 = 0;
    //    }
    //    destaddr = mayDestAddr;
    //}
    //if ( tmpAXYValue == 0x80u )
    //{
    //    gFixedRAM1_b20 = 0x80u;
    //    mayIO20Flag1 = 0;
    //    if ( (_BYTE)mayIO23Index1 > 0u )
    //    {
    //        if ( !gTimer0WaveoutStarted )
    //        {
    //            GenerateAndPlayJGWav();
    //            destaddr = mayDestAddr;
    //            LOBYTE(mayIO23Index1) = 0;
    //        }
    //    }
    //}
    if (value == 0xC2u) {

    } else if (value == 0xC4) {

    }
    if (timer0waveoutstart) {

    }
    if (value == 0x80u) {
        if (!timer0waveoutstart) {

        }
    }
    zpioregs[io23_unknow] = value;
    (void)write;
}

// timer的值似乎是浮动的. 也就是02的值再变, 在用start时候也变回写入值? (假设错误, 都变0, 除了TMODE0以外)
//void __iocallconv Write02Timer0Value( BYTE write, BYTE value )
//{
//    // SPDC1016
//    if (timer0started) {
//        //prevtimer0value = value;
//    }
//    zpioregs[io02_timer0_val] = value;
//    //rw023_timer01val.timer0 = value;
//    (void)write;
//}


//////////////////////////////////////////////////////////////////////////
// Keypad registers
//////////////////////////////////////////////////////////////////////////
unsigned /*char*/ keypadmatrix[8][8] = {};

/* (AI written)
 * NC3000 物理引脚导通键盘模型 (SPDC1064 SoC):
 *
 * 模拟 24 个 I/O 引脚之间的无源双向开闭开关网络：
 * - 行线 (y = 0..7): 连接至 Port 1 (P10..P17)，方向由 IO $15 控制 (1=输出, 0=输入)。
 * - 列线 (x = 0..7):
 *     x = 0:    Port 0 bit 0 (P00) - 顶部热键 (网络, PDA, 计算, 时间, 开/关, 英汉, AHD, 剑桥)
 *     x = 1:    Port 6 bit 1 (P61) - 方向键与导航 (O, L, ↑, ↓, P, ⇟, 输入, →)
 *               (同时响应 Port 0 bit 1，兼容 NC2000 移植游戏)
 *     x = 2:    Port 6 bit 0 (P60) - 功能与摇杆键 (F1..F4, 跟读, 发音暂停, 发音, 复读)
 *               (同时响应 Port 0 bit 2，兼容 NC2000 移植游戏)
 *     x = 3:    Port 6 bit 2/3 (y!=3 为 P62，y==3 为 P63)
 *     x = 4:    Port 0 bit 4 (P04) - QWERTY 字母排 (Q, W, E, R, T, Y, U, I)
 *     x = 5:    Port 0 bit 5 (P05) - ASDF 字母排 (A, S, D, F, G, H, J, K)
 *     x = 6:    Port 0 bit 6 (P06) - ZXCV 字母排 (Z, X, C, V, B, N, M, ⇞)
 *     x = 7:    Port 0 bit 7 (P07) - 底部控制排 (←, 求助, 中英数, 跳出, 符号, ., 空格, 输入法)
 * - 机身侧面键:
 *     录音键 (RECORD, $22): P62 <-> P00
 *     红外接收 (IRDA, $21): P63 <-> P00
 *
 * 物理传导规律:
 *   若 Pin A 为输出且 Pin B 为输入 -> Pin B 接收 Pin A 的输出电平。
 *   若 Pin B 为输出且 Pin A 为输入 -> Pin A 接收 Pin B 的输出电平。
 *   若两端同为输入 -> 无驱动信号，均维持下拉默认电平 (0)。
 *   支持 24 个引脚任意混杂的输入/输出配置，支持高电平有效与低电平有效扫描。
 */
static void UpdateKeypadRegistersNC3000_Physical() //AI written
{
    // 1. Determine Port 1 direction: 1 = output, 0 = input
    uint8_t p1_dir = w15_port1_DIR107;

    // 2. Determine Port 0 direction from IO $0F
    uint8_t p0_dir = 0;
    if (rw0f_b4_DIR00)  p0_dir |= 0x01;
    if (rw0f_b5_DIR01)  p0_dir |= 0x02;
    if (rw0f_b6_DIR023) p0_dir |= 0x0C;
    if (rw0f_b7_DIR047) p0_dir |= 0xF0;

    // Detect if Port 0 or Port 6 is driving active-low (scanning with single 0 bit)
    bool p0_active_low = false;
    uint8_t p0_hi = w08_port0_OL & 0xF0;
    if (p0_hi == 0xE0 || p0_hi == 0xD0 || p0_hi == 0xB0 || p0_hi == 0x70) p0_active_low = true;
    uint8_t p0_lo = w08_port0_OL & 0x0F;
    if (p0_lo == 0x0E || p0_lo == 0x0D || p0_lo == 0x0B || p0_lo == 0x07) p0_active_low = true;

    bool p6_active_low = false;
    uint8_t p6_lo = w1e_port6_OL & 0x0F;
    if (p6_lo == 0x0E || p6_lo == 0x0D || p6_lo == 0x0B || p6_lo == 0x07) p6_active_low = true;

    bool col_active_low = (p0_active_low || p6_active_low);

    // Initial input states
    uint8_t p1_in_hi = 0, p1_in_lo = 0;
    uint8_t p0_in_hi = 0, p0_in_lo = 0;
    uint8_t p6_in_hi = 0, p6_in_lo = 0;

    // 3. Main Matrix Conduction: 8 rows (P10..P17) x 8 cols (P0/P6)
    for (int y = 0; y < 8; y++) {
        bool a_is_out = (p1_dir & (1 << y)) != 0;
        bool a_val    = (w09_port1_OL & (1 << y)) != 0;

        for (int x = 0; x < 8; x++) {
            if (!keypadmatrix[y][x]) continue;

            // Pin B: Column line
            bool b_is_out = false;
            bool b_val = false;
            int port_type = 0; // 0 = Port 0, 6 = Port 6
            int pin_bit = 0;

            bool p0_is_driving = (p0_dir != 0);

            if (x == 0) {
                port_type = 0; pin_bit = 0;
                b_is_out = (p0_dir & 0x01) != 0;
                b_val    = (w08_port0_OL & 0x01) != 0;
            } else if (x == 1) {
                // NC3000 hardware is P61 (bit 1 of Port 6)
                port_type = 6; pin_bit = 1;
                bool nc2k_col = ((w08_port0_OL & 0x02) != 0) && ((p0_dir & 0x02) != 0);
                bool p6_col = !p0_is_driving && ((w1e_port6_OL & 0x02) != 0);
                b_is_out = p6_col || nc2k_col;
                b_val    = p6_col || nc2k_col;
            } else if (x == 2) {
                // NC3000 hardware is P60 (bit 0 of Port 6)
                port_type = 6; pin_bit = 0;
                bool nc2k_col = ((w08_port0_OL & 0x04) != 0) && ((p0_dir & 0x04) != 0);
                bool p6_col = !p0_is_driving && ((w1e_port6_OL & 0x01) != 0);
                b_is_out = p6_col || nc2k_col;
                b_val    = p6_col || nc2k_col;
            } else if (x == 3) {
                port_type = 6; pin_bit = (y == 3) ? 3 : 2;
                uint8_t p6_mask = (y == 3) ? 0x08 : 0x04;
                bool nc2k_col = ((w08_port0_OL & 0x08) != 0) && ((p0_dir & 0x08) != 0);
                bool p6_col = !p0_is_driving && ((w1e_port6_OL & p6_mask) != 0);
                b_is_out = p6_col || nc2k_col;
                b_val    = p6_col || nc2k_col;
            } else {
                port_type = 0; pin_bit = x;
                b_is_out = ((p0_dir & (1 << pin_bit)) != 0) || p0_active_low;
                b_val    = (w08_port0_OL & (1 << pin_bit)) != 0;
            }

            // Conduction Rule: Pin A (Port 1) <-> Pin B (Port 0 or Port 6)
            // Case A -> B: Row is output, Column is input
            if (a_is_out && !b_is_out) {
                if (a_val) {
                    if (port_type == 0) p0_in_hi |= (1 << pin_bit);
                    if (port_type == 6) {
                        p6_in_hi |= (1 << pin_bit);
                        // Reflect to Port 0 for NC2000 compatibility if Port 0 is in input mode
                        if (x == 1 && !(p0_dir & 0x02)) p0_in_hi |= 0x02;
                        if (x == 2 && !(p0_dir & 0x04)) p0_in_hi |= 0x04;
                        if (x == 3 && !(p0_dir & 0x08)) p0_in_hi |= 0x08;
                    }
                    // Special wake-up / ON-OFF key at (4, 0)
                    if (y == 4 && x == 0 && port_type == 0) p0_in_hi |= 0x04;
                } else {
                    if (port_type == 0) p0_in_lo |= (1 << pin_bit);
                    if (port_type == 6) {
                        p6_in_lo |= (1 << pin_bit);
                        if (x == 1 && !(p0_dir & 0x02)) p0_in_lo |= 0x02;
                        if (x == 2 && !(p0_dir & 0x04)) p0_in_lo |= 0x04;
                        if (x == 3 && !(p0_dir & 0x08)) p0_in_lo |= 0x08;
                    }
                }
            }
            // Case B -> A: Column is output, Row is input
            else if (b_is_out && !a_is_out) {
                if (col_active_low) {
                    if (!b_val) p1_in_lo |= (1 << y);
                } else {
                    if (b_val) p1_in_hi |= (1 << y);
                }
            }
        }
    }

    // 4. Side keys (RECORD: P62 <-> P00, IRDA: P63 <-> P00)
    if (keypadmatrix[0][3]) { // RECORD
        if (w1e_port6_OL & 0x04) p0_in_hi |= 0x01;
    }
    if (keypadmatrix[3][3]) { // IRDA
        if (w1e_port6_OL & 0x08) p0_in_hi |= 0x01;
    }

    // 5. Synthesize final input registers
    if (col_active_low) {
        r09_port1_ID = (p1_dir & w09_port1_OL) | (~p1_dir & ~p1_in_lo);
    } else {
        r09_port1_ID = (p1_dir & w09_port1_OL) | (~p1_dir & p1_in_hi);
    }

    r08_port0_ID = (p0_dir & w08_port0_OL) | (~p0_dir & p0_in_hi & ~p0_in_lo);
    r1e_port6_ID = (w1e_port6_OL | p6_in_hi) & ~p6_in_lo;
}

void UpdateKeypadRegisters()
{
    if (nc3000mode) {
        UpdateKeypadRegistersNC3000_Physical();
        return;
    }

    const bool use_pull_high_emulation = true;
    // if( (~ext_reg[0x24])&0xf) enable_key_debug_once=1;
    // TODO: 2pass check
    // 设port0/port1都有下拉电阻, 并且输入没有锁存. 也即如果设置为输入, 没有导电橡胶从别的线路拉高时候, 自动会变0
    // 计算可以用2种方法, 1, 循环matrix, 对每个节点求传导. 2循环
    unsigned char port1control = w15_port1_DIR107;
    unsigned char port0control = rw0f_b4_DIR00 << 4 | rw0f_b5_DIR01 << 5 | rw0f_b6_DIR023 << 6 | rw0f_b7_DIR047 << 7;// // b4~b7
    if(enable_key_debug_once){
        printf("[key_debug] port1control=%02x, port0control=%02x\n", port1control, port0control);
    }
    unsigned char port1controlbit = 1; // aka, y control bit
    unsigned char tmpdest0 = 0, tmpdest1 = 0;
    if(use_pull_high_emulation){
        if(nc1020mode||nc2000mode){//handle port0 pull high
                tmpdest0 = (~ext_reg[0x24])&0xf;
        }
    }
    unsigned short tmpp30tv = 0;
    // 我已在WritePort0和WritePort1时候, 传导了输出状态的引脚的电平到输入
    // 不不, 这里应该先取输出锁存器的
    unsigned char port1data = w09_port1_OL, port0data = w08_port0_OL;
    bool yreceive = false;
    for (int y = 0; y < 8; y++) {
        // y0,y1 = P30 (can only receive)
        // y2~7 = P12~P17
        bool ysend = ((port1control & port1controlbit) != 0);
        if (pc1000mode && y < 2) {
            //ysend = false;
            yreceive = (zpioregs[io07_port_config] & 0x04) == 0;
            ysend = !yreceive;
            if (yreceive == false) {
                qDebug("yreceive:%d", yreceive);
            }
        }
        unsigned char xbit = 1;
        for (int x = 0; x < 8; x++) {
            // x = Port00~Port07
            unsigned char port0controlbit;
            if (x < 2) {
                // 0, 1 = b4 b5
                port0controlbit = xbit << 4;
            } else if (x < 4) {
                // 2, 3 = b6
                port0controlbit = 0x40;
            } else {
                // 4, 5, 6, 7 = b7
                port0controlbit = 0x80u;
            }
            bool xsend = ((port0control & port0controlbit) != 0);

            if(enable_key_debug_once){
                printf("[key_debug] x=%d, y=%d, xsend=%d, ysend=%d %02x %02x\n", x, y, xsend, ysend,port1data,port1controlbit);
            }

            //nc2000 doesn't have this special P30 row
            if (pc1000mode && y == 0) {
                // Special P30 "row"
                if (x == 0) {
                    // P10
                    xsend = port1control & 1;
                }
                if (x == 1) {
                    // P11
                    xsend = port1control & 2;
                }
                if (x == 2) {
                    // on/off is GNDxP30, confirmed by CC800 PCB scan, thx Sailor-HB
                    xsend = true;
                }
            }
            if (ysend != xsend) {
                if (ysend) {
                    // port1y-> port0x, and x is receive
                    if (keypadmatrix[y][x]==1 ) {
                        if((port1data & port1controlbit) != 0){
                            tmpdest0 |= xbit;
                        }else if(use_pull_high_emulation){//needed by the pull high case
                            if(nc1020mode||nc2000mode){
                                if(xbit &0x0f && (ext_reg[0x24] & xbit) ==0){//in theory this if is not needed
                                    tmpdest0 &= ~xbit;
                                }
                            }
                        }
                    }
                } else {
                    // port0x -> port1y, and y is receive
                    // port0,port1 -> p30
                    if (y >= 2 || nc2000mode || nc1020mode) {
                        if (keypadmatrix[y][x]==1 && ((port0data & xbit) != 0)) {
                            tmpdest1 |= port1controlbit;
                        }
                        //nc2000 etc never runs to below elses
                    } else if (y == 1) {
                        // hotkey
                        if (keypadmatrix[y][x]==1 && ((port0data & xbit) == 0)) {
                            tmpp30tv |= xbit;
                        }
                    } else {
                        // rewind, record, on/off, ir
                        if (x < 2) {
                            // port1 and xcontrolbit!!!
                            if (keypadmatrix[y][x]==1 && ((port1data & xbit) == 0)) {
                                tmpp30tv |= 1 << (x + 8);
                            }
                        } else if (x == 2) {
                            // on/off key
                            if (keypadmatrix[y][x]==1) {
                                tmpp30tv |= 1 << (x + 8);
                             }
                        }
                    }
                }
            }

            xbit = xbit << 1;
        }
        port1controlbit = port1controlbit << 1;
    }
    port1data = r09_port1_ID;
    port0data = r08_port0_ID;
    // 将port1里面对应于"输入"的都清掉. (此处不是因为下拉电阻, 而是配合tmpdest1里面省掉的传导为0的操作?)
    // 如果彻底模拟, 还要模拟出tmpdest1_lo, 用于完成此处的AND
    // 但是P10/P11要不要清掉?
    // 判断前清还是判断后清?
    if (port1control != 0xFFu) {
        // port1 should clean some bits
        // using port1control as port1mask
        // sometimes port10,11 should clean here 
        port1data &= port1control; // pre set receive bits to 0
    }
    // 将port0里面对应于"输入"的都清掉.
    // TODO: use rw0f_b4_DIR00
    if (port0control != 0xF0u) {
        // clean port0
        // calculate port0 mask
        // in most case port0 will be set to 0
        unsigned char port0mask = (port0control >> 4) & 0x3; // bit4->DIR00 bit5->DIR01
        if (port0control & 0x40) {
            // bit6->DIR02,DIR03
            port0mask |= 0x0C; // 00001100
        }
        if (port0control & 0x80u) {
            // bit7->DIR04,05,06,07
            port0mask |= 0xF0u; // 11110000
        }
        port0data &= port0mask;
    }
    port0data |= tmpdest0;
    port1data |= tmpdest1;
    if (r09_port1_ID != port1data || r08_port0_ID != port0data) {
        qDebug("old [0015]:%02x [0009]:%02x [0008]:%02x", w15_port1_DIR107, r09_port1_ID, r08_port0_ID);
        qDebug("new [0015]:%02x [0009]:%02x [0008]:%02x", w15_port1_DIR107, port1data, port0data);
    }

  if(!use_pull_high_emulation){ //no longer needed, but kept for compare
    // this is tmp fix for nc2000 hotkey wakeup
    // todo: better fix, probably need to handle below:
    //       when port0[3:0] defined as input, it got "on" function and is pulled high. (it's controled by P0PU)
    if(nc2000mode||nc1020mode) {
        if(port1control==0xff&&port0control==0xc0 &&w09_port1_OL==0x00){
            bool hot_key_pressed=false;
            // note: port0control==0xc0 ----->rw0f_b4_DIR00 ==0x00 && rw0f_b5_DIR01 ==0x00
            for(int y=0;y<8;y++){ 
                if(keypadmatrix[y][1]){  // if any of this row is pressed, the pull high value get cleared
                    hot_key_pressed=true; 
                }
            }
            if(hot_key_pressed){
                port0data |=0x03;
                port0data&=~0x02;
            }
        }
    }
    if(nc1020mode){ //this is a similiar hack since pull high is not implemented correctly yet
        //seems like only 1020tw uses this
        if(port1control==0x00&&port0control==0x00){
            port0data|= 0x03;
        }
    }
  }

    r09_port1_ID = port1data;
    r08_port0_ID = port0data;

    //printf("<port0=%d port1=%d tmpp30tv=%d>\n",port0data,port1data, tmpp30tv);
    if(pc1000mode){
        if (tmpp30tv) {
            //有开机或热键按下, 应当改P30为0
            qDebug("P30 hotkey scan: %04X", tmpp30tv);
            if (yreceive) {
                zpioregs[io0B_port3_data] &= 0xFE;
            }
        } else if (yreceive) {
            zpioregs[io0B_port3_data] |= 1;
        }
    }
    if(enable_key_debug_once) {
        printf("[key_debug] new r08_port0_ID=%02x, r09_port1_ID=%02x, tmpp30tv=%04x, tmpdest0=%02x tmpdest1=%02x\n", r08_port0_ID, r09_port1_ID, tmpp30tv, tmpdest0, tmpdest1);
    }
    if(enable_key_debug_once>0) enable_key_debug_once--;
}

BYTE __iocallconv ReadPort0( BYTE read )
{
    UpdateKeypadRegisters();
    //qDebug("ggv wanna read keypad port0, [%04x] -> %02x", read, mem[read]);
    return r08_port0_ID;
    (void)read;
}

BYTE __iocallconv ReadPort1( BYTE read )
{
    // Reset by IBF change from 1 to 0
    zpioregs[io01_int_status] &= ~0x80; //b7: IBF

    UpdateKeypadRegisters();
    //qDebug("ggv wanna read keypad port1, [%04x] -> %02x", read, mem[read]);
    return r09_port1_ID;
    (void)read;
}

void __iocallconv Write08Port0( BYTE write, BYTE value )
{
    //qDebug("ggv wanna write keypad port0, [%04x] (%02x) -> %02x", write, mem[write], value);
    w08_port0_OL = value; // set output latches first

    BYTE passmask = 0, passdata = 0;
    // distribute to input data
    if (rw0f_b4_DIR00) {
        passmask |= 1;
        passdata |= value & 1;
        }
    if (rw0f_b5_DIR01) {
        passmask |= 2;
        passdata |= value & 2;
    }
    if (rw0f_b6_DIR023) {
        passmask |= 0xC;
        passdata |= value & 0xC;
    }
    if (rw0f_b7_DIR047) {
        passmask |= 0xF0;
        passdata |= value & 0xF0;
    }
    r08_port0_ID = (r08_port0_ID & ~passmask) | passdata;
    UpdateKeypadRegisters();
    (void)write;
}

void __iocallconv Write09Port1( BYTE write, BYTE value )
{
    //qDebug("ggv wanna write keypad port1, [%04x] (%02x) -> %02x", write, mem[write], value);
    w09_port1_OL = value; // latches
    // TODO: apply these pass-though algorithm after PTYPE changed in Write04General control?
    if (w04_b46_PTYPE == 0 || w04_b46_PTYPE == 5) {
        BYTE passmask = 0, passdata = 0;
        if (w04_b46_PTYPE == 0) {
            passmask |= w15_port1_DIR107 & 0xF;
            passdata |= value & 0xF;
        }
        passmask |= w15_port1_DIR107 & 0xF0;
        passdata |= value & 0xF0;

        r09_port1_ID = (r09_port1_ID & ~passmask) | passdata;
        }
    // Reset by OBE change from 1 to 0
    zpioregs[io01_int_status] &= ~0x40; // b6: OBF

    UpdateKeypadRegisters();
    (void)write;
    }

unsigned char &cpf=nc2k_states.cpf;  
unsigned char &lcden=nc2k_states.lcden; 
void __iocallconv Write0BPort3LCDStartAddr( BYTE write, BYTE value )
{
    // 控制LCD地址有效位数
    unsigned short b6b5 = (value & 0x60) >> 5;
    cpf=(value>>2)&7;
    lcden=(value>>1)&1;
    if(debug_level>=2) printf("Write0BPort3LCDStartAddr %02x b6b5=%02x cpf=%02x lcden=%02x\n",value,b6b5,cpf,lcden);
    // CPU   A15 A14 A13 A12 A11 A10 A9 A8 A7 A6 A5 A4
    // LCD   0   0   L13 L12 L11 L10 L9 L8 L7 L6 L5 L4     for LCDX1=0  LCDX0=0 3FFF
    // LCD   0   0   0   L12 L11 L10 L9 L8 L7 L6 L5 L4     for LCDX1=0  LCDX0=1 1FFF
    // LCD   0   0   0   0   L11 L10 L9 L8 L7 L6 L5 L4     for LCDX1=1  LCDX0=0 0FFF
    // LCD   0   0   0   0   0   L10 L9 L8 L7 L6 L5 L4     for LCDX1=1  LCDX0=1 07FF
    lcdbuffaddrmask = 0x3FFF >> b6b5;
    qDebug("ggv wanna change lcdbuf address to 0x%04x in io0B", lcdbuffaddr & lcdbuffaddrmask);
    // P30 should always input?
    // TODO: output latch
    zpioregs[io0B_port3_data] = (value & 0xFE) | (zpioregs[io0B_port3_data] & 1);
    (void)write;
    }

void __iocallconv Write15Dir1( BYTE write, BYTE value )
{
    //qDebug("ggv wanna config keypad port1, [%04x] (%02x) -> %02x", write, mem[write], value);
    w15_port1_DIR107 = value;
    UpdateKeypadRegisters();
    (void)write;
    }

void __iocallconv Write19CkvSelect( BYTE write, BYTE value )
{
    // 19不读取?!
    zpioregs[io19_ckv_select] = value;
    hotlinkios->w19_b6_P46T = (value & 0x40) != 0;
    hotlinkios->w19_b5_P45T = (value & 0x20) != 0;
    (void)write;
        }

void __iocallconv Write07PortConfig( BYTE write, BYTE value )
{
    zpioregs[io07_port_config] = value;
    hotlinkios->w07_b6_DIR46 = (value & 0x40) != 0;
    hotlinkios->w07_b5_DIR45 = (value & 0x20) != 0;

    if (hotlinkios->w19_b6_P46T == 0 && hotlinkios->w19_b5_P45T == 0) {
        qDebug("DIR CLK%c: DATA%c", hotlinkios->w07_b5_DIR45?'O':'I',  hotlinkios->w07_b6_DIR46?'O':'I');
    }

    (void)write;
        }

void __iocallconv Write18Port4( BYTE write, BYTE value )
{
    zpioregs[io18_port4_data] = value;
    hotlinkios->w18_b6_P46OL = (value & 0x40) != 0;
    hotlinkios->w18_b5_P45OL = (value & 0x20) != 0;

    if (hotlinkios->w19_b6_P46T == 0 && hotlinkios->w19_b5_P45T == 0) {
        qDebug("W CLK%c: O%dI%d, DATA%c: O%dI%d", hotlinkios->w07_b5_DIR45?'O':'I', hotlinkios->w18_b5_P45OL,hotlinkios->r18_b5_P45ID,  hotlinkios->w07_b6_DIR46?'O':'I',  hotlinkios->w18_b6_P46OL,hotlinkios->r18_b6_P46ID);
    }

    int a=value &0x80;
    if (a==0) a=-1;
    void beeper_on_io_write(int);
    beeper_on_io_write(a);

    (void)write;
    }

BYTE __iocallconv Read18Port4( BYTE )
{
    // Data
    if (hotlinkios->w19_b6_P46T == 0) {
        if (hotlinkios->w07_b6_DIR46) {
            // short logic
            zpioregs[io18_port4_data] = (zpioregs[io18_port4_data] & ~0x40) | hotlinkios->w18_b6_P46OL << 6;
        } else {
            zpioregs[io18_port4_data] = (zpioregs[io18_port4_data] & ~0x40) | hotlinkios->r18_b6_P46ID << 6;
    }
        }
    // Clock
    if (hotlinkios->w19_b5_P45T == 0) {
        // CMOV?
        zpioregs[io18_port4_data] = (zpioregs[io18_port4_data] & ~0x20) | (hotlinkios->w07_b5_DIR45?hotlinkios->w18_b5_P45OL:hotlinkios->r18_b5_P45ID) << 5;
    }
    if (hotlinkios->w19_b6_P46T == 0 && hotlinkios->w19_b5_P45T == 0) {
        //qDebug("CLK%c:%d, DATA%c:%d", hotlinkios->w07_b5_DIR45?'O':'I', (zpioregs[io18_port4_data] & 0x20) != 0,  hotlinkios->w07_b6_DIR46?'O':'I', (zpioregs[io18_port4_data] & 0x40) != 0);
        qDebug("CLK%c: O%dI%d, DATA%c: O%dI%d", hotlinkios->w07_b5_DIR45?'O':'I', hotlinkios->w18_b5_P45OL,hotlinkios->r18_b5_P45ID,  hotlinkios->w07_b6_DIR46?'O':'I',  hotlinkios->w18_b6_P46OL,hotlinkios->r18_b6_P46ID);
    }
    if(nc1020mode||nc2000mode||nc3000mode){
        //nc3000c-lee has it but seems like no need?
        return zpioregs[io18_port4_data]|0x20;
    }
    return zpioregs[io18_port4_data];
}

BYTE __iocallconv Read01IntStatus( BYTE )
{
    BYTE r = zpioregs[io01_int_status];
    //printf("read 01 %02x\n",r);
    zpioregs[io01_int_status] = r & ~0x3F;
    return r;
    }

void __iocallconv Write01IntEnable( BYTE write, BYTE value )
{
    //printf("write 01 %02x\n",value);
    w01_int_enable = value;
    (void)write;
}

// TODO: combine back in read04, or need update every time w04_bxx_YYY value changed
void __iocallconv Write04GeneralCtrl(BYTE write, BYTE value)
{
    w04_b46_PTYPE = value >> 4 & 7;
    if (w04_b46_PTYPE) {
        qDebug("PTYPE:%d", w04_b46_PTYPE);
    }
    w04_b7_EPOL = (value & 0x80) != 0;
    w04_b03_TBC = value & 0xF;
    //printf("write 04 %02x\n",value);
    zpioregs[io04_general_ctrl] = value;
    (void)write;
}

HotlinkBundle* hotlinkios = nullptr;

// For P45,P45
// ERROR_ACCESS_DENIED for Global prefix (SeCreateGlobalPrivilege)
void CreateHotlinkMapping()
{
    if(hotlinkios) {free(hotlinkios);hotlinkios = nullptr;}
    hotlinkios = (HotlinkBundle*)malloc(sizeof(HotlinkBundle));
    memset(hotlinkios, 0, sizeof(HotlinkBundle));
}



BYTE __iocallconv Read1EPort6( BYTE read ) //added for nc3000
{
    UpdateKeypadRegisters();
    return r1e_port6_ID;
    (void)read;
}

/* (AI written)
 * Port 6 is the NC3000's other half of the keyboard column lines.
 *
 * The keyboard is 8 column lines x 8 row lines, and port 1 carries the rows:
 *   - port 0 bits 4..7  drive 4 column lines (the firmware's table at $94B6,
 *     entries 10 20 40 80 = one bit high, and EF DF BF 7F = one bit low)
 *   - port 6 bits 0..3  drive the other 4 column lines ($94AE: 01 02 04 08)
 * With no key pressed the driven level simply reads back on port 1 (the two
 * ports share the row wires), which is why the firmware compares the read
 * against the pattern it just wrote.  Pressing a key connects one column line
 * to one row line and perturbs that value.
 */
void __iocallconv Write1EPort6( BYTE write, BYTE value ) //added for nc3000
{
    w1e_port6_OL = value;
    UpdateKeypadRegisters();
    (void)write;
}
