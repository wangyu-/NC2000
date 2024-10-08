#include <SDL2/SDL.h>
#include "comm.h"
#include <map>
using namespace std;

struct TKeyItem {
    //TKeyItem(int ID, const char* graphic, const char* subscript);
    TKeyItem(int ID, int keycode, const char* graphic, const char* subscript, const char* label,vector<int>);

    //int fRow;
    //int fColumn;
    //const char* fGraphic; // TODO:
    //const char* fSubscript;
    //const char* fSuperLabel; // label on top
    int code=0;
    vector<int> sdl_keys;
};

TKeyItem::TKeyItem( int ID, int keycode, const char* graphic, const char* subscript, const char* label, vector<int> sdl_keys0)
    :// fRow(ID / 10)
    //, fColumn(ID % 10)
    //fGraphic(graphic)
    //, fSubscript(subscript)
    //, fSuperLabel(label)
    sdl_keys(sdl_keys0)
{
    code=keycode;
}

extern unsigned /*char*/ keypadmatrix[8][8];

TKeyItem* item[20][8] = {
        NULL,       // P10, P30
        NULL,       // P11, P30
        // 不确定是0x02还是0x0f
        new TKeyItem(18, 0x02,  NULL, NULL, "ON/OFF", {SDLK_F12}),        // GND, P30
        NULL,       // P??, P30
        NULL,       // P??, P30
        NULL,       // P??, P30
        NULL,       // P??, P30
        NULL,       // P??, P30
        
        new TKeyItem(0, 0x0B, "英汉", NULL, "汉英",{SDLK_F5}),          // P00, P30
        new TKeyItem(1, 0x0C, "名片", NULL, "通讯",{SDLK_F6}),          // P01, P30
        new TKeyItem(2, 0x0D, "计算", NULL, "换算",{SDLK_F7}),          // P02, P30
        new TKeyItem(3, 0x0A, "行程", NULL, "记事",{SDLK_F8}),          // P03, P30
        new TKeyItem(4, 0x09, "资料", NULL, "游戏",{SDLK_F9}),          // P04, P30
        new TKeyItem(5, 0x08, "时间", NULL, "其他",{SDLK_F10}),        // P05, P30
        new TKeyItem(6, 0x0E, "网络", NULL, NULL,{SDLK_F11}),        // P06, P30
        NULL,       // P07, P30
        
        new TKeyItem(50, 0x38, "求助", NULL, NULL,{SDLK_LEFTBRACKET}),  // P00, P12
        new TKeyItem(51, 0x39, "中英数", NULL, "SHIFT",{SDLK_RIGHTBRACKET}),   // P01, P12
        new TKeyItem(52, 0x3A, "输入法", NULL, "反查 CAPS",{SDLK_BACKSLASH}), // P02, P12
        new TKeyItem(53, 0x3B, "跳出", "AC", NULL, {SDLK_ESCAPE}),     // P03, P12
        new TKeyItem(54, 0x3C, "符\n号", "0", "继续", {SDLK_0}),           // P04, P12
        new TKeyItem(55, 0x3D, ".", ".", "-", {SDLK_PERIOD}),      // P05, P12
        new TKeyItem(56, 0x3E, "空格", "=", "✓", {SDLK_EQUALS,SDLK_SPACE}),       // P06, P12
        new TKeyItem(57, 0x3F, "←", "", NULL, {SDLK_LEFT,SDLK_BACKSPACE}),     // P07, P12
        
        new TKeyItem(40, 0x30, "Z", "(", ")",{SDLK_z}),           // P00, P13
        new TKeyItem(41, 0x31, "X", "π", "X!",{SDLK_x}),           // P01, P13
        new TKeyItem(42, 0x32, "C", "EXP", "。'\"",{SDLK_c}),           // P02, P13
        new TKeyItem(43, 0x33, "V", "C",NULL,{SDLK_v}),           // P03, P13
        new TKeyItem(44, 0x34, "B", "1",NULL,{SDLK_b,SDLK_1}),           // P04, P13
        new TKeyItem(45, 0x35, "N", "2",NULL,{SDLK_n,SDLK_2}),           // P05, P13
        new TKeyItem(46, 0x36, "M", "3",NULL,{SDLK_m,SDLK_3}),           // P06, P13
        new TKeyItem(47, 0x37, "⇞", "税",NULL,{SDLK_COMMA}),   // P07, P13
        
        new TKeyItem(30, 0x28, "A", "log", "10x",{SDLK_a}),       // P00, P14
        new TKeyItem(31, 0x29, "S", "ln", "ex",{SDLK_s}),       // P01, P14
        new TKeyItem(32, 0x2A, "D", "Xʸ", "y√x",{SDLK_d}),       // P02, P14
        new TKeyItem(33, 0x2B, "F", "√", "X\u00B2",{SDLK_f}),       // P03, P14
        new TKeyItem(34, 0x2C, "G", "4",NULL,{SDLK_g,SDLK_4}),       // P04, P14
        new TKeyItem(35, 0x2D, "H", "5",NULL,{SDLK_h,SDLK_5}),       // P05, P14
        new TKeyItem(36, 0x2E, "J", "6",NULL,{SDLK_j,SDLK_6}),       // P06, P14
        new TKeyItem(37, 0x2F, "K", "±",NULL,{SDLK_k}),       // P07, P14
        
        new TKeyItem(20, 0x20, "Q", "sin", "sin-1",{SDLK_q}),       // P00, P15
        new TKeyItem(21, 0x21, "W", "cos", "cos-1",{SDLK_w}),       // P01, P15
        new TKeyItem(22, 0x22, "E", "tan", "tan-1",{SDLK_e}),       // P02, P15
        new TKeyItem(23, 0x23, "R", "1/X", "hyp",{SDLK_r}),       // P03, P15
        new TKeyItem(24, 0x24, "T", "7",NULL,{SDLK_t,SDLK_7}),       // P04, P15
        new TKeyItem(25, 0x25, "Y", "8",NULL,{SDLK_y,SDLK_8}),       // P05, P15
        new TKeyItem(26, 0x26, "U", "9",NULL,{SDLK_u,SDLK_9}),       // P06, P15
        new TKeyItem(27, 0x27, "I", "%",NULL,{SDLK_i}),       // P07, P15
        
        new TKeyItem(28, 0x18, "O", "÷", "#",{SDLK_o}),           // P00, P16
        new TKeyItem(38, 0x19, "L", "x", "*",{SDLK_l}),           // P01, P16
        new TKeyItem(48, 0x1A, "▲", "-",NULL,{SDLK_UP}),         // P02, P16
        new TKeyItem(58, 0x1B, "▼", "+",NULL,{SDLK_DOWN}),     // P03, P16
        new TKeyItem(29, 0x1C, "P", "MC", "☎",{SDLK_p}),           // P04, P16
        new TKeyItem(39, 0x1D, "输入", "MR",NULL,{SDLK_RETURN}),   // P05, P16
        new TKeyItem(49, 0x1E, "⇟", "M-",NULL,{SDLK_SLASH}), // P06, P16
        new TKeyItem(59, 0x1F, "→", "M+",NULL,{SDLK_RIGHT}),   // P07, P16
        
        NULL,       // P00, P17
        NULL,       // P01, P17
        new TKeyItem(12, 0x10, "F1", NULL, "插入",{SDLK_F1}),       // P02, P17
        new TKeyItem(13, 0x11, "F2", NULL, "删除",{SDLK_F2}),       // P03, P17
        new TKeyItem(14, 0x12, "F3", NULL, "查找",{SDLK_F3}),       // P04, P17
        new TKeyItem(15, 0x13, "F4", NULL, "修改",{SDLK_F4}),       // P05, P17
        NULL,       // P06, P17
        NULL,       // P07, P17
        //newly added
        new TKeyItem(0, 0x14, "xx", NULL, "xx",{SDLK_QUOTE}),
        new TKeyItem(0, 0x15, "xx", NULL, "xx",{SDLK_SEMICOLON}),
        new TKeyItem(0, 0x01, NULL, NULL, "xx", {SDLK_BACKQUOTE}), 
    };


static map<int,uint> sdl_to_item;
void init_keyitems(){
      for (int y = 0; y < 20; y++) {
        for (int x = 0; x < 8; x++) {
            //fKeyItems[y][x] = item[y][x];
            if (item[y][x] == NULL) {
                //keypadmatrix[y][x] = 2;
            } else {
                //int row = item[y][x]->fRow;
                //int col = item[y][x]->fColumn;
                //int index = row * 10 + col;
                //MyRectButton* button = [MyRectButton buttonWithType:UIButtonTypeRoundedRect];
                //button.contentScaleFactor = 1;
                //button.layer.contentsScale = 1;
                //item[y][x]->tag = y * 0x10 + x;
                for(auto e: item[y][x]->sdl_keys){
                    sdl_to_item[e]=item[y][x]->code;
                }
            }
        }
      }
}

uint8_t map_key_wayback(int32_t sym){
  if(sdl_to_item.find(sym)!=sdl_to_item.end()){
    return sdl_to_item[sym];
  }
  return 0;
}

void SetKeyWayback(uint8_t key_id, bool down_or_up){
    /*
    unsigned int y = key_id / 16;
    unsigned int x = key_id % 16;
    if (y < 8 && x < 8) {
        keypadmatrix[y][x] = down_or_up;
    }*/

    // below make it compatible with nc1020's code
    unsigned int y = key_id % 8;
    unsigned int x = key_id / 8;
    if (y < 8 && x < 8) {
        keypadmatrix[y][x] = down_or_up;
    }

}

void handle_key_wayback(signed int sym, bool key_down){
        /*if(enable_debug_key_shoot){
          printf("event <%d,%d; %llu>\n", sym,key_down,SDL_GetTicks64()%1000);
        }*/
        uint8_t value=map_key_wayback(sym);
        if(value!=0){
           SetKeyWayback(value, key_down);
        }
        switch ( sym) {
          case SDLK_BACKQUOTE:
            if(key_down==1){
              //enable_dyn_debug^= 0x1;
            }
            break;

          case SDLK_TAB:
            if(key_down==1){
                //fast_forward^= 0x1;
            }
            break;

          default :  // unsupported
            break;
        }
}
