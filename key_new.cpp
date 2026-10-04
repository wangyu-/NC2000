#include <SDL2/SDL.h>
#include "comm.h"
#include <SDL_keyboard.h>
#include <SDL_keycode.h>
#include <map>
#include "compare/pc1000bus.h"
#include "console.h"
#include "state.h"
#include "NekoDriverIO.h"
using namespace std;
extern BusPC1000 *bus_pc1000;
extern nc2k_states_t nc2k_states;
static uint8_t * ext_reg=nc2k_states.ext_reg;

struct TKeyItem {
    TKeyItem(int ID, int keycode,int code_y, int code_x, const char* graphic, const char* subscript, const char* label,vector<int>);

    int code=0;
    int code_y=-1;
    int code_x=-1;
    vector<int> sdl_keys;
};


// ID keycode are no longer used, but they are kept for comparsion with wayback and nc1020
TKeyItem::TKeyItem( int ID, int keycode, int code_y, int code_x, const char* graphic, const char* subscript, const char* label, vector<int> sdl_keys0)
    :sdl_keys(sdl_keys0)
{
    code=keycode;
    this->code_y = code_y;
    this->code_x = code_x;
}

//the comments e.g. P00, P30 has no meaning for nc1020/2000/3000, they are copied from wayback and not changed.
vector<TKeyItem*> items2000_1020 = {
        //new TKeyItem(18, 0x02,0,0,  NULL, NULL, "ON/OFF", {SDLK_F12}),        // GND, P30
    //    new TKeyItem(0, 0x01, 1,0, NULL, NULL, "infra_red", {SDLK_BACKQUOTE}), 
        //new TKeyItem(0, 0x01, 1,0, NULL, NULL, "infra_red", {SDLK_LALT}), 
        new TKeyItem(0, 0x0B, 3,1, "英汉", NULL, "汉英",{SDLK_F5}),          // P00, P30
        new TKeyItem(1, 0x0C, 4,1, "名片", NULL, "通讯",{SDLK_F6}),          // P01, P30
        new TKeyItem(2, 0x0D, 5,1, "计算", NULL, "换算",{SDLK_F7}),          // P02, P30
        new TKeyItem(3, 0x0A, 2,1, "行程", NULL, "记事",{SDLK_F8}),          // P03, P30
        new TKeyItem(4, 0x09, 1,1, "资料", NULL, "游戏",{SDLK_F9}),          // P04, P30
        new TKeyItem(5, 0x08, 0,1, "时间", NULL, "其他",{SDLK_F10}),        // P05, P30
        new TKeyItem(6, 0x0E, 6,1, "网络", NULL, NULL,{SDLK_F11}),        // P06, P30
};
vector<TKeyItem*> items3000_special = {
    /* (AI written)
     * NC3000 front panel.  Layout taken from the official keyboard artwork
     * (info 里的 "nc3000+键盘图.JPG"):
     *
     *   ON/OFF (左上圆键，独立)    插入 删除 查找 修改 = F1..F4 (矩阵 (0..3,2))
     *   网络  (左上第二个圆键，独立)  上面那片"12 个标签"其实只有 6 个实体按键，
     *                            每个键按一次是主功能、再按一次是副功能，
     *                            主菜单是 2 行 x 6 列，正好一一对应（用户在真机上核对过）：
     *                              英汉/汉英   -> 菜单 (1,1) / (2,1)
     *                              AHD/词库    -> 菜单 (1,2) / (2,2)
     *                              剑桥/学习   -> 菜单 (1,3) / (2,3)
     *                              PDA(游戏)   -> 菜单 (1,4) / (2,4)
     *                              计算/换算   -> 菜单 (1,5) / (2,5)
     *                              时间/系统   -> 菜单 (1,6) / (2,6)
     *   右侧摇杆(一个四向键)：复读 / 跟读 / 发音暂停 / 录音
     *   机身侧面两个键：红外接收、单词精灵
     *
     * 矩阵列的含义（由 --key-probe 实测反推）：
     *   col 4 = Q W E R T Y U I      (QWERTY 排)
     *   col 5 = A S D F G H J K      (ASDF 排)
     *   col 6 = Z X C V B N M ⇞      (ZXCV 排)
     *   col 7 = 求助 中英数 输入法 跳出 符号 . 空格 ←   (底部功能排)
     *   col 0..3 = port6 的四条列线 = 上面那 6 个机型键 + 网络 + 摇杆
     *
     * 注意：col 0..3 的电学模型还没完全对上（见 docs/NC3000_按键矩阵实测.md 第 6.2 节），
     * 所以这些键的坐标是按实物布局排的，能跑通哪些要靠 --key-probe 复核。
     */
    /*
     * 矩阵第 0 列的 8 个位置 = 机身顶部那 8 个按键，两端正好对上：
     *   固件热键扫描（bank3 $4576）把 $4628 = FE FD FB F7 EF DF BF 7F 依次写进 port1、
     *   再读 port0 bit0；命中后 $4632 = 0C 04 02 00 01 06 08 0A 写进 $03E3。
     *   即"行 0..7"就是那 8 个键。
     * 与用户核对的实物：左上两个独立圆键(开/关、网络) + 6 个两档键
     * (英汉/汉英、AHD/词库、剑桥/学习、PDA/游戏、计算/换算、时间/系统)。
     */
    /*
     * ★ 行号与键位的对应（由 common.txt 的 *_FUN 常量 + 热键表 $4632
     *   （0C 04 02 00 18 06 08 0A）实测确认）：
     *     y=0 → $0C = NET_FUN   （网络，独立键）
     *     y=1 → $04 = NAMECARD/PIM_FUN（PDA，与 $05 GAME_FUN 复用）
     *     y=2 → $02 = CALCULATOR_FUN （计算/$03 CHANGE_FUN 换算）
     *     y=3 → $00 = 时间（与 $01 OTHER_FUN 系统 复用）
     *     y=4 → $18 = ONOFF_KEY （开/关，独立键，也是唤醒键）
     *     y=5 → $06 = 英汉（与 $07 HANYING_FUN 复用）
     *     y=6 → $08 = AHD_FUN （与 $09 CIKU_FUN 词库 复用）
     *     y=7 → $0A = CB_FUN/JIANQIAO（与 $0B 复用）
     * 注意：之前把"网络"和"开/关"写反了（网络在 y=4、开/关在 y=0），
     *       结果按网络键固件收到的是 ONOFF（$18）→ 直接关机黑屏。
     */
    new TKeyItem(0,  0x0, 0,0, "网络", NULL, NULL,   {SDLK_F11}),
    new TKeyItem(0,  0x0, 1,0, "PDA",  NULL, "游戏", {SDLK_F8}),
    new TKeyItem(1,  0x0, 2,0, "计算", NULL, "换算", {SDLK_F9}),
    new TKeyItem(2,  0x0, 3,0, "时间", NULL, "系统", {SDLK_F10}),
    new TKeyItem(18, 0x0, 4,0, "开/关", NULL, "ON/OFF", {SDLK_F12}), // 也是模拟器的唤醒键
    new TKeyItem(3,  0x0, 5,0, "英汉", NULL, "汉英", {SDLK_F5}),
    new TKeyItem(4,  0x0, 6,0, "AHD",  NULL, "词库", {SDLK_F6}),
    new TKeyItem(5,  0x0, 7,0, "剑桥", NULL, "学习", {SDLK_F7}),
    /*
     * 机身侧面两个按键（键盘图之外，用户 2026-09-27 指正）：
     *   左：录音        键值 0x22 = RECORD_KEY
     *   右：红外接收    键值 0x21 = IRDA_KEY
     * 右侧摇杆：复读 0x25 / 跟读 0x27 / 发音暂停 0x0F / 录音 0x22。
     *
     * 这些键**不在主键盘矩阵里**，固件是分两条路认的（见 docs/NC3000_按键矩阵实测.md 第 28 节）：
     *   1) BIOS $EC3E 的"特殊键组"扫描：往 IO $1C 写 $EAB2[X]|$40，看 bit5；
     *      X=1→$16(→) X=2→$22(录音) X=3→$25(复读) X=4→$17(←) X=5→$0F(发音暂停)
     *      ⇒ 录在了 x=3 这条"虚拟列"上（主扫根本看不到 x=3，不会串键）
     *   2) 主扫表 $EF32 里的普通位置：跟读 0x27 在 col3/rowbit2 ⇒ 矩阵 (4,2)（x=2 = port6 bit0）
     * 红外 0x21 在主扫表 $EF32 与特殊键组扫描里都找不到，是红外接收硬件那条路产生的；
     * 落位沿用 (3,3) 这个空位。**2026-09-29 用户实测这条路没问题**（上游作者 wangyu-
     * 也确认红外通信一切正常），所以不再是"待查/没建模"的状态。
     */
    new TKeyItem(0, 0x22, 0,3, "录音", NULL, "side", {SDLK_INSERT}),
    new TKeyItem(0, 0x25, 1,3, "复读", NULL, NULL, {SDLK_HOME}),
    new TKeyItem(0, 0x0F, 2,3, "发音暂停", NULL, NULL, {SDLK_PAGEUP}),
    new TKeyItem(0, 0x21, 3,3, "红外接收", NULL, "side", {SDLK_LALT}),
    new TKeyItem(0, 0x27, 4,2, "跟读", NULL, NULL, {SDLK_END}),
};
vector<TKeyItem*> items3000_col1 = {
    /* (AI written)
     * 主键盘。坐标不是猜的：用 --hold 长按 + 读固件自己解出的 $C7，
     * 逐个位置实测出来的（见 docs/NC3000_按键矩阵实测.md 第 19 节）：
     *   col 4 = Q W E R T Y U I     (0..7)   数字 1..8 是它们的第二功能
     *   col 5 = A S D F G H J K     (0..7)
     *   col 6 = Z X C V B N M ⇞     (0..7)
     *   col 7 = 求助 中英数 输入法 跳出 符号 . 空格 ←
     *   col 1 = O L ↑ ↓ P 输入 ⇟ →  (0..7)   数字 9 / 0 在 O 和 P 上
     */
    
    /*这部分会覆盖nc2000/nc1020原有的定义*/
    new TKeyItem(0,0,0,1,"O",NULL,NULL,{SDLK_o}),
    new TKeyItem(0,0,1,1,"L",NULL,NULL,{SDLK_l}),
    new TKeyItem(0,0,2,1,"↑",NULL,NULL,{SDLK_UP}),
    new TKeyItem(0,0,3,1,"↓",NULL,NULL,{SDLK_DOWN}),
    new TKeyItem(0,0,4,1,"P",NULL,NULL,{SDLK_p}),
    new TKeyItem(0,0,5,1,"输入",NULL,NULL,{SDLK_RETURN,SDLK_KP_ENTER}),
    new TKeyItem(0,0,6,1,"⇟",NULL,NULL,{SDLK_SLASH}),
    new TKeyItem(0,0,7,1,"→",NULL,NULL,{SDLK_RIGHT}),

    /*主键盘其它部分跟nc2000是一样的，不用重复定义*/
};

vector<TKeyItem*> items3000_col1_pro_mode = {
    new TKeyItem(0,0,0,1,"O",NULL,NULL,{SDLK_9}),
    new TKeyItem(0,0,1,1,"L",NULL,NULL,{SDLK_o}),
    new TKeyItem(0,0,2,1,"↑",NULL,NULL,{SDLK_UP,SDLK_l}),
    new TKeyItem(0,0,3,1,"↓",NULL,NULL,{SDLK_DOWN,SDLK_PERIOD}),
    new TKeyItem(0,0,4,1,"P",NULL,NULL,{SDLK_0}),
    new TKeyItem(0,0,5,1,"输入",NULL,NULL,{SDLK_RETURN, SDLK_p}),
    new TKeyItem(0,0,6,1,"⇟",NULL,NULL,{SDLK_SEMICOLON}),
    new TKeyItem(0,0,7,1,"→",NULL,NULL,{SDLK_RIGHT,SDLK_SLASH}),
};
vector<TKeyItem*> items = {
        NULL,       // P10, P30
        NULL,       // P11, P30
        // 不确定是0x02还是0x0f
        //new TKeyItem(18, 0x02,2,0,  NULL, NULL, "ON/OFF", {SDLK_F12}),        // GND, P30
        NULL,       // P??, P30
        NULL,       // P??, P30
        NULL,       // P??, P30
        NULL,       // P??, P30
        NULL,       // P??, P30
        
        //new TKeyItem(0, 0x0B, 3,1, "英汉", NULL, "汉英",{SDLK_F5}),          // P00, P30
        //new TKeyItem(1, 0x0C, 4,1, "名片", NULL, "通讯",{SDLK_F6}),          // P01, P30
        //new TKeyItem(2, 0x0D, 5,1, "计算", NULL, "换算",{SDLK_F7}),          // P02, P30
        //new TKeyItem(3, 0x0A, 2,1, "行程", NULL, "记事",{SDLK_F8}),          // P03, P30
        //new TKeyItem(4, 0x09, 1,1, "资料", NULL, "游戏",{SDLK_F9}),          // P04, P30
        //new TKeyItem(5, 0x08, 0,1, "时间", NULL, "其他",{SDLK_F10}),        // P05, P30
        //new TKeyItem(6, 0x0E, 6,1, "网络", NULL, NULL,{SDLK_F11}),        // P06, P30
        NULL,       // P07, P30
        
        new TKeyItem(50, 0x38, 0,7, "求助", NULL, NULL,{SDLK_LEFTBRACKET}),  // P00, P12
        new TKeyItem(51, 0x39, 1,7, "中英数", NULL, "SHIFT",{SDLK_RIGHTBRACKET}),   // P01, P12
        new TKeyItem(52, 0x3A, 2,7, "输入法", NULL, "反查 CAPS",{SDLK_BACKSLASH}), // P02, P12
        new TKeyItem(53, 0x3B, 3,7, "跳出", "AC", NULL, {SDLK_ESCAPE}),     // P03, P12
        new TKeyItem(54, 0x3C, 4,7, "符\n号", "0", "继续", {SDLK_0}),           // P04, P12
        new TKeyItem(55, 0x3D, 5,7, ".", ".", "-", {SDLK_PERIOD}),      // P05, P12
        new TKeyItem(56, 0x3E, 6,7, "空格", "=", "✓", {SDLK_EQUALS,SDLK_SPACE}),       // P06, P12
        new TKeyItem(57, 0x3F, 7,7, "←", "", NULL, {SDLK_LEFT}),     // P07, P12
        
        new TKeyItem(40, 0x30, 0,6, "Z", "(", ")",{SDLK_z}),           // P00, P13
        new TKeyItem(41, 0x31, 1,6, "X", "π", "X!",{SDLK_x}),           // P01, P13
        new TKeyItem(42, 0x32, 2,6, "C", "EXP", "。'\"",{SDLK_c}),           // P02, P13
        new TKeyItem(43, 0x33, 3,6, "V", "C",NULL,{SDLK_v}),           // P03, P13
        new TKeyItem(44, 0x34, 4,6, "B", "1",NULL,{SDLK_b,SDLK_1}),           // P04, P13
        new TKeyItem(45, 0x35, 5,6, "N", "2",NULL,{SDLK_n,SDLK_2}),           // P05, P13
        new TKeyItem(46, 0x36, 6,6, "M", "3",NULL,{SDLK_m,SDLK_3}),           // P06, P13
        new TKeyItem(47, 0x37, 7,6, "⇞", "税",NULL,{SDLK_COMMA}),   // P07, P13
        
        new TKeyItem(30, 0x28, 0,5, "A", "log", "10x",{SDLK_a}),       // P00, P14
        new TKeyItem(31, 0x29, 1,5, "S", "ln", "ex",{SDLK_s}),       // P01, P14
        new TKeyItem(32, 0x2A, 2,5, "D", "Xʸ", "y√x",{SDLK_d}),       // P02, P14
        new TKeyItem(33, 0x2B, 3,5, "F", "√", "X\u00B2",{SDLK_f}),       // P03, P14
        new TKeyItem(34, 0x2C, 4,5, "G", "4",NULL,{SDLK_g,SDLK_4}),       // P04, P14
        new TKeyItem(35, 0x2D, 5,5, "H", "5",NULL,{SDLK_h,SDLK_5}),       // P05, P14
        new TKeyItem(36, 0x2E, 6,5, "J", "6",NULL,{SDLK_j,SDLK_6}),       // P06, P14
        new TKeyItem(37, 0x2F, 7,5, "K", "±",NULL,{SDLK_k}),       // P07, P14
        
        new TKeyItem(20, 0x20, 0,4, "Q", "sin", "sin-1",{SDLK_q}),       // P00, P15
        new TKeyItem(21, 0x21, 1,4, "W", "cos", "cos-1",{SDLK_w}),       // P01, P15
        new TKeyItem(22, 0x22, 2,4, "E", "tan", "tan-1",{SDLK_e}),       // P02, P15
        new TKeyItem(23, 0x23, 3,4, "R", "1/X", "hyp",{SDLK_r}),       // P03, P15
        new TKeyItem(24, 0x24, 4,4, "T", "7",NULL,{SDLK_t,SDLK_7}),       // P04, P15
        new TKeyItem(25, 0x25, 5,4, "Y", "8",NULL,{SDLK_y,SDLK_8}),       // P05, P15
        new TKeyItem(26, 0x26, 6,4, "U", "9",NULL,{SDLK_u,SDLK_9}),       // P06, P15
        new TKeyItem(27, 0x27, 7,4, "I", "%",NULL,{SDLK_i}),       // P07, P15
        
        new TKeyItem(28, 0x18, 0,3, "O", "÷", "#",{SDLK_o}),           // P00, P16
        new TKeyItem(38, 0x19, 1,3, "L", "x", "*",{SDLK_l}),           // P01, P16
        new TKeyItem(48, 0x1A, 2,3, "▲", "-",NULL,{SDLK_UP}),         // P02, P16
        new TKeyItem(58, 0x1B, 3,3, "▼", "+",NULL,{SDLK_DOWN}),     // P03, P16
        new TKeyItem(29, 0x1C, 4,3, "P", "MC", "☎",{SDLK_p}),           // P04, P16
        new TKeyItem(39, 0x1D, 5,3, "输入", "MR",NULL,{SDLK_RETURN}),   // P05, P16
        new TKeyItem(49, 0x1E, 6,3, "⇟", "M-",NULL,{SDLK_SLASH}), // P06, P16
        new TKeyItem(59, 0x1F, 7,3, "→", "M+",NULL,{SDLK_RIGHT}),   // P07, P16
        
        NULL,       // P00, P17
        NULL,       // P01, P17
        new TKeyItem(12, 0x10, 0,2, "F1", NULL, "插入",{SDLK_F1}),       // P02, P17
        new TKeyItem(13, 0x11, 1,2, "F2", NULL, "删除",{SDLK_F2,SDLK_BACKSPACE}),       // P03, P17
        new TKeyItem(14, 0x12, 2,2, "F3", NULL, "查找",{SDLK_F3}),       // P04, P17
        new TKeyItem(15, 0x13, 3,2, "F4", NULL, "修改",{SDLK_F4}),       // P05, P17
        NULL,       // P06, P17
        NULL,       // P07, P17
        //newly added
        new TKeyItem(0, 0x14, 4,2, "报时", NULL, "xx",{SDLK_QUOTE}),
        new TKeyItem(0, 0x15, 5,2,"发音", NULL, "xx",{SDLK_SEMICOLON}),
        //new TKeyItem(0, 0x01, 1,0, NULL, NULL, "xx", {SDLK_BACKQUOTE}), 
    };

vector<TKeyItem*> pro_mode_items = {
        new TKeyItem(50, 0x38, 0,7, "求助", NULL, NULL,{SDLK_z}),  // P00, P12
        new TKeyItem(51, 0x39, 1,7, "中英数", NULL, "SHIFT",{SDLK_x}),   // P01, P12
        new TKeyItem(52, 0x3A, 2,7, "输入法", NULL, "反查 CAPS",{SDLK_c}), // P02, P12
        new TKeyItem(53, 0x3B, 3,7, "跳出", "AC", NULL, {SDLK_ESCAPE,SDLK_v}),     // P03, P12
        new TKeyItem(54, 0x3C, 4,7, "符\n号", "0", "继续", {SDLK_b}),           // P04, P12
        new TKeyItem(55, 0x3D, 5,7, ".", ".", "-", {SDLK_n}),      // P05, P12
        new TKeyItem(56, 0x3E, 6,7, "空格", "=", "✓", {SDLK_SPACE,SDLK_m}),       // P06, P12
        new TKeyItem(57, 0x3F, 7,7, "←", "", NULL, {SDLK_LEFT,SDLK_COMMA}),     // P07, P12
        
        new TKeyItem(40, 0x30, 0,6, "Z", "(", ")",{SDLK_a}),           // P00, P13
        new TKeyItem(41, 0x31, 1,6, "X", "π", "X!",{SDLK_s}),           // P01, P13
        new TKeyItem(42, 0x32, 2,6, "C", "EXP", "。'\"",{SDLK_d}),           // P02, P13
        new TKeyItem(43, 0x33, 3,6, "V", "C",NULL,{SDLK_f}),           // P03, P13
        new TKeyItem(44, 0x34, 4,6, "B", "1",NULL,{SDLK_g}),           // P04, P13
        new TKeyItem(45, 0x35, 5,6, "N", "2",NULL,{SDLK_h}),           // P05, P13
        new TKeyItem(46, 0x36, 6,6, "M", "3",NULL,{SDLK_j}),           // P06, P13
        new TKeyItem(47, 0x37, 7,6, "⇞", "税",NULL,{SDLK_k}),   // P07, P13
        
        new TKeyItem(30, 0x28, 0,5, "A", "log", "10x",{SDLK_q}),       // P00, P14
        new TKeyItem(31, 0x29, 1,5, "S", "ln", "ex",{SDLK_w}),       // P01, P14
        new TKeyItem(32, 0x2A, 2,5, "D", "Xʸ", "y√x",{SDLK_e}),       // P02, P14
        new TKeyItem(33, 0x2B, 3,5, "F", "√", "X\u00B2",{SDLK_r}),       // P03, P14
        new TKeyItem(34, 0x2C, 4,5, "G", "4",NULL,{SDLK_t}),       // P04, P14
        new TKeyItem(35, 0x2D, 5,5, "H", "5",NULL,{SDLK_y}),       // P05, P14
        new TKeyItem(36, 0x2E, 6,5, "J", "6",NULL,{SDLK_u}),       // P06, P14
        new TKeyItem(37, 0x2F, 7,5, "K", "±",NULL,{SDLK_i}),       // P07, P14
        
        new TKeyItem(20, 0x20, 0,4, "Q", "sin", "sin-1",{SDLK_1}),       // P00, P15
        new TKeyItem(21, 0x21, 1,4, "W", "cos", "cos-1",{SDLK_2}),       // P01, P15
        new TKeyItem(22, 0x22, 2,4, "E", "tan", "tan-1",{SDLK_3}),       // P02, P15
        new TKeyItem(23, 0x23, 3,4, "R", "1/X", "hyp",{SDLK_4}),       // P03, P15
        new TKeyItem(24, 0x24, 4,4, "T", "7",NULL,{SDLK_5}),       // P04, P15
        new TKeyItem(25, 0x25, 5,4, "Y", "8",NULL,{SDLK_6}),       // P05, P15
        new TKeyItem(26, 0x26, 6,4, "U", "9",NULL,{SDLK_7}),       // P06, P15
        new TKeyItem(27, 0x27, 7,4, "I", "%",NULL,{SDLK_8}),       // P07, P15
        
        new TKeyItem(28, 0x18, 0,3, "O", "÷", "#",{SDLK_9}),           // P00, P16
        new TKeyItem(38, 0x19, 1,3, "L", "x", "*",{SDLK_o}),           // P01, P16
        new TKeyItem(48, 0x1A, 2,3, "▲", "-",NULL,{SDLK_UP,SDLK_l}),         // P02, P16
        new TKeyItem(58, 0x1B, 3,3, "▼", "+",NULL,{SDLK_DOWN,SDLK_PERIOD}),     // P03, P16
        new TKeyItem(29, 0x1C, 4,3, "P", "MC", "☎",{SDLK_0}),           // P04, P16
        new TKeyItem(39, 0x1D, 5,3, "输入", "MR",NULL,{SDLK_RETURN, SDLK_p}),   // P05, P16
        new TKeyItem(49, 0x1E, 6,3, "⇟", "M-",NULL,{SDLK_SEMICOLON}), // P06, P16
        new TKeyItem(59, 0x1F, 7,3, "→", "M+",NULL,{SDLK_RIGHT,SDLK_SLASH}),   // P07, P16
        
        NULL,       // P00, P17
        NULL,       // P01, P17
        new TKeyItem(12, 0x10, 0,2, "F1", NULL, "插入",{SDLK_F1}),       // P02, P17
        new TKeyItem(13, 0x11, 1,2, "F2", NULL, "删除",{SDLK_F2,SDLK_BACKSPACE}),       // P03, P17
        new TKeyItem(14, 0x12, 2,2, "F3", NULL, "查找",{SDLK_F3}),       // P04, P17
        new TKeyItem(15, 0x13, 3,2, "F4", NULL, "修改",{SDLK_F4}),       // P05, P17
        NULL,       // P06, P17
        NULL,       // P07, P17
        //newly added
        new TKeyItem(0, 0x14, 4,2, "报时", NULL, "xx",{ SDLK_EQUALS}),
        new TKeyItem(0, 0x15, 5,2,"发音", NULL, "xx",{ SDLK_MINUS }),
        //new TKeyItem(0, 0x01, 1,0, NULL, NULL, "xx", {SDLK_BACKQUOTE}), 
    };

vector<TKeyItem*> items1000 = {
    NULL,       // P10, P30
    NULL,       // P11, P30
    new TKeyItem(18, 0, 0,2, NULL, NULL, "ON/OFF", {SDLK_F12}),        // GND, P30
    NULL,       // P??, P30
    NULL,       // P??, P30
    NULL,       // P??, P30
    NULL,       // P??, P30
    NULL,       // P??, P30
    
    new TKeyItem(0, 0, 1,0, "英汉", NULL, "汉英",{SDLK_F5}),          // P00, P30
    new TKeyItem(1, 0, 1,1, "名片", NULL, "通讯",{SDLK_F6}),          // P01, P30
    new TKeyItem(2, 0, 1,2, "计算", NULL, "换算",{SDLK_F7}),          // P02, P30
    new TKeyItem(3, 0, 1,3, "行程", NULL, "记事",{SDLK_F8}),          // P03, P30
    new TKeyItem(4, 0, 1,4, "资料", NULL, "游戏",{SDLK_F9}),          // P04, P30
    new TKeyItem(5, 0, 1,5, "时间", NULL, "其他",{SDLK_F10}),        // P05, P30
    new TKeyItem(6, 0, 1,6, "网络", NULL, NULL,{SDLK_F11}),        // P06, P30
    NULL,       // P07, P30
    
    new TKeyItem(50, 0, 2,0, "求助", NULL, NULL,{SDLK_LEFTBRACKET}),  // P00, P12
    new TKeyItem(51, 0, 2,1, "中英数", NULL, "SHIFT",{SDLK_RIGHTBRACKET}),   // P01, P12
    new TKeyItem(52, 0, 2,2, "输入法", NULL, "反查 CAPS",{SDLK_BACKSLASH}), // P02, P12
    new TKeyItem(53, 0, 2,3, "跳出", "AC", NULL, {SDLK_ESCAPE}),     // P03, P12
    new TKeyItem(54, 0, 2,4, "符\n号", "0", "继续", {SDLK_0}),           // P04, P12
    new TKeyItem(55, 0, 2,5, ".", ".", "-", {SDLK_PERIOD}),      // P05, P12
    new TKeyItem(56, 0, 2,6, "空格", "=", "✓", {SDLK_EQUALS,SDLK_SPACE}),       // P06, P12
    new TKeyItem(57, 0, 2,7, "←", "", NULL, {SDLK_LEFT}),     // P07, P12
    
    new TKeyItem(40, 0, 3,0, "Z", "(", ")",{SDLK_z}),           // P00, P13
    new TKeyItem(41, 0, 3,1, "X", "π", "X!",{SDLK_x}),           // P01, P13
    new TKeyItem(42, 0, 3,2, "C", "EXP", "。'\"",{SDLK_c}),           // P02, P13
    new TKeyItem(43, 0, 3,3, "V", "C",NULL,{SDLK_v}),           // P03, P13
    new TKeyItem(44, 0, 3,4, "B", "1",NULL,{SDLK_b,SDLK_1}),           // P04, P13
    new TKeyItem(45, 0, 3,5, "N", "2",NULL,{SDLK_n,SDLK_2}),           // P05, P13
    new TKeyItem(46, 0, 3,6, "M", "3",NULL,{SDLK_m,SDLK_3}),           // P06, P13
    new TKeyItem(47, 0, 3,7, "⇞", "税",NULL,{SDLK_COMMA}),   // P07, P13
    
    new TKeyItem(30, 0, 4,0, "A", "log", "10x",{SDLK_a}),       // P00, P14
    new TKeyItem(31, 0, 4,1, "S", "ln", "ex",{SDLK_s}),       // P01, P14
    new TKeyItem(32, 0, 4,2, "D", "Xʸ", "y√x",{SDLK_d}),       // P02, P14
    new TKeyItem(33, 0, 4,3, "F", "√", "X\u00B2",{SDLK_f}),       // P03, P14
    new TKeyItem(34, 0, 4,4, "G", "4",NULL,{SDLK_g,SDLK_4}),       // P04, P14
    new TKeyItem(35, 0, 4,5, "H", "5",NULL,{SDLK_h,SDLK_5}),       // P05, P14
    new TKeyItem(36, 0, 4,6, "J", "6",NULL,{SDLK_j,SDLK_6}),       // P06, P14
    new TKeyItem(37, 0, 4,7, "K", "±",NULL,{SDLK_k}),       // P07, P14
    
    new TKeyItem(20, 0, 5,0, "Q", "sin", "sin-1",{SDLK_q}),       // P00, P15
    new TKeyItem(21, 0, 5,1, "W", "cos", "cos-1",{SDLK_w}),       // P01, P15
    new TKeyItem(22, 0, 5,2, "E", "tan", "tan-1",{SDLK_e}),       // P02, P15
    new TKeyItem(23, 0, 5,3, "R", "1/X", "hyp",{SDLK_r}),       // P03, P15
    new TKeyItem(24, 0, 5,4, "T", "7",NULL,{SDLK_t,SDLK_7}),       // P04, P15
    new TKeyItem(25, 0, 5,5, "Y", "8",NULL,{SDLK_y,SDLK_8}),       // P05, P15
    new TKeyItem(26, 0, 5,6, "U", "9",NULL,{SDLK_u,SDLK_9}),       // P06, P15
    new TKeyItem(27, 0, 5,7, "I", "%",NULL,{SDLK_i}),       // P07, P15
    
    new TKeyItem(28, 0, 6,0, "O", "÷", "#",{SDLK_o}),           // P00, P16
    new TKeyItem(38, 0, 6,1, "L", "x", "*",{SDLK_l}),           // P01, P16
    new TKeyItem(48, 0, 6,2, "▲", "-",NULL,{SDLK_UP}),         // P02, P16
    new TKeyItem(58, 0, 6,3, "▼", "+",NULL,{SDLK_DOWN}),     // P03, P16
    new TKeyItem(29, 0, 6,4, "P", "MC", "☎",{SDLK_p}),           // P04, P16
    new TKeyItem(39, 0, 6,5, "输入", "MR",NULL,{SDLK_RETURN}),   // P05, P16
    new TKeyItem(49, 0, 6,6, "⇟", "M-",NULL,{SDLK_SLASH}), // P06, P16
    new TKeyItem(59, 0, 6,7, "→", "M+",NULL,{SDLK_RIGHT}),   // P07, P16
    
    NULL,       // P00, P17
    NULL,       // P01, P17
    new TKeyItem(12, 0, 7,2, "F1", NULL, "插入",{SDLK_F1}),       // P02, P17
    new TKeyItem(13, 0, 7,3, "F2", NULL, "删除",{SDLK_F2,SDLK_BACKSPACE}),       // P03, P17
    new TKeyItem(14, 0, 7,4, "F3", NULL, "查找",{SDLK_F3}),       // P04, P17
    new TKeyItem(15, 0, 7,5, "F4", NULL, "修改",{SDLK_F4}),       // P05, P17
    NULL,       // P06, P17
    NULL,       // P07, P17

    //newly add
    new TKeyItem(0, 0, 7,0,"xx", NULL, "xx",{SDLK_SEMICOLON}),
    new TKeyItem(0, 0, 7,1,"xx", NULL, "xx",{SDLK_QUOTE}),
};


static map<int,pair<int,int> > sdl_to_item;
void copy_items_deref(vector<TKeyItem*> &src, vector<TKeyItem> &dst){
  for(auto x: src) if(x) dst.push_back(*x);
}
void init_keyitems(){
  sdl_to_item.clear();
  vector<TKeyItem> current_items;
  if(nc2000mode || nc1020mode || nc3000mode){ // their keyboard scans are similar
    if(!pro_key){
      copy_items_deref(items, current_items);
    }else{
      copy_items_deref(pro_mode_items, current_items);
    }
    if(nc1020mode||nc2000mode){
      copy_items_deref(items2000_1020, current_items);
      if(nc2000mode) {
        current_items.push_back(TKeyItem(18, 0x02,0,0,  NULL, NULL, "ON/OFF", {SDLK_F12}));
        current_items.push_back(TKeyItem(0, 0x01, 1,0, NULL, NULL, "infra_red", {SDLK_LALT})); 
      }
      if(nc1020mode){
        current_items.push_back(TKeyItem(0, -1, 2,0, NULL, NULL, "infra_red", {SDLK_LALT})); 
      }
    }
    if(nc3000mode) {
      copy_items_deref(items3000_special, current_items);
      if(!pro_key){
          copy_items_deref(items3000_col1, current_items);
      }else {
          copy_items_deref(items3000_col1_pro_mode, current_items);
      }
    }
  }

  if(pc1000mode){ //pc1000's keyboard scan shares no common part with nc2000/nc1020/nc3000
    if(pro_key){
      printf("WARN: pro_key is not supported for pc1000 mode\n");
    }
    copy_items_deref(items1000, current_items);
  }

  for (int i=0; i<current_items.size(); i++) {
      assert(current_items[i].code_y>=0);
      assert(current_items[i].code_x>=0);
      for(auto e: current_items[i].sdl_keys){
          sdl_to_item[e]=pair<int,int>(current_items[i].code_y, current_items[i].code_x);
      }
  }
}

pair<int,int> map_key_wayback(int32_t sym){
  if(sdl_to_item.find(sym)!=sdl_to_item.end()){
    return sdl_to_item[sym];
  }
  return pair<int,int>(-1,-1);
}

void SetKeyWayback(int code_y,int code_x, bool down_or_up){
  if(pc1000mode){
    //todo not really works
    if(code_x==0 && code_y==0 && down_or_up){
      void warm_reset_if_clkoff();
      warm_reset_if_clkoff();
    }
  }
  if(nc3000mode){
      if(code_x==0 && code_y==4 && down_or_up){
          void warm_reset_if_clkoff();
          warm_reset_if_clkoff();
      }
  }
  if(nc2000mode||nc1020mode){
      if(code_x<2&& down_or_up){
        void warm_reset_if_clkoff();
        warm_reset_if_clkoff();
      }
  }

    if (code_y < 8 && code_x < 8) {
        keypadmatrix[code_y][code_x] = down_or_up;
    }

}
void handle_key_wayback(signed int sym, bool key_down){
        if(debug_level>=2){
          printf("key %d %s\n", sym, key_down ? "down" : "up");
        }
        if(sym==SDLK_F12 && shift_down&& ctrl_down){ // shift+ctrl+F12 triggers reset button
            if(key_down==1){
              void cold_reset();
              cold_reset();
            }
            return;
        }
        /*if(enable_debug_key_shoot){
          printf("event <%d,%d; %llu>\n", sym,key_down,(u64_t)SDL_GetTicks64()%1000);
        }*/
        auto value=map_key_wayback(sym);
        if(nc1020mode && sym==SDLK_F12 ){
          //nc1020's on/off is not on the 8x8 keyboard scanning matrix, it is an independent pin
          uint8_t* ram_io=nc2k_states.ram_io;
          if(key_down){
            ram_io[0x0b]&=~1;
            void warm_reset_if_clkoff();
            warm_reset_if_clkoff();
          }else {
            ram_io[0x0b]|=1;
          }
          if(debug_level>=2) printf("current value of 0x0b bit0: %d\n", ram_io[0x0b]&0x01);

        }
        if(value.first!=-1 && value.second!=-1){
          SetKeyWayback(value.first,value.second, key_down); //set up the 8x8 key scan matrix
          if(bus_pc1000){ //for compatibility with pc1000emux bus
            if(key_down){
              bus_pc1000->keyDown2(value.first, value.second);
            }else{
              bus_pc1000->keyUp();
            }
          }
        }
        if(log_on_key_press==1 &&sym != SDLK_F11 || (log_on_key_press >1 && sym== log_on_key_press)){
          // --log-on-key-press, enable logging for debug when interested key is pressed
          if(key_down && shift_down){
            enable_dyn_debug_next_n=100*1000000;
          }
        }
        switch ( sym) {
          case SDLK_BACKQUOTE:    //handles the "pro-key" mode
            if(shift_down){
              if(key_down==1){
                pro_key^= 0x1;
                printf("pro_key %s\n", pro_key ? "on" : "off");
                init_keyitems();
                extern SDL_Window* window;
                SDL_SetWindowTitle(window, get_title().c_str());
                //enable_dyn_debug^= 0x1;
              }
            }
            break;

          case SDLK_TAB:       //handles fast forward toggle
            if(key_down==1){
                fast_forward^= 0x1;
                printf("fast_forward %s\n", fast_forward ? "on" : "off");
                extern SDL_Window* window;
                SDL_SetWindowTitle(window, get_title().c_str());
            }
            break;

          default :  // unsupported
            break;
        }
}
