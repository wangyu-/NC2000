#include <SDL2/SDL.h>
#include "comm.h"
#include "nc2000.h"
#include <SDL_keycode.h>
#include <iostream>
#include <map>
#include "sound.h"
#include "udp_server.h"
#include "wayback_key.h"
using namespace std;

static int enable_debug_key_shoot=false;
static bool fast_forward=false;

static SDL_Renderer* renderer;
static uint8_t lcd_buf[SCREEN_WIDTH * SCREEN_HEIGHT / 8*2];


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
  bool power_save= false;

  uint64_t start_tick = SDL_GetTicks64();
  uint64_t expected_tick = 0;

  uint64_t last_key_pressed_tick = 0;

  while (loop) {
    if(power_save) {
      SDL_Delay(200);
    }
    if(! power_save){
      RunTimeSlice(SLICE_INTERVAL, false);
    }

    SDL_Event event;
    map<signed int, bool> mp;
    bool key_pressed= false;
    while (SDL_PollEvent(&event)) {
      if ( event.type == SDL_QUIT ) {
        loop = false;
      } else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        key_pressed = true;
        bool key_down = (event.type == SDL_KEYDOWN);
        //try to consolidate multiple key shoot into one
        //not sure if necessary. But it's helpful for debug
        mp[event.key.keysym.sym]= key_down;
        for(auto it=mp.begin();it!=mp.end();it++){
          if(use_legacy_key_io) handle_key(it->first, it->second);
          else handle_key_wayback(it->first,it->second);
        }
      }
    }

    if(expected_tick/LCD_REFRESH_INTERVAL != (expected_tick+SLICE_INTERVAL)/LCD_REFRESH_INTERVAL){
      if(!power_save){
        if (!CopyLcdBuffer(lcd_buf)) {
          std::cout << "Failed to copy buffer renderer." << std::endl;
        }
        Render();
      }
    }

    uint64_t current_time = SDL_GetTicks64();
    if (key_pressed) {
      last_key_pressed_tick = current_time;
    }

    if(current_time - last_key_pressed_tick >300*1000){
      if(power_save == false){
        power_save = true;
        printf("enter power save\n");
      }
    }else{
      if(power_save == true) {
        power_save = false;
        printf("get out of power save\n");
      }
    }

    expected_tick+=SLICE_INTERVAL;
    uint64_t actual_tick= current_time - start_tick;

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
    long long exceed=current_time -start_tick  -expected_tick;
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
