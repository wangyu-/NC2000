#include <SDL2/SDL.h>
#include "nc1020.h"
#include <SDL_keycode.h>
#include <iostream>
#include <map>
using namespace std;


int enable_debug_key_shoot=false;
bool fast_forward=false;

SDL_Renderer* renderer;
static uint8_t lcd_buf[SCREEN_WIDTH * SCREEN_HEIGHT / 8];

bool InitEverything() {
  if (SDL_Init(SDL_INIT_EVERYTHING) == -1) {
    std::cout << " Failed to initialize SDL : " << SDL_GetError() << std::endl;
    return false;
  }
  SDL_Window* window =
    SDL_CreateWindow("WQX", 0, 40, LINE_SIZE * SCREEN_WIDTH, LINE_SIZE * SCREEN_HEIGHT, 0);
  if (!window) {
    std::cout << "Failed to create window : " << SDL_GetError() << std::endl;
    return false;
  }
  renderer = SDL_CreateRenderer(window, -1, 0);
  if (!renderer) {
    std::cout << "Failed to create renderer : " << SDL_GetError() << std::endl;
    return false;
  }
  SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH * LINE_SIZE, SCREEN_HEIGHT * LINE_SIZE);

  Initialize();
  LoadNC1020();
  
  return true;
}

void Render() {
  SDL_RenderClear(renderer);
  SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
    SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);

  unsigned char* bytes = nullptr;
  int pitch = 0;
  static const SDL_Rect source = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
  SDL_LockTexture(texture, &source, reinterpret_cast<void**>(&bytes), &pitch);
  static const unsigned char on_color[4] = { 255, 0, 0, 0 };
  static const unsigned char off_color[4] = { 255, 255, 255, 255 };
  static const size_t color_size = sizeof(on_color);
  for (int i = 0; i < sizeof(lcd_buf); ++i) {
    for (int j = 0; j < 8; ++j) {
      bool pixel = (lcd_buf[i] & (1 << (7 - j))) != 0;
      memcpy(bytes, pixel ? on_color : off_color, color_size);
      bytes += color_size;
    }
  }
  SDL_UnlockTexture(texture);
  static const SDL_Rect destination =
    { 0, 0, SCREEN_WIDTH * LINE_SIZE, SCREEN_HEIGHT * LINE_SIZE };
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
          case SDLK_DELETE: return 0x3F;

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

          default:return 0x00;
    }
}
void handle_key(signed int sym, bool key_down){
        if(enable_debug_key_shoot){
          printf("event <%d,%d; %llu>\n", sym,key_down,SDL_GetTicks64()%1000);
        }
        uint8_t value=map_key(sym);
        if(value!=0){
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


    RunTimeSlice(FRAME_INTERVAL, false);

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
          handle_key(it->first,it->second);
        }
      }
    }
    if (!CopyLcdBuffer(lcd_buf)) {
      std::cout << "Failed to copy buffer renderer." << std::endl;
    }
    Render();
    expected_tick+=FRAME_INTERVAL;
    uint64_t actual_tick= SDL_GetTicks64() - start_tick;

  if(fast_forward) {
      expected_tick =actual_tick;
  }
  if(actual_tick <expected_tick) {
    {SDL_Delay(expected_tick-actual_tick);}
  }
    //SDL_Delay(FRAME_INTERVAL < tick ? 0 : FRAME_INTERVAL - tick);
  }
}

int main(int argc, char* args[]) {
  if (!InitEverything())
    return -1;
  
  RunGame();
  if(false){
    SaveNC1020();
  }

  return 0;
}
