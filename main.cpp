#include <SDL2/SDL.h>
#include "comm.h"
#include "nc2000.h"
#include <SDL_keycode.h>
#include <iostream>
#include <map>
#include "sound.h"
#include "udp_server.h"
using namespace std;

static int enable_debug_key_shoot=false;
static bool fast_forward=false;

static SDL_Renderer* renderer;
static uint8_t lcd_buf[SCREEN_WIDTH * SCREEN_HEIGHT / 8*2];

struct TKeyItem {
    //TKeyItem(int ID, const char* graphic, const char* subscript);
    TKeyItem(int ID, const char* graphic, const char* subscript, const char* label,vector<int>);

    int fRow;
    int fColumn;
    const char* fGraphic; // TODO:
    const char* fSubscript;
    const char* fSuperLabel; // label on top
    int tag=0;
    vector<int> sdl_keys;
};

TKeyItem::TKeyItem( int ID, const char* graphic, const char* subscript, const char* label, vector<int> sdl_keys0)
    : fRow(ID / 10)
    , fColumn(ID % 10)
    , fGraphic(graphic)
    , fSubscript(subscript)
    , fSuperLabel(label)
    , sdl_keys(sdl_keys0)
{
}

extern unsigned /*char*/ keypadmatrix[8][8];

TKeyItem* item[8][8] = {
        NULL,       // P10, P30
        NULL,       // P11, P30
        new TKeyItem(18, NULL, NULL, "ON/OFF", {SDLK_F12}),        // GND, P30
        NULL,       // P??, P30
        NULL,       // P??, P30
        NULL,       // P??, P30
        NULL,       // P??, P30
        NULL,       // P??, P30
        
        new TKeyItem(0, "英汉", NULL, "汉英",{SDLK_F5}),          // P00, P30
        new TKeyItem(1, "名片", NULL, "通讯",{SDLK_F6}),          // P01, P30
        new TKeyItem(2, "计算", NULL, "换算",{SDLK_F7}),          // P02, P30
        new TKeyItem(3, "行程", NULL, "记事",{SDLK_F8}),          // P03, P30
        new TKeyItem(4, "资料", NULL, "游戏",{SDLK_F9}),          // P04, P30
        new TKeyItem(5, "时间", NULL, "其他",{SDLK_F10}),        // P05, P30
        new TKeyItem(6, "网络", NULL, NULL,{SDLK_F11}),        // P06, P30
        NULL,       // P07, P30
        
        new TKeyItem(50, "求助", NULL, NULL,{SDLK_LEFTBRACKET}),  // P00, P12
        new TKeyItem(51, "中英数", NULL, "SHIFT",{SDLK_RIGHTBRACKET}),   // P01, P12
        new TKeyItem(52, "输入法", NULL, "反查 CAPS",{SDLK_BACKSLASH}), // P02, P12
        new TKeyItem(53, "跳出", "AC", NULL, {SDLK_ESCAPE}),     // P03, P12
        new TKeyItem(54, "符\n号", "0", "继续", {SDLK_0}),           // P04, P12
        new TKeyItem(55, ".", ".", "-", {SDLK_PERIOD}),      // P05, P12
        new TKeyItem(56, "空格", "=", "✓", {SDLK_EQUALS}),       // P06, P12
        new TKeyItem(57, "←", "", NULL, {SDLK_LEFT,SDLK_BACKSPACE}),     // P07, P12
        
        new TKeyItem(40, "Z", "(", ")",{SDLK_z}),           // P00, P13
        new TKeyItem(41, "X", "π", "X!",{SDLK_x}),           // P01, P13
        new TKeyItem(42, "C", "EXP", "。'\"",{SDLK_c}),           // P02, P13
        new TKeyItem(43, "V", "C",NULL,{SDLK_v}),           // P03, P13
        new TKeyItem(44, "B", "1",NULL,{SDLK_b,SDLK_1}),           // P04, P13
        new TKeyItem(45, "N", "2",NULL,{SDLK_n,SDLK_2}),           // P05, P13
        new TKeyItem(46, "M", "3",NULL,{SDLK_m,SDLK_3}),           // P06, P13
        new TKeyItem(47, "⇞", "税",NULL,{SDLK_COMMA}),   // P07, P13
        
        new TKeyItem(30, "A", "log", "10x",{SDLK_a}),       // P00, P14
        new TKeyItem(31, "S", "ln", "ex",{SDLK_s}),       // P01, P14
        new TKeyItem(32, "D", "Xʸ", "y√x",{SDLK_d}),       // P02, P14
        new TKeyItem(33, "F", "√", "X\u00B2",{SDLK_f}),       // P03, P14
        new TKeyItem(34, "G", "4",NULL,{SDLK_g,SDLK_4}),       // P04, P14
        new TKeyItem(35, "H", "5",NULL,{SDLK_h,SDLK_5}),       // P05, P14
        new TKeyItem(36, "J", "6",NULL,{SDLK_j,SDLK_6}),       // P06, P14
        new TKeyItem(37, "K", "±",NULL,{SDLK_k}),       // P07, P14
        
        new TKeyItem(20, "Q", "sin", "sin-1",{SDLK_q}),       // P00, P15
        new TKeyItem(21, "W", "cos", "cos-1",{SDLK_w}),       // P01, P15
        new TKeyItem(22, "E", "tan", "tan-1",{SDLK_e}),       // P02, P15
        new TKeyItem(23, "R", "1/X", "hyp",{SDLK_r}),       // P03, P15
        new TKeyItem(24, "T", "7",NULL,{SDLK_t,SDLK_7}),       // P04, P15
        new TKeyItem(25, "Y", "8",NULL,{SDLK_y,SDLK_8}),       // P05, P15
        new TKeyItem(26, "U", "9",NULL,{SDLK_u,SDLK_9}),       // P06, P15
        new TKeyItem(27, "I", "%",NULL,{SDLK_i}),       // P07, P15
        
        new TKeyItem(28, "O", "÷", "#",{SDLK_o}),           // P00, P16
        new TKeyItem(38, "L", "x", "*",{SDLK_l}),           // P01, P16
        new TKeyItem(48, "▲", "-",NULL,{SDLK_UP}),         // P02, P16
        new TKeyItem(58, "▼", "+",NULL,{SDLK_DOWN}),     // P03, P16
        new TKeyItem(29, "P", "MC", "☎",{SDLK_p}),           // P04, P16
        new TKeyItem(39, "输入", "MR",NULL,{SDLK_RETURN}),   // P05, P16
        new TKeyItem(49, "⇟", "M-",NULL,{SDLK_SLASH}), // P06, P16
        new TKeyItem(59, "→", "M+",NULL,{SDLK_RIGHT}),   // P07, P16
        
        NULL,       // P00, P17
        NULL,       // P01, P17
        new TKeyItem(12, "F1", NULL, "插入",{SDLK_F1}),       // P02, P17
        new TKeyItem(13, "F2", NULL, "删除",{SDLK_F2}),       // P03, P17
        new TKeyItem(14, "F3", NULL, "查找",{SDLK_F3}),       // P04, P17
        new TKeyItem(15, "F4", NULL, "修改",{SDLK_F4}),       // P05, P17
        NULL,       // P06, P17
        NULL,       // P07, P17
    };


map<int,uint> sdl_to_item;
void init_keyitems(){
      for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            //fKeyItems[y][x] = item[y][x];
            if (item[y][x] == NULL) {
                //keypadmatrix[y][x] = 2;
            } else {
                int row = item[y][x]->fRow;
                int col = item[y][x]->fColumn;
                int index = row * 10 + col;
                //MyRectButton* button = [MyRectButton buttonWithType:UIButtonTypeRoundedRect];
                //button.contentScaleFactor = 1;
                //button.layer.contentsScale = 1;
                item[y][x]->tag = y * 0x10 + x;
                for(auto e: item[y][x]->sdl_keys){
                    sdl_to_item[e]=item[y][x]->tag;
                }
            }
        }
      }
}

uint8_t map_key_neko(int32_t sym){
  if(sdl_to_item.find(sym)!=sdl_to_item.end()){
    return sdl_to_item[sym];
  }
  return 0;
}

void SetKeyNeko(uint8_t key_id, bool down_or_up){
    unsigned int y = key_id / 16;
    unsigned int x = key_id % 16;
    if (y < 8 && x < 8) {
        keypadmatrix[y][x] = down_or_up;
    }

}

void handle_key_neko(signed int sym, bool key_down){
        /*if(enable_debug_key_shoot){
          printf("event <%d,%d; %llu>\n", sym,key_down,SDL_GetTicks64()%1000);
        }*/
        uint8_t value=map_key_neko(sym);
        if(value!=0){
           SetKeyNeko(value, key_down);
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


bool InitEverything() {
  if (SDL_Init(SDL_INIT_EVERYTHING) == -1) {
    std::cout << " Failed to initialize SDL : " << SDL_GetError() << std::endl;
    return false;
  }
  init_audio();

  SDL_Window* window =
    SDL_CreateWindow("WQX", 0, 40, LINE_SIZE * SCREEN_WIDTH *total_size, LINE_SIZE * SCREEN_HEIGHT *total_size, 0);
  if (!window) {
    std::cout << "Failed to create window : " << SDL_GetError() << std::endl;
    return false;
  }
  renderer = SDL_CreateRenderer(window, -1, 0);
  if (!renderer) {
    std::cout << "Failed to create renderer : " << SDL_GetError() << std::endl;
    return false;
  }
  SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH * LINE_SIZE *total_size, SCREEN_HEIGHT * LINE_SIZE *total_size);

  Initialize();
  LoadNC1020();
  
  return true;
}

void Render() {
  SDL_RenderClear(renderer);
  SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
    SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH*total_size, SCREEN_HEIGHT*total_size);

  unsigned char* bytes = nullptr;
  int pitch = 0;
  static const SDL_Rect source = { 0, 0, SCREEN_WIDTH*total_size, SCREEN_HEIGHT*total_size };
  SDL_LockTexture(texture, &source, reinterpret_cast<void**>(&bytes), &pitch);
  
  static const unsigned char colors[4]={255,180,105,0};
  static const unsigned char shadows[4]={255,
        (unsigned char)(colors[1]+(255-colors[1])/8),
        (unsigned char)(colors[2]+(255-colors[2])/4),
        (unsigned char)(colors[3]+(255-colors[3])/2)
        };

  static const unsigned char white_color[4] = { 0, colors[0], colors[0], colors[0] };
  static const unsigned char near_white_color[4] = { 0, colors[1], colors[1], colors[1] };
  static const unsigned char near_black_color[4] = { 0, colors[2], colors[2], colors[2] };
  static const unsigned char black_color[4] = { 0, colors[3], colors[3], colors[3] };

  static const unsigned char white_color_shadow[4] = { 0, shadows[0], shadows[0], shadows[0] };
  static const unsigned char near_white_color_shadow[4] = { 0, shadows[1], shadows[1], shadows[1]  };
  static const unsigned char near_black_color_shadow[4] = { 0, shadows[2], shadows[2], shadows[2]  };
  static const unsigned char black_color_shadow[4] = { 0, shadows[3], shadows[3], shadows[3] };

  static const unsigned char * index[4]={white_color,near_white_color,near_black_color,black_color};
  static const unsigned char * index_shadow[4]={white_color, near_white_color_shadow, near_black_color_shadow, black_color_shadow};
  static const size_t color_size = sizeof(black_color);
  //unsigned char lcd[80*(pixel_size+gap_zize)][160*(pixel_size+gap_zize)][color_size] ;
  //unsigned char lcd[80*(pixel_size+gap_zize)][160*(pixel_size+gap_zize)][color_size] ;
  unsigned char (*p)[160*total_size][color_size] ;
  p=(unsigned char (*)[160*total_size][color_size] ) bytes;
  if(!is_grey_mode()){
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT/8; ++i) {
      for (int j = 0; j < 8; ++j) {
        bool pixel = (lcd_buf[i] & (1 << (7 - j))) != 0;
        int pos=i*8+j;
        int value= pixel? 3:0;
        int r=pos/160;
        int c=pos%160;
        //memcpy(p[r][c],index[value],color_size);
        for(int u=r*total_size;u<r*total_size+total_size;u++){
          for(int v=c*total_size;v<c*total_size+total_size;v++){
              if(u-r*total_size<pixel_size && v-c*total_size<pixel_size){
                memcpy(p[u][v],index[value],color_size);     
              }/*else if (u==r*total_size &&  v-c*total_size>=pixel_size || v==c*total_size && u-r*total_size>=pixel_size){
                memcpy(p[u][v],index[0],color_size);  
              }*/
              else{
                memcpy(p[u][v],index_shadow[value],color_size);
                /*unsigned char tmp[4];
                memcpy(tmp,index[value],color_size);
                tmp[1]+=(255-tmp[1])/2;tmp[2]+=(255-tmp[2])/2;tmp[3]+=(255-tmp[3])/2;
                memcpy(p[u][v],tmp,color_size);*/
              }
          }
        }
        //memcpy(bytes, pixel ? black_color : white_color, color_size);
        //bytes += color_size;
      }
    }
  }else{
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT/8 *2; ++i) {
      for (int j = 0; j < 4; ++j) {
        uint8_t value=(lcd_buf[i]>>(6-j*2)) &0x03;
        int pos=(i*8+j*2)/2;
        int r=pos/160;
        int c=pos%160;
        //memcpy(p[r][c],index[value],color_size);
        for(int u=r*total_size;u<r*total_size+total_size;u++){
          for(int v=c*total_size;v<c*total_size+total_size;v++){
              if(u-r*total_size<pixel_size && v-c*total_size<pixel_size){
                memcpy(p[u][v],index[value],color_size);     
              }/*else if (u==r*total_size &&  v-c*total_size>=pixel_size || v==c*total_size && u-r*total_size>=pixel_size){
                memcpy(p[u][v],index[0],color_size);  
              }*/
              else{
                memcpy(p[u][v],index_shadow[value],color_size);
                /*unsigned char tmp[4];
                memcpy(tmp,index[value],color_size);
                tmp[1]+=(255-tmp[1])/2;tmp[2]+=(255-tmp[2])/2;tmp[3]+=(255-tmp[3])/2;
                memcpy(p[u][v],tmp,color_size);*/
              }
          }
        }
      }
    }
  }
  /*
  for(int i=0;i<80;i++){
    for(int j=0;j<160;j++){
      memcpy(bytes, index[lcd[i][j]],color_size);
      bytes+=color_size;
    }
  }*/
  SDL_UnlockTexture(texture);
  static const SDL_Rect destination =
    { 0, 0, SCREEN_WIDTH * LINE_SIZE *total_size, SCREEN_HEIGHT * LINE_SIZE *total_size };
  SDL_RenderCopy(renderer, texture, &source, &destination);
  SDL_RenderPresent(renderer);
  SDL_DestroyTexture(texture);
}

uint8_t map_key(int32_t sym){
    switch(sym){
          case SDLK_RIGHT: return 0x1F;
          case SDLK_LEFT: return 0x3F;
          case SDLK_DOWN: return 0x1B;
          case SDLK_UP: return 0x1A;

          case SDLK_RETURN: return 0x1D;
          case SDLK_SPACE: return 0x3E;
          case SDLK_PERIOD: return 0x3D;
          case SDLK_ESCAPE: return 0x3B;
          case SDLK_MINUS: return 0x0E;
          case SDLK_EQUALS: return 0x3E;

          case SDLK_LEFTBRACKET: return 0x38;
          case SDLK_RIGHTBRACKET: return 0x39;
          case SDLK_BACKSLASH: return 0x3A;

          case SDLK_COMMA: return 0x37;
          case SDLK_SLASH: return 0x1E;
          case SDLK_BACKSPACE: return 0x3F;

          case SDLK_0: return 0x3c;
          case SDLK_1: return 0x34;
          case SDLK_2: return 0x35;
          case SDLK_3: return 0x36;
          case SDLK_4: return 0x2c;
          case SDLK_5: return 0x2d;
          case SDLK_6: return 0x2e;
          case SDLK_7: return 0x24;
          case SDLK_8: return 0x25;
          case SDLK_9: return 0x26;

          case SDLK_a: return 0x28;
          case SDLK_b: return 0x34;
          case SDLK_c: return 0x32;
          case SDLK_d: return 0x2a;
          case SDLK_e: return 0x22;
          case SDLK_f: return 0x2b;
          case SDLK_g: return 0x2c;
          case SDLK_h: return 0x2d;
          case SDLK_i: return 0x27;
          case SDLK_j: return 0x2e;
          case SDLK_k: return 0x2f;
          case SDLK_l: return 0x19;
          case SDLK_m: return 0x36;
          case SDLK_n: return 0x35;
          case SDLK_o: return 0x18;
          case SDLK_p: return 0x1c;
          case SDLK_q: return 0x20;
          case SDLK_r: return 0x23;
          case SDLK_s: return 0x29;
          case SDLK_t: return 0x24;
          case SDLK_u: return 0x26;
          case SDLK_v: return 0x33;
          case SDLK_w: return 0x21;
          case SDLK_x: return 0x31;
          case SDLK_y: return 0x25;
          case SDLK_z: return 0x30;

          case SDLK_F1: return 0x10;
          case SDLK_F2: return 0x11;
          case SDLK_F3: return 0x12;
          case SDLK_F4: return 0x13;
          case SDLK_F5: return 0x0B;
          case SDLK_F6: return 0x0C;
          case SDLK_F7: return 0x0D;
          case SDLK_F8: return 0x0A;
          case SDLK_F9: return 0x09;
          case SDLK_F10: return 0x08;
          case SDLK_F11: return 0x0E;
          case SDLK_F12: return 0x0F;

          case SDLK_SEMICOLON: return 0x15;
          case SDLK_QUOTE: return 0x14;
          
          default:return 0xff;
    }

    /*note: 0x01  infra red
            0x00  on/off  (according to peek/pkoe)
            0x0f  on/off  (according to hackwaly/nc1020/NC1020_KeypadView.java) (wang-yue/NC1020/blob/master/main.cpp)
            0x02  on/off   (according to hackwaly/jswqx/src/keyinput.js)
    */
}
void handle_key(signed int sym, bool key_down){
        if(enable_debug_key_shoot){
          printf("event <%d,%d; %llu>\n", sym,key_down,SDL_GetTicks64()%1000);
        }
        uint8_t value=map_key(sym);
        if(value!=0xff){
          SetKey(value, key_down);
        }
        switch ( sym) {
          case SDLK_BACKQUOTE:
            if(key_down==1){
              //enable_dyn_debug^= 0x1;
            }
            break;

          case SDLK_TAB:
            if(key_down==1){
                fast_forward^= 0x1;
            }
            break;

          default :  // unsupported
            break;
        }
}

void RunGame() {
  bool loop = true;

  uint64_t start_tick = SDL_GetTicks64();
  uint64_t expected_tick = 0;

  while (loop) {
    RunTimeSlice(SLICE_INTERVAL, false);

    SDL_Event event;
    map<signed int, bool> mp;
    while (SDL_PollEvent(&event)) {
      if ( event.type == SDL_QUIT ) {
        loop = false;
      } else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        bool key_down = (event.type == SDL_KEYDOWN);

        //try to consolidate multiple key shoot into one
        //not sure if necessary. But it's helpful for debug
        mp[event.key.keysym.sym]= key_down;
        for(auto it=mp.begin();it!=mp.end();it++){
          handle_key_neko(it->first,it->second);
        }
      }
    }

    if(expected_tick/LCD_REFRESH_INTERVAL != (expected_tick+SLICE_INTERVAL)/LCD_REFRESH_INTERVAL){
      if (!CopyLcdBuffer(lcd_buf)) {
        std::cout << "Failed to copy buffer renderer." << std::endl;
      }
      Render();
    }
    expected_tick+=SLICE_INTERVAL;
    uint64_t actual_tick= SDL_GetTicks64() - start_tick;

  if(fast_forward) {
      expected_tick =actual_tick;
  }

  //if actual is behind expected_tick too much, we only remember 300ms
  if(actual_tick >expected_tick + 300) {
    expected_tick = actual_tick-300;
  }

  // similiar strategy as above
  if(expected_tick > actual_tick + 300) {
    actual_tick = expected_tick-300;
  }

  if(actual_tick < expected_tick) {
    {SDL_Delay(expected_tick-actual_tick);}
    long long exceed=SDL_GetTicks64()-start_tick  -expected_tick;
    if(exceed>10){
      printf("oops sleep too much %lld\n",exceed);
    }
  }

  /*while((actual_tick=SDL_GetTicks64() - start_tick) <expected_tick) {
    //{SDL_Delay(expected_tick-actual_tick);}
  }*/
    //SDL_Delay(FRAME_INTERVAL < tick ? 0 : FRAME_INTERVAL - tick);
  }
}
int main(int argc, char* args[]) {
  int listen_port=9000;
  if(argc>1){
    sscanf(args[1],"%d",&listen_port);
  }
  init_udp_server(listen_port);
  init_keyitems();
  if (!InitEverything())
    return -1;
  
  //SDL_SetThreadPriority(SDL_THREAD_PRIORITY_HIGH);
  //SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);
  RunGame();
  if(false){
    SaveNC1020();
  }

  return 0;
}
