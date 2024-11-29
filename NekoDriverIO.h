#ifndef NEKODRIVER_IO_H
#define NEKODRIVER_IO_H

typedef unsigned char BYTE;
#define __iocallconv



// Full MOS IO Ports?
// (I/O) io_zp_bsw
extern bool rw0f_b4_DIR00;
extern bool rw0f_b5_DIR01;
extern bool rw0f_b6_DIR023; // 02 03
extern bool rw0f_b7_DIR047; // 04 05 06 07
extern bool rw0f_b3_SH;    // Sample & Hold for A/D
extern BYTE rw0f_b02_ZB02; // b0..b2 (RESCPUB)

// (O/P) io_general_ctrl
extern bool w04_b7_EPOL;   // 外部中断 (P40 OR P41 OR P00) 极性
extern BYTE w04_b46_PTYPE; // Port1 PTYPE0~PTYPE7
extern BYTE w04_b03_TBC;   // LCD地址线, Timebase时钟

// (O/P) io_port1_dir
// 受限于PTYPE0|5
extern BYTE w15_port1_DIR107;// DIR10~DIR17

// (I/O) 读取时候逐位判断DIR, 确定从ID(matrix更新)还是OL直接读取
// 假设速度, 假设1016的输入比6502的执行速度快很多, 例如延迟在10ns, 则基本可以当作输出延迟+输入延迟在STA执行途中已过去.
// 假设短路, 遇到2个都是输出, 一高一低, matrix连通了他们2者, 则实际因为是导电橡胶联通的, 实际输出高的pmos+导电橡胶+nmos的Rds构成分压网络.
// 因此定出优化规则: 在改变端口方向和写入端口时候, 立刻同步刷新输入数据. 等同于我们加了缓冲. 而处理按键时候, 忽略输出对输出的传导.
// 实际流程既是: 先复制输出状态引脚, 再处理导电橡胶传导.
extern BYTE w08_port0_OL;  // output latch
extern BYTE r08_port0_ID;  // input data

extern BYTE w09_port1_OL;
extern BYTE r09_port1_ID;

// TODO: endian on non x86/arm target
union timer01_u {
    unsigned short timer01;
    struct {
        unsigned char timer0;
        unsigned char timer1;
    };
};

extern timer01_u* rw023_timer01val;

// 假设spdc的timer如下: 没有接入到io的参数值, 依然存在于外设中, 也即如果io bit被链接到其他设备, 已经设置好的还保持?
extern BYTE w0c_b67_TMODESL;    // 01一起的计数方式
extern BYTE w0c_b45_TM0S;       // timer0时钟周期, 在TMODE1下接入
extern BYTE w0c_b23_TM1S;       // timer1时钟周期, 在TMODE1下接入
extern BYTE w0c_b345_TMS;       // 其他模式下4个bit只有3个用上


//extern BYTE rw19_b6_P46T;
//extern BYTE rw19_b6_P45T;

struct HotlinkBundle
{
    bool w19_b6_P46T;  // 0: P46 io, 1:TMAO
    bool w19_b5_P45T;  // 0: p45 io, 1:TMAGI
    bool w07_b6_DIR46;  // 只写, P46方向. 当P46T为1时是TMAO1
    bool w07_b5_DIR45;  // 只写, P45方向. 当P45T为1时是TMAGP
    bool w18_b6_P46OL;  // 输出锁存, 充当DAT
    bool r18_b6_P46ID;  // 输入实时状态
    bool w18_b5_P45OL;  // 输出锁存, 充当CLK
    bool r18_b5_P45ID;
    int remotelogpos;
    char remotelogbuf[0x4000];
};

extern HotlinkBundle* hotlinkios;

// Temp
//////extern unsigned char* zpioregs[0x40];
extern unsigned char* zpioregs;

#define _ADD_TM0I_BIT() zpioregs[io01_int_status] |= 0x10
#define _ADD_TM1I_BIT() zpioregs[io01_int_status] |= 0x20

// io01_int_status use zpioregs.
extern BYTE w01_int_enable;

extern int timer0ticks;
extern int timer1ticks;

BYTE __iocallconv NullRead (BYTE read);
void __iocallconv NullWrite (BYTE write, BYTE value);

BYTE __iocallconv Read04StopTimer0 (BYTE read); // $04
BYTE __iocallconv Read05StartTimer0 (BYTE read); // $05
BYTE __iocallconv Read07StartTimer1 (BYTE read); // $07
BYTE __iocallconv Read06StopTimer1 (BYTE read); // $06
BYTE __iocallconv ReadPort0 (BYTE read); // $08
BYTE __iocallconv ReadPort1 (BYTE read); // $09
BYTE __iocallconv Read18Port4 (BYTE read); // $09
BYTE __iocallconv ReadPort6EXP (BYTE read); // 0x1e //newly added

BYTE __iocallconv Read00BankSwitch (BYTE read); // $00
void __iocallconv Write00BankSwitch (BYTE write, BYTE value); // $00
BYTE __iocallconv Read01IntStatus (BYTE read); // $01
void __iocallconv Write01IntEnable (BYTE write, BYTE value); // $01
//void __iocallconv Write02Timer0Value(BYTE write, BYTE value); // $02
void __iocallconv Write04GeneralCtrl(BYTE write, BYTE value); // $04
void __iocallconv Write05ClockCtrl(BYTE write, BYTE value); // $05
void __iocallconv Write06LCDStartAddr (BYTE write, BYTE value); // $06
void __iocallconv Write0BLCDStartAddrPort3(BYTE write, BYTE value); // $0B
void __iocallconv Write0CTimer01Control (BYTE write, BYTE value); // $0C
void __iocallconv Write08Port0 (BYTE write, BYTE value); // $08
void __iocallconv Write09Port1 (BYTE write, BYTE value); // $09
void __iocallconv Write0BPort3LCDStartAddr (BYTE write, BYTE value); // $0B
void __iocallconv Write15Dir1( BYTE write, BYTE value ); // $15
void __iocallconv Write19CkvSelect(BYTE write, BYTE value); // $19
void __iocallconv Write07PortConfig (BYTE write, BYTE value); // $07
void __iocallconv Write18Port4(BYTE write, BYTE value); // $18

void __iocallconv WriteZeroPageBankswitch (BYTE write, BYTE value); // $0F
void __iocallconv Write0AROABBS (BYTE write, BYTE value); // $0A
void __iocallconv Write0DVolumeIDLCDSegCtrl(BYTE write, BYTE value); // $0D
void __iocallconv Write20JG(BYTE write, BYTE value); // $20
void __iocallconv Write23Unknow(BYTE write, BYTE value); // $20

void CreateHotlinkMapping();
void RemoveHotlinkMapping();

#endif
