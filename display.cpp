#include <SDL2/SDL.h>
#include "comm.h"
#include "compare/c6502.h"
#include "nc2000.h"
#include "console.h"
#include "lcdstripe/lcdpainter.h"
#include "display.h"

SDL_Renderer* renderer;

uint8_t lcd_buf[SCREEN_WIDTH * SCREEN_HEIGHT / 8*2];

MyLCDView*  lcdview;

void init_lcd_stripe(){
   lcdview = new MyLCDView(("resource/lcdstripe_slice_"+lcdstripe_suffix+".json").c_str());
   lcdview->loadStripeTexture(("resource/lcdstripe_"+lcdstripe_suffix+".bmp").c_str(), renderer);
}

unsigned char *lcd_effect_buffer = nullptr;

inline void handle_pixel(int u,int v,const unsigned char * color_arr[], int idx){
    //todo: inefficient, change to for each lcd tile use SDL_RenderCopy
    unsigned char (*p)[SCREEN_WIDTH*total_size][4] ;
    p = (decltype(p)) lcd_effect_buffer;
    if(!enable_lcd_latency_effect){
        memcpy(p[u][v], color_arr[idx], 4);
    }else{
      for(int i=1;i<4;i++){
        if( color_arr[idx][i]<p[u][v][i] ){//value smaller means darker, which mean higher voltage on wqx
          //voltage increasing
          unsigned int delta=p[u][v][i]- color_arr[idx][i];
          delta*=lcd_effect_charge_a;
          delta/=lcd_effect_charge_b;
          if(delta==0) delta++;
          p[u][v][i]-=delta;
        }if(color_arr[idx][i]>p[u][v][i]){
          //voltage reducing
          unsigned int delta=color_arr[idx][i]-p[u][v][i];
          delta*=lcd_effect_discharge_a;
          delta/=lcd_effect_discharge_b;
          if(delta==0) delta++;
          p[u][v][i]+=delta;
        }else{
        }
      }
    }
}
int render_cnt=0;
uint64_t last_inner_render_tick=0;
uint64_t last_outer_render_tick=0;
void Render(uint64_t tick) {
  if(tick/LCD_INNER_REFRESH_INTERVAL == last_inner_render_tick/LCD_INNER_REFRESH_INTERVAL){
    return; //not time to render
  }
  last_inner_render_tick= tick;

  unsigned char &lcden = nc2k_states.lcden;
  unsigned char &lcdon = nc2k_states.lcdon;
  bool lcd_on = true;
  if(nc2000mode||nc1020mode){
    uint8_t* ram_io=nc2k_states.ram_io;
    if(nc2000mode) lcd_on = (lcden && lcdon);
    if(nc1020mode) lcd_on = lcdon;
    if(ram_io[0x05]>>5==7){ //clk off
      lcd_on=false;
    }
  }
  if (console_on){
    draw_console();
  }else if(!lcd_on) {
    memset(lcd_buf, 0, sizeof(lcd_buf));
  }
  else if (!CopyLcdBuffer(lcd_buf)) {
    std::cout << "Failed to copy buffer renderer." << std::endl;
  }

  
  SDL_Texture *texture;
  static SDL_Rect source = { 0, 0, SCREEN_WIDTH*total_size, SCREEN_HEIGHT*total_size };
  unsigned char* bytes = nullptr;
  int pitch = 0;
  bool do_SDL_refresh = (tick/LCD_OUTER_REFRESH_INTERVAL != last_outer_render_tick/LCD_OUTER_REFRESH_INTERVAL);

  if(do_SDL_refresh){
    last_outer_render_tick= tick;
    render_cnt++;
    //if(render_cnt%100==0) printf("Render %d times\n",cnt);
    SDL_RenderSetLogicalSize(renderer, lcdview->getLCDWidth(), lcdview->getLCDHeight());
    lcdview->paint(renderer, true, lcd_on&&!console_on);

    SDL_RenderSetLogicalSize(renderer, (SCREEN_WIDTH +LEFT_GAP +RIGHT_GAP-1) * lcd_scale *total_size+ (LEFT_GAP_EXTRA+RIGHT_GAP_EXTRA)*lcd_scale, SCREEN_HEIGHT * lcd_scale *total_size);
    //SDL_RenderClear(renderer);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
    SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH*total_size, SCREEN_HEIGHT*total_size);
    SDL_LockTexture(texture, &source, reinterpret_cast<void**>(&bytes), &pitch);
  }
  
  static const unsigned char colors[4]={245,180,105,0};
  static const unsigned char shadows[4]={255,
        (unsigned char)(colors[1]+(255-colors[1])/8),
        (unsigned char)(colors[2]+(255-colors[2])/4),
        (unsigned char)(colors[3]+(255-colors[3])/3)
        };

  static const unsigned char pure_white_color[4] = { 0, colors[0], colors[0], colors[0] };
  static const unsigned char pure_white_color_shadow[4] = { 0, shadows[0],shadows[0],shadows[0]};

  static const unsigned char white_color[4] = { 0, (unsigned char)(colors[0]*b_scale), (unsigned char)(colors[0]*g_scale), (unsigned char)(colors[0]*r_scale) };
  static const unsigned char near_white_color[4] = { 0, (unsigned char)(colors[1]*b_scale), (unsigned char)(colors[1]*g_scale), (unsigned char)(colors[1]*r_scale) };
  static const unsigned char near_black_color[4] = { 0, (unsigned char)(colors[2]*b_scale), (unsigned char)(colors[2]*g_scale), (unsigned char)(colors[2]*r_scale) };
  static const unsigned char black_color[4] = { 0, (unsigned char)(colors[3]*b_scale), (unsigned char)(colors[3]*g_scale), (unsigned char)(colors[3]*r_scale) };

  static const unsigned char white_color_shadow[4] = { 0, (unsigned char)(shadows[0]*b_scale), (unsigned char)(shadows[0]*g_scale), (unsigned char)(shadows[0]*r_scale) };
  static const unsigned char near_white_color_shadow[4] = { 0, (unsigned char)(shadows[1]*b_scale), (unsigned char)(shadows[1]*g_scale), (unsigned char)(shadows[1]*r_scale) };
  static const unsigned char near_black_color_shadow[4] = { 0, (unsigned char)(shadows[2]*b_scale), (unsigned char)(shadows[2]*g_scale), (unsigned char)(shadows[2]*r_scale) };
  static const unsigned char black_color_shadow[4] = { 0, (unsigned char)(shadows[3]*b_scale), (unsigned char)(shadows[3]*g_scale), (unsigned char)(shadows[3]*r_scale) };

  static const unsigned char * index[4]={white_color,near_white_color,near_black_color,black_color};
  static const unsigned char * index_shadow[4]={white_color_shadow, near_white_color_shadow, near_black_color_shadow, black_color_shadow};
  static const size_t color_size = sizeof(black_color);

  static const unsigned char black_color_console[4] = { 0, (unsigned char)(255*1.0), 0, 0 };
  static const unsigned char black_color_shadow_console[4] = { 0, 0, (unsigned char)(255*1.0), 0 };
  static const unsigned char * index_console[4]={white_color,near_white_color,near_black_color,black_color_console};
  static const unsigned char * index_shadow_console[4]={white_color_shadow, near_white_color_shadow, near_black_color_shadow, black_color_shadow_console};
  //static const unsigned char * index_console[4]={white_color,near_white_color,near_black_color,black_color_console};

  unsigned char (*p)[SCREEN_WIDTH*total_size][4] ;
  p = (decltype(p)) lcd_effect_buffer;

  if(!is_grey_mode()){
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT/8; ++i) {
      for (int j = 0; j < 8; ++j) {
        bool pixel = (lcd_buf[i] & (1 << (7 - j))) != 0;
        int pos=i*8+j;
        int value= pixel? 3:0;
        int r=pos/160;
        int c=pos%160;
        if(c==0){
          lcdview->setPixel(0, r, value );
        }
        int u=r*total_size;
        int v=c*total_size;
        if(console_on) {
          handle_pixel(u,v,index_console,value);
          if(gap_size)handle_pixel(u,v+total_size-1,index_shadow_console,value);
        }
        else {
          handle_pixel(u,v,index,value);
          if(gap_size)handle_pixel(u,v+total_size-1,index_shadow,value);
        }
        if(!do_SDL_refresh) continue;
        int v1=*(int*)p[u][v];
        int v2=*(int*)p[u][v+total_size-1];
        for(int i=0;i<pixel_size;i++){
            int *p2=(int*)p[u+i][v];
            for(int j=0;j<pixel_size;j++){
                *(p2++) = v1;
            }
        }
        for(int i=0;i<gap_size;i++){
          for(int j=0;j<total_size;j++){
              *(int*)p[u+j][v+pixel_size+i] = v2;
              *(int*)p[u+pixel_size+i][v+j] = v2;
          }
        }

      }
    }
  }else{
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT/8 *2; ++i) {
      for (int j = 0; j < 4; ++j) {
        uint8_t value=(lcd_buf[i]>>(6-j*2)) &0x03;
        int pos=(i*8+j*2)/2;
        int r=pos/160;
        int c=pos%160;
        if(c==0){
          lcdview->setPixel(0, r, value );
        }
        int u=r*total_size;
        int v=c*total_size;
        handle_pixel(u,v,index,value);
        if(gap_size)handle_pixel(u,v+total_size-1,index_shadow,value);
        if(!do_SDL_refresh) continue;
        int v1=*(int*)p[u][v];
        int v2=*(int*)p[u][v+total_size-1];
        for(int i=0;i<pixel_size;i++){
            int *p2=(int*)p[u+i][v];
            for(int j=0;j<pixel_size;j++){
                *(p2++) = v1;
            }
        }
        for(int i=0;i<gap_size;i++){
          for(int j=0;j<total_size;j++){
              *(int*)p[u+j][v+pixel_size+i] = v2;
              *(int*)p[u+pixel_size+i][v+j] = v2;
          }
        }
      }
    }
  }
  if(!do_SDL_refresh) return;
  memcpy(bytes,lcd_effect_buffer,SCREEN_HEIGHT*total_size* SCREEN_WIDTH*total_size * 4);

  SDL_UnlockTexture(texture);

    static SDL_Rect destination =
    { LEFT_GAP* lcd_scale *total_size + LEFT_GAP_EXTRA*lcd_scale, 0, SCREEN_WIDTH* lcd_scale *total_size, SCREEN_HEIGHT * lcd_scale *total_size };

  static SDL_Rect source2 = { 1*total_size, 0, (SCREEN_WIDTH-1)*total_size, SCREEN_HEIGHT*total_size };
  static SDL_Rect destination2 =
    { LEFT_GAP* lcd_scale *total_size + LEFT_GAP_EXTRA*lcd_scale, 0, (SCREEN_WIDTH -1)* lcd_scale *total_size, SCREEN_HEIGHT * lcd_scale *total_size };
  if(console_on) SDL_RenderCopy(renderer, texture, &source, &destination);
  else SDL_RenderCopy(renderer, texture, &source2, &destination2);

  SDL_RenderPresent(renderer);
  SDL_DestroyTexture(texture);
}
