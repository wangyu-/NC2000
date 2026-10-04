#ifndef _LCD_PAINTER_H
#define _LCD_PAINTER_H

#include <SDL.h>
#include <stdint.h>

typedef struct tagLCDStripe {
    SDL_Rect texture;
    int left, top;
} TLCDStripe;

class MyLCDView
{
public:
    MyLCDView(const char* jsonpath);
    ~MyLCDView();
private:
    TLCDStripe fLCDStripes[80];
    SDL_Point fLCDPixelPoint;
    SDL_Rect fLCDPixel, fLCDEmpty;
    SDL_Texture* fLCDTexture;
    int fTextureWidth, fTextureHeight;
    int fLCDWidth, fLCDHeight;
    unsigned char fPixel[160*80]; // TODO: bit or gray
    unsigned char fPixelNC3000[80]={}; // NC3000 compatibility layer
public:
    void loadStripeTexture(const char * texpath, SDL_Renderer* render);
    void setPixel(int x, int y, unsigned char);
    void paint(SDL_Renderer* render, bool lcdon, bool draw_stripe);
    int getLCDWidth();
    int getLCDHeight();
private:
    void initLCDStripe(const char * jsonpath);
};

#endif
