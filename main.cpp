#include <SDL2/SDL.h>
#include "comm.h"
#include "dsp/dsp.h"
#include "nc2000.h"
#include <SDL_events.h>
#include <SDL_keycode.h>
#include <cstring>
#include <iostream>
#include <map>
#include "sound.h"
#include "udp_server.h"
#include "key.h"
#include "key_new.h"
#include "settings.h"
#include "display.h"
#include "console.h"

using namespace std;

SDL_Window* window;

bool InitAudioVideo() {
  extern SDL_Renderer* renderer;
  lcd_effect_buffer = new unsigned char[SCREEN_HEIGHT*total_size* SCREEN_WIDTH*total_size * 4];
  memset(lcd_effect_buffer, 0, SCREEN_HEIGHT*total_size* SCREEN_WIDTH*total_size * 4);

  if (SDL_Init(SDL_INIT_EVERYTHING) == -1) {
    std::cout << " Failed to initialize SDL : " << SDL_GetError() << std::endl;
    return false;
  }
  init_audio();

  window =
    SDL_CreateWindow(get_str_of_mode().c_str(), 0, 40, lcd_scale * (SCREEN_WIDTH +LEFT_GAP +RIGHT_GAP-1) *total_size +(LEFT_GAP_EXTRA+RIGHT_GAP_EXTRA)*lcd_scale, lcd_scale * SCREEN_HEIGHT *total_size, 0);
  if (!window) {
    std::cout << "Failed to create window : " << SDL_GetError() << std::endl;
    return false;
  }
  renderer = SDL_CreateRenderer(window, -1, 0);
  if (!renderer) {
    std::cout << "Failed to create renderer : " << SDL_GetError() << std::endl;
    return false;
  }

  init_lcd_stripe();//need to call after renderer is created
  
  return true;
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
      SDL_Delay(200);
    }
    if(! power_save){
      RunTimeSlice(SLICE_INTERVAL);
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
      SDL_Delay(expected_tick-actual_tick);
      long long exceed=current_time -start_tick  -expected_tick;
      if(exceed>10){
        if(debug_level>=1) printf("oops sleep too much %lld\n",exceed);
      }
    }

  }
}

int main(int argc, char* args[]) {
  process_args(argc, args);
  init_parameters();
#if defined(__MINGW32__)
  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
#endif
  int res1=SDL_SetThreadPriority(SDL_THREAD_PRIORITY_TIME_CRITICAL);
  if(debug_level>=1) printf("SDL_SetThreadPriority returned %d\n", res1);

  if(listen_port>0) init_udp_server(listen_port);
  init_keyitems();
  LoadNC2k();
  if (!InitAudioVideo())
    return -1;

  main_loop();
  if(save_flash_on_exit){
    save_flash("");
  }
  if(save_state_on_exit){
    save_state("");
  }

  shutdown_audio();

  return 0;
}
