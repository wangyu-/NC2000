#include <SDL2/SDL.h>
#include "comm.h"
#include "nc2000.h"
#include <SDL_events.h>
#include <SDL_keycode.h>
#include <cstring>
#include <iostream>
#include <map>
#include "sound.h"
#include "misc/udp_server.h"
#include "key.h"
#include "key_new.h"
#include "settings.h"
#include "display.h"
#include "console.h"
#include "lcdstripe/lcdpainter.h"

using namespace std;

SDL_Window* window;
SDL_Renderer* renderer;
MyLCDView*  lcdview;

//Initialize Resource, this function is not supposed to be called repeatedly, otherwise there will be resource leak.
// If you are trying to create an emscripten/android/ios version, init_resource() should be called only once,
// emu_entry() is the only function you need to re-call after switching rom or model. 
void init_resource() {
  #if defined(__MINGW32__)
  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
#endif
  int res1=SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);
  if(debug_level>=1) printf("SDL_SetThreadPriority returned %d\n", res1);

  if(listen_port>0) init_udp_server(listen_port);

  lcd_effect_buffer = new unsigned char[SCREEN_HEIGHT*total_size* SCREEN_WIDTH*total_size * 4];
  memset(lcd_effect_buffer, 0, SCREEN_HEIGHT*total_size* SCREEN_WIDTH*total_size * 4);

  if (SDL_Init(SDL_INIT_EVERYTHING) == -1) {
    std::cout << " Failed to initialize SDL : " << SDL_GetError() << std::endl;
    exit(-1);
  }
  init_audio();

  window =
    SDL_CreateWindow(get_str_of_mode().c_str(), 0, 40, lcd_scale * (SCREEN_WIDTH +LEFT_GAP +RIGHT_GAP-1) *total_size +(LEFT_GAP_EXTRA+RIGHT_GAP_EXTRA)*lcd_scale, lcd_scale * SCREEN_HEIGHT *total_size, 0);
  if (!window) {
    std::cout << "Failed to create window : " << SDL_GetError() << std::endl;
    exit(-1);
  }
  renderer = SDL_CreateRenderer(window, -1, 0);
  if (!renderer) {
    std::cout << "Failed to create renderer : " << SDL_GetError() << std::endl;
    exit(-1);
  }

  lcdview = new MyLCDView(("resource/lcdstripe_slice_"+lcdstripe_suffix+".json").c_str());
  lcdview->loadStripeTexture(("resource/lcdstripe_"+lcdstripe_suffix+".bmp").c_str(), renderer);
  
}

long long get_current_time_milliseconds() {
    struct timespec spec;
    if (clock_gettime(CLOCK_REALTIME, &spec) == -1) {
        // Handle error, e.g., print an error message and exit
        perror("clock_gettime");
        return -1; 
    }
    // Convert seconds and nanoseconds to milliseconds
    return (long long)spec.tv_sec * 1000 + (long long)spec.tv_nsec / 1000000; 
}

void delay_milliseconds(int ms){
  if(spin_wait){
    u64_t start_tick = SDL_GetTicks64();
    while(SDL_GetTicks64() < start_tick + ms) {
        spin_iterations(100*1000);
    }
  }
  else {
    SDL_Delay(ms);
  }
}
void main_loop() {
  bool loop = true;
  bool power_save= false;

  u64_t start_tick = SDL_GetTicks64();
  u64_t expected_tick = 0;

  u64_t last_key_pressed_tick = 0;


  u64_t last_time_rtc=0;
  u64_t current_time_rtc=0;


  while (loop) {
    if(sync_on_resume && enable_auto_time_sync)
    {
      last_time_rtc = current_time_rtc;
      current_time_rtc = get_current_time_milliseconds();
      if(last_time_rtc && last_time_rtc > current_time_rtc) {
        if(debug_level>=1) printf("oops, time goes back, last=%llu current=%llu, delta=%llu\n",last_time_rtc,current_time_rtc, current_time_rtc - last_time_rtc);
      }
      if(last_time_rtc && current_time_rtc - last_time_rtc > 10*1000) {
        if(debug_level>=1) printf("detected time jump last=%llu current=%llu, delta=%llu\n",last_time_rtc,current_time_rtc,current_time_rtc-last_time_rtc);
        //there is timejump in between, likely because of system sleep and recover
        if(nc2000mode){
            void sync_time_2000();
            sync_time_2000();
        }
        if(nc1020mode){
            void sync_time_1020();
            sync_time_1020();}
      }
    }

    if(power_save) {
      delay_milliseconds(200);
    }
    if(! power_save){
      RunTimeSlice(SLICE_INTERVAL);
    }
  
    if(reload_pending){
      if(debug_level>=1) printf("reload pending, exit main loop\n");
      break;
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
          if(it->first==SDLK_LSHIFT || it->first==SDLK_RSHIFT){
            shift_down=it->second;
            continue;
          }
          if(it->first==SDLK_LCTRL || it->first==SDLK_RCTRL){
            ctrl_down=it->second;
            continue;
          }
          bool console_on_saved=console_on;
          handle_console(it->first, it->second);// handles 1. console toggle 2. console itself
          if(console_on_saved){
            continue;
          }
          if(use_legacy_key_io) handle_key(it->first, it->second);
          else handle_key_wayback(it->first,it->second);
        }
      } else if (event.type == SDL_TEXTINPUT) {
        input_text(event.text.text);
      }
    }

    if(!power_save){
      Render(expected_tick);
    }
    u64_t current_time = SDL_GetTicks64();

    if (key_pressed) {
      last_key_pressed_tick = current_time;
    }

    if(current_time - last_key_pressed_tick >power_save_interval*1000ll){
      if(power_save == false){
        power_save = true;
        printf("enter power save\n");
      }
    }else{
      if(power_save == true) {
        power_save = false;
        if(enable_auto_time_sync&&sync_on_resume){
          if(nc2000mode){
            printf("sync time on power save resume\n");
            void sync_time_2000();
            sync_time_2000();
          }
          if(nc1020mode){
            printf("sync time on power save resume\n");
            void sync_time_1020();
            sync_time_1020();
          }
        }
      }
    }

    expected_tick+=SLICE_INTERVAL;
    u64_t actual_tick= current_time - start_tick;

    if(fast_forward && !fast_forward_limit) {
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
      delay_milliseconds(expected_tick-actual_tick);
      long long exceed=SDL_GetTicks64() -start_tick  -expected_tick;
      if(exceed>10){
        if(debug_level>=1) printf("oops sleep too much %lld\n",exceed);
      }
    }

  }
}

//Entry Point of the emulator, this function can be called repeatedly if you need.
// repeating calling this function can be useful if you are creating an emscripten/android/ios version.
//E.g., you can change model and rom path, then call this function again to switch to new model and rom without restarting the whole program,
// check reload/load_nc2000/load_nc1020 command in cmd.cpp as an example
void emu_entry(){ 
    LoadNC2k();
    main_loop();
}

int main(int argc, char* args[]) {
  process_args(argc, args);
  init_resource();

  do {
    reload_pending=false;
    emu_entry();
  } while (reload_pending);

  SaveNC2kIfNeed(); // handle --auto-save-flash or --auto-save-all

  shutdown_audio(); //explictly shutdown audio to avoid bug on some platform. Other resources doesn't need this, since OS can always recollect them correctly.

  return 0;
}
