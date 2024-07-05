#include <SDL2/SDL.h>
#include "nc1020.h"
#include <SDL_keycode.h>
#include <iostream>
#include <map>
using namespace std;
const uint32_t FRAME_RATE=30;
const uint32_t FRAME_INTERVAL= (1000u/FRAME_RATE);


#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 80
#define LINE_SIZE 2

const int enable_debug_key_shoot=true;
bool fast_forward=false;
//const int enable_debug_cycles=false;

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

  wqx::WqxRom rom = {
    .romPath = "./obj_lu.bin",
    .norFlashPath = "./nc1020.fls",
    .statesPath = "./nc1020.sts",
  };

  if(nc2000){
     rom.norFlashPath= "./2600nor.bin";
  }
  wqx::Initialize(rom);
  wqx::LoadNC1020();
  
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


void handle_key(signed int sym, bool key_down){
        if(enable_debug_key_shoot){
          printf("event <%d,%d; %llu>\n", sym,key_down,SDL_GetTicks64()%1000);
        }
        switch ( sym) {
          case SDLK_RIGHT: // Right
            wqx::SetKey(0x1F, key_down);
            break;
          case SDLK_LEFT:  // Left
            wqx::SetKey(0x3F, key_down);
            break;
          case SDLK_DOWN:  // Down
            wqx::SetKey(0x1B, key_down);
            break;
          case SDLK_UP:  // Up
            wqx::SetKey(0x1A, key_down);
            break;

          case SDLK_RETURN:
            wqx::SetKey(0x1D, key_down);
            break;
          case SDLK_SPACE:
            wqx::SetKey(0x3E, key_down);
            break;
          case SDLK_PERIOD:
            wqx::SetKey(0x3D, key_down);
            break;
          case SDLK_ESCAPE: 
            wqx::SetKey(0x3B, key_down);
            break;
          case SDLK_MINUS: 
            wqx::SetKey(0x0E, key_down);
            break;
          case SDLK_EQUALS: 
            wqx::SetKey(0x3E, key_down);
            break;

          case SDLK_LEFTBRACKET:
            wqx::SetKey(0x38, key_down); 
            break;
          case SDLK_RIGHTBRACKET:
            wqx::SetKey(0x39, key_down); 
            break;
          case SDLK_BACKSLASH:
            wqx::SetKey(0x3A, key_down); 
            break;

          case SDLK_COMMA:
            wqx::SetKey(0x37, key_down); 
            break;
          case SDLK_SLASH:
            wqx::SetKey(0x1E, key_down); 
            break;

          case SDLK_DELETE:
            wqx::SetKey(0x3F, key_down); 
            break;

          /*case SDLK_TAB:
            wqx::SetKey(0x3A, key_down); 
            break;*/



    
          case SDLK_0: 
            wqx::SetKey(0x3c, key_down);
            break;
          case SDLK_1:
            wqx::SetKey(0x34, key_down);
            break;
          case SDLK_2: 
            wqx::SetKey(0x35, key_down);
            break;
          case SDLK_3: 
            wqx::SetKey(0x36, key_down);
            break;
          case SDLK_4: 
            wqx::SetKey(0x2c, key_down);
            break;
          case SDLK_5: 
            wqx::SetKey(0x2d, key_down);
            break;
          case SDLK_6:
            wqx::SetKey(0x2e, key_down);
            break;
          case SDLK_7: 
            wqx::SetKey(0x24, key_down);
            break;
          case SDLK_8: 
            wqx::SetKey(0x25, key_down);
            break;
          case SDLK_9: 
            wqx::SetKey(0x26, key_down);
            break;





          case SDLK_a: 
            wqx::SetKey(0x28, key_down);
            break;
          case SDLK_b: 
            wqx::SetKey(0x34, key_down);
            break;
          case SDLK_c: 
            wqx::SetKey(0x32, key_down);
            break;
          case SDLK_d: 
            wqx::SetKey(0x2a, key_down);
            break;
          case SDLK_e: 
            wqx::SetKey(0x22, key_down);
            break;
          case SDLK_f: 
            wqx::SetKey(0x2b, key_down);
            break;
          case SDLK_g: 
            wqx::SetKey(0x2c, key_down);
            break;
          case SDLK_h: 
            wqx::SetKey(0x2d, key_down);
            break;
          case SDLK_i: 
            wqx::SetKey(0x27, key_down);
            break;
          case SDLK_j: 
            wqx::SetKey(0x2e, key_down);
            break;
          case SDLK_k: 
            wqx::SetKey(0x2f, key_down);
            break;
          case SDLK_l: 
            wqx::SetKey(0x19, key_down);
            break;
          case SDLK_m: 
            wqx::SetKey(0x36, key_down);
            break;
          case SDLK_n: 
            wqx::SetKey(0x35, key_down);
            break;
          case SDLK_o: 
            wqx::SetKey(0x18, key_down);
            break;
          case SDLK_p: 
            wqx::SetKey(0x1c, key_down);
            break;
          case SDLK_q: 
            wqx::SetKey(0x20, key_down);
            break;
          case SDLK_r: 
            wqx::SetKey(0x23, key_down);
            break;
          case SDLK_s: 
            wqx::SetKey(0x29, key_down);
            break;
          case SDLK_t: 
            wqx::SetKey(0x24, key_down);
            break;
          case SDLK_u: 
            wqx::SetKey(0x26, key_down);
            break;
          case SDLK_v: 
            wqx::SetKey(0x33, key_down);
            break;
          case SDLK_w: 
            wqx::SetKey(0x21, key_down);
            break;
          case SDLK_x: 
            wqx::SetKey(0x31, key_down);
            break;
          case SDLK_y: 
            wqx::SetKey(0x25, key_down);
            break;
          case SDLK_z: 
            wqx::SetKey(0x30, key_down);
            break;


          case SDLK_F1:
            wqx::SetKey(0x10, key_down);
            break;
          case SDLK_F2:
            wqx::SetKey(0x11, key_down);
            break;
          case SDLK_F3:
            wqx::SetKey(0x12, key_down);
            break;
          case SDLK_F4:
            wqx::SetKey(0x13, key_down);
            break;
          case SDLK_F5:
            wqx::SetKey(0x0B, key_down);
            break;
          case SDLK_F6:
            wqx::SetKey(0x0C, key_down); 
            break;
          case SDLK_F7:
            wqx::SetKey(0x0D, key_down);
            break;
          case SDLK_F8: 
            wqx::SetKey(0x0A, key_down); 
            break;
          case SDLK_F9:
            wqx::SetKey(0x09, key_down);
            break;
          case SDLK_F10:
            wqx::SetKey(0x08, key_down); 
            break;
          case SDLK_F11:
            wqx::SetKey(0x0E, key_down); 
            break;
          case SDLK_F12:
            wqx::SetKey(0x0F, key_down); 
            break;

          case SDLK_BACKQUOTE:
            if(key_down==1){
              enable_dyn_debug^= 0x1;
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


    wqx::RunTimeSlice(FRAME_INTERVAL, false);

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
    if (!wqx::CopyLcdBuffer(lcd_buf)) {
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
  //wqx::SaveNC1020();

  return 0;
}
