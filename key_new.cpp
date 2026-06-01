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
vector<TKeyItem*> items3000 = {
        new TKeyItem(18, 0x0,0,0, "网络", NULL, "", {SDLK_F12}),        // GND, P30
        new TKeyItem(0, 0x0, 1,0, "pda", NULL, "游戏", {SDLK_F5}), 
        new TKeyItem(0, 0x0, 2,0, "计算", NULL, "换算",{SDLK_F6}),          // P00, P30
        new TKeyItem(1, 0x0, 3,0, "时间", NULL, "系统",{SDLK_F7}),          // P01, P30
        ///new TKeyItem(2, 0x0, 4,0, "开关？？？", NULL, "汉英",{SDLK_F8}),          // P02, P30
        new TKeyItem(3, 0x0, 5,0, "英汉", NULL, "汉英",{SDLK_F9}),          // P03, P30
        new TKeyItem(4, 0x0, 6,0, "ahd", NULL, "词库",{SDLK_F10}),          // P04, P30
        new TKeyItem(5, 0x0, 7,0, "剑桥", NULL, "学习",{SDLK_F11}),        // P05, P30
       // new TKeyItem(6, 0x0, 6,1, "网络", NULL, NULL,{SDLK_F11}),        // P06, P30
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
void init_keyitems(){
    sdl_to_item.clear();
    decltype(items) current_items;
    if(!pro_key){
      current_items=items;
    }else{
      current_items=pro_mode_items;
    }
    if(nc1020mode||nc2000mode){
      for(auto x: items2000_1020) current_items.push_back(x);
      if(nc2000mode) {
        current_items.push_back(new TKeyItem(18, 0x02,0,0,  NULL, NULL, "ON/OFF", {SDLK_F12}));
        current_items.push_back(new TKeyItem(0, 0x01, 1,0, NULL, NULL, "infra_red", {SDLK_LALT})); 
      }
      if(nc1020mode){
        current_items.push_back(new TKeyItem(0, -1, 2,0, NULL, NULL, "infra_red", {SDLK_LALT})); 
      }
    }
    if(nc3000mode) {
      for(auto x: items3000) current_items.push_back(x);
    }
    if(pc1000mode){
      current_items=items1000;
    }
      for (int i=0; i<current_items.size(); i++) {
            if (current_items[i] == NULL) {
                //keypadmatrix[y][x] = 2;
            } else {
                assert(current_items[i]->code_y>=0);
                assert(current_items[i]->code_x>=0);
                for(auto e: current_items[i]->sdl_keys){
                    //sdl_to_item[e]=item[y][x]->code;
                    sdl_to_item[e]=pair<int,int>(current_items[i]->code_y, current_items[i]->code_x);
                }
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
  if(pc1000mode || nc3000mode){
    //todo not really works
    if(code_x==0 && code_y==0 && down_or_up){
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
