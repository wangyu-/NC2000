#include "NekoDriver.h"
extern "C" {
#ifdef HANDYPSP
#include "ANSI/w65c02.h"
#else
#include "ANSI/65C02.h"
#endif
}
#include "CC800IOName.h"
#include "NekoDriverIO.h"
#include <string.h>


bool timer0run = false;
bool timer1run_tmie = false;

// WQXSIM
bool timer0waveoutstart = false;
int prevtimer0value = 0;
unsigned short gThreadFlags;
//unsigned char* gGeneralCtrlPtr;
//unsigned short mayGenralnClockCtrlValue;

// Full MOS IO Ports?
// (I/O) io_zp_bsw
bool rw0f_b4_DIR00 = 0;
bool rw0f_b5_DIR01 = 0;
bool rw0f_b6_DIR023 = 0; // 02 03
bool rw0f_b7_DIR047 = 0; // 04 05 06 07
bool rw0f_b3_SH = 0;    // Sample & Hold for A/D
BYTE rw0f_b02_ZB02 = 0; // b0..b2 (RESCPUB)

// (O/P) io_general_ctrl
bool w04_b7_EPOL = 0;   // 外部中断 (P40 OR P41 OR P00) 极性
BYTE w04_b46_PTYPE = 0; // Port1 PTYPE0~PTYPE7
BYTE w04_b03_TBC = 0;   // LCD地址线, Timebase时钟

// (O/P) io_port1_dir
// 受限于PTYPE0|5
BYTE w15_port1_DIR107 = 0;// DIR10~DIR17

// (I/O) 读取时候逐位判断DIR, 确定从ID(matrix更新)还是OL直接读取
// 假设速度, 假设1016的输入比6502的执行速度快很多, 例如延迟在10ns, 则基本可以当作输出延迟+输入延迟在STA执行途中已过去.
// 假设短路, 遇到2个都是输出, 一高一低, matrix连通了他们2者, 则实际因为是导电橡胶联通的, 实际输出高的pmos+导电橡胶+nmos的Rds构成分压网络.
// 因此定出优化规则: 在改变端口方向和写入端口时候, 立刻同步刷新输入数据. 等同于我们加了缓冲. 而处理按键时候, 忽略输出对输出的传导.
// 实际流程既是: 先复制输出状态引脚, 再处理导电橡胶传导.
BYTE w08_port0_OL = 0;  // output latch
BYTE r08_port0_ID = 0;  // input data

BYTE w09_port1_OL = 0;
BYTE r09_port1_ID = 0;

timer01_u* rw023_timer01val = (timer01_u*)&zpioregs[io02_timer0_val];

BYTE w0c_b67_TMODESL = 0;    // 01一起的计数方式
BYTE w0c_b45_TM0S = 0;       // timer0时钟周期, 在TMODE1下接入
BYTE w0c_b23_TM1S = 0;       // timer1时钟周期, 在TMODE1下接入
BYTE w0c_b345_TMS = 0;       // 其他模式下4个bit只有3个用上

// Temp
unsigned char zpioregs[0x40];

int timer0ticks = 0;
int timer1ticks = 0;

BYTE w01_int_enable = 0;

BYTE __iocallconv Read05StartTimer0( BYTE ) // 05
{
    // SPDC1016
    qDebug("ggv wanna start timer0");
    timer0run = true;
    timer0ticks = 0; // reset only on start. not on every write?
    
    return zpioregs[io02_timer0_val]; // follow rulz by GGV
}

BYTE __iocallconv Read04StopTimer0( BYTE ) // 04
{
    // SPDC1016
    qDebug("ggv wanna stop timer0");
    //BYTE r = zpioregs[io02_timer0_val];
    timer0run = false;

    return zpioregs[io02_timer0_val];// zpioregs[io04_general_ctrl];
}

BYTE __iocallconv Read07StartTimer1( BYTE ) // 07
{
    // SPDC1016
    qDebug("ggv wanna start timer1");
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
    timer1run_tmie = false;
    gThreadFlags |= 0x02; // Add 0x02
    return zpioregs[io03_timer1_val];
}

bool lcdoffshift0flag = false;

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

unsigned short lcdbuffaddr = 0x09C0;
unsigned short lcdbuffaddrmask = 0x0FFF;

void __iocallconv Write06LCDStartAddr( BYTE write, BYTE value ) // 06
{
    // value 对应LCD地址的b11~b4
    //unsigned int t = ((zpioregs[io0C_lcd_config] & 0x3) << 12);
    //t = t | (value << 4);
    unsigned short t = (value << 4);
    lcdbuffaddr &= ~0x0FF0; // 去掉中间8bit
    lcdbuffaddr |= t;
    printf("ggv wanna change lcdbuf address to 0x%04x in io06\n", lcdbuffaddr & lcdbuffaddrmask);
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
    printf("ggv wanna change lcdbuf address to 0x%04x in io0C\n", lcdbuffaddr & lcdbuffaddrmask);
    //qDebug("ggv also wanna change timer settings to 0x%02x.", (value & 0xC));
    w0c_b67_TMODESL = value >> 6;
    if (w0c_b67_TMODESL == 1) {
        w0c_b45_TM0S = (value >> 4) & 3;
        w0c_b23_TM1S = (value >> 2) & 3;
    } else {
        // only 3bit
        w0c_b345_TMS = (value >> 3) & 7;
    }
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
unsigned /*char*/ keypadmatrix[8][8] = {0,};

void UpdateKeypadRegisters()
{
    // TODO: 2pass check
    // 设port0/port1都有下拉电阻, 并且输入没有锁存. 也即如果设置为输入, 没有导电橡胶从别的线路拉高时候, 自动会变0
    // 计算可以用2种方法, 1, 循环matrix, 对每个节点求传导. 2循环
    unsigned char port1control = w15_port1_DIR107;
    unsigned char port0control = rw0f_b4_DIR00 << 4 | rw0f_b5_DIR01 << 5 | rw0f_b6_DIR023 << 6 | rw0f_b7_DIR047 << 7;// // b4~b7
    unsigned char port1controlbit = 1; // aka, y control bit
    unsigned char tmpdest0 = 0, tmpdest1 = 0;
    unsigned short tmpp30tv = 0;
    // 我已在WritePort0和WritePort1时候, 传导了输出状态的引脚的电平到输入
    // 不不, 这里应该先取输出锁存器的
    unsigned char port1data = w09_port1_OL, port0data = w08_port0_OL;
    bool yreceive = false;
    for (int y = 0; y < 8; y++) {
        // y0,y1 = P30 (can only receive)
        // y2~7 = P12~P17
        bool ysend = ((port1control & port1controlbit) != 0);
        if (y < 2) {
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
            if (y == 0) {
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
                    if (keypadmatrix[y][x]==1 && ((port1data & port1controlbit) != 0)) {
                        tmpdest0 |= xbit;
                    }
                } else {
                    // port0x -> port1y, and y is receive
                    // port0,port1 -> p30
                    if (y >= 2) {
                        if (keypadmatrix[y][x]==1 && ((port0data & xbit) != 0)) {
                        tmpdest1 |= xbit;
                    }
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
    r09_port1_ID = port1data;
    r08_port0_ID = port0data;
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

void __iocallconv Write0BPort3LCDStartAddr( BYTE write, BYTE value )
{
    // 控制LCD地址有效位数
    unsigned short b6b5 = (value & 0x60) >> 5;
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
    return zpioregs[io18_port4_data];
    }

BYTE __iocallconv Read01IntStatus( BYTE )
{
    BYTE r = zpioregs[io01_int_status];
    zpioregs[io01_int_status] = r & ~0x3F;
    return r;
    }

void __iocallconv Write01IntEnable( BYTE write, BYTE value )
{
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
    zpioregs[io04_general_ctrl] = value;
    (void)write;
}

HotlinkBundle* hotlinkios = 0;


// For P45,P45
// ERROR_ACCESS_DENIED for Global prefix (SeCreateGlobalPrivilege)
void CreateHotlinkMapping()
{
    hotlinkios = (HotlinkBundle*)malloc(sizeof(HotlinkBundle));
    memset(hotlinkios, 0, sizeof(HotlinkBundle));
}

void RemoveHotlinkMapping()
{
    free(hotlinkios);
}
