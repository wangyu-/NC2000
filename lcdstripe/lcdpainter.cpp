//#include <windows.h>
//#include <gdiplus.h>
#include <cstdio>
#include <fcntl.h>
#include <assert.h>
//#include <io.h>

#include "lcdpainter.h"
#include "json.h"
#include "comm.h"

struct TStripeMiscInfo
{
    struct {
        int sevenseg;
        int line;
        bool oddline;
    } gap;
    struct {
        int width;
        int height;
    } lcd;
};

TLCDStripe getCoordInfoWithKey(json::jobject& json, const char* key);
TStripeMiscInfo getStripeMiscInfo(json::jobject& json);
void quickdump(unsigned int addr, const unsigned char *data, unsigned int amount);

MyLCDView::MyLCDView(const char* jsonpath)
    : fLCDTexture(0)
    , fPixel{0,}
{
    initLCDStripe(jsonpath);
}

MyLCDView::~MyLCDView()
{
    if (fLCDTexture) {
        SDL_DestroyTexture(fLCDTexture);
    }
}

uint32_t getpixel(SDL_Surface *surface, int x, int y)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to retrieve */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch (bpp)
    {
        case 1:
            return *p;
            break;

        case 2:
            return *(Uint16 *)p;
            break;

        case 3:
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {return p[0] << 16 | p[1] << 8 | p[2];}
            else {return p[0] | p[1] << 8 | p[2] << 16;}
            break;

        case 4:
            return *(uint32_t *)p;
            break;

        default:
            assert(false);
            return 0;       /* shouldn't happen, but avoids warnings */
    }
}

void MyLCDView::loadStripeTexture(const char * texpath, SDL_Renderer* render)
{
    //original BMP should be SDL_PIXELFORMAT_ARGB8888
    //ARGB means A in MSB
    SDL_Surface* surface = SDL_LoadBMP(texpath); 
    if (surface == NULL) {
        printf("file %s open failed: %s\n", texpath, SDL_GetError());
        exit(-1);
    }
    if(debug_level>=1){
        const char* formatName = SDL_GetPixelFormatName(surface->format->format);
        printf("pixel format=%s, bpp=%d\n", formatName, surface->format->BytesPerPixel);
    }

    int width = surface->w;
    int height = surface->h;
    printf("lcdstripe: w %d h %d\n", width, height);
    
    fLCDTexture = SDL_CreateTexture(render, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
    void* buffer = malloc(width * height * 4);
    uint32_t* bufptr = (uint32_t*)buffer;

    for(int i = 0; i < height; i++){
        for(int j=0;j<width;j++){
            uint32_t color= getpixel(surface, j, i);
            //printf("%08x ",color);
            /*printf("R: %d G: %d B: %d A: %d\n", 
                (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, (color >> 24) & 0xFF);*/
            int alpha = (color >> 24) & 0xFF;
            //if(alpha<0x20) alpha*=0.66; //manually turn icons lighter
            int r = (color >> 16) & 0xFF;
            int g = (color >> 8) & 0xFF;
            int b = color & 0xFF;
            r=g=b=(r+g+b)/3; // turn into grayscale
            r = 255-((255-r) * alpha) / 255;  //apply alpha into color
            g = 255-((255-g) * alpha) / 255;
            b = 255-((255-b) * alpha) / 255;
            if(alpha!=0) alpha=0xff;
            r*=r_scale; 
            b*=b_scale;
            g*=g_scale;
            color = (alpha << 24) | (r << 16) | (g << 8) | b;
            *bufptr = color << 8 | color >> 24; //turn ARGB to RGBA
            bufptr++;   
        }
    }
    SDL_UpdateTexture(fLCDTexture, NULL, buffer, width * 4);

    /*Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    Gdiplus::Bitmap* bitmap = new Gdiplus::Bitmap(texpath);
    if (bitmap) {
        int width = bitmap->GetWidth();
        int height = bitmap->GetHeight();
        void* buffer = malloc(width * height * 4);
        uint32_t* bufptr = (uint32_t*)buffer;
        fLCDTexture = SDL_CreateTexture(render, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                Gdiplus::Color pixel;
                if (bitmap->GetPixel(x, y, &pixel) == Gdiplus::Ok) {
                    Gdiplus::ARGB color = pixel.GetValue();
                    *bufptr = color << 8 | color >> 24; // ARGB to RGBA?
                }
                bufptr++;
            }
        }
        SDL_UpdateTexture(fLCDTexture, NULL, buffer, width * 4);
        free(buffer);
        delete bitmap;
    }
    Gdiplus::GdiplusShutdown(gdiplusToken);*/
}

#define COPYMOVESSD(dst, src, step) fLCDStripes[dst] = fLCDStripes[src]; fLCDStripes[dst].left += step

void MyLCDView::initLCDStripe(const char* jsonpath)
{
    FILE *f = fopen(jsonpath, "rb");
    if(f==0) {
        printf("file %s not exist!\n", jsonpath);
        exit(-1);
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  /* same as rewind(f); */
    char* jsontext = (char*)malloc(fsize + 1);
    fread(jsontext, fsize, 1, f);
    jsontext[fsize] = 0;
    fclose(f);
    /*struct _stat64 st;
    if (_wstat64(jsonpath, &st) == -1) {
        return;
    }
    char* jsontext = (char*)malloc(st.st_size + 1);
    int fd = _wopen(jsonpath, O_RDONLY);
    int readed = _read(fd, jsontext, st.st_size);
    _close(fd);*/
    //jsontext[readed] = 0;
    json::jobject json = json::jobject::parse(jsontext);
    free(jsontext);

    TLCDStripe Line    = getCoordInfoWithKey(json, "lcd_line"); // odd
    TLCDStripe Line2   = getCoordInfoWithKey(json, "lcd_line5"); // even
    TLCDStripe VertBar = getCoordInfoWithKey(json, "lcd_vertbar");
    TLCDStripe HonzBar = getCoordInfoWithKey(json, "lcd_hbar");
    TLCDStripe Pixel   = getCoordInfoWithKey(json, "lcdpixel");
    fLCDPixel = Pixel.texture;
    fLCDEmpty = getCoordInfoWithKey(json, "lcdoverlap").texture;
    TStripeMiscInfo gap = getStripeMiscInfo(json);

    // 左上角数码管
    fLCDStripes[0]  = getCoordInfoWithKey(json, "lcd_seven_vert3"); //F
    fLCDStripes[1]  = getCoordInfoWithKey(json, "lcd_seven_honz1"); // A
    fLCDStripes[2]  = getCoordInfoWithKey(json, "lcd_seven_vert1"); //B
    fLCDStripes[3]  = getCoordInfoWithKey(json, "lcd_seven_honz2"); // G
    fLCDStripes[33] = getCoordInfoWithKey(json, "lcd_seven_vert2"); //C
    fLCDStripes[34] = getCoordInfoWithKey(json, "lcd_seven_honz3"); // D
    fLCDStripes[35] = getCoordInfoWithKey(json, "lcd_seven_vert4"); //E
    int sevenstep = gap.gap.sevenseg;
    // 第二列坐标平移
    COPYMOVESSD(5, 0, sevenstep);
    COPYMOVESSD(6, 1, sevenstep);
    COPYMOVESSD(7, 2, sevenstep);
    COPYMOVESSD(8, 3, sevenstep);
    COPYMOVESSD(29, 33, sevenstep);
    COPYMOVESSD(30, 34, sevenstep);
    COPYMOVESSD(31, 35, sevenstep);
    // 第三列
    COPYMOVESSD(10, 5, sevenstep);
    COPYMOVESSD(11, 6, sevenstep);
    COPYMOVESSD(13, 7, sevenstep);
    COPYMOVESSD(14, 8, sevenstep);
    COPYMOVESSD(24, 29, sevenstep);
    COPYMOVESSD(25, 30, sevenstep);
    COPYMOVESSD(26, 31, sevenstep);
    // 第四列
    COPYMOVESSD(15, 10, sevenstep);
    COPYMOVESSD(16, 11, sevenstep);
    COPYMOVESSD(17, 13, sevenstep);
    COPYMOVESSD(18, 14, sevenstep);
    COPYMOVESSD(19, 24, sevenstep);
    COPYMOVESSD(21, 25, sevenstep);
    COPYMOVESSD(22, 26, sevenstep);

    fLCDStripes[32] = getCoordInfoWithKey(json, "lcd_point");
    fLCDStripes[9]  = getCoordInfoWithKey(json, "lcd_semicolon");
    COPYMOVESSD(27, 32, sevenstep); // Point
    COPYMOVESSD(23, 27, sevenstep);

    // right lines, isodd?
    if (gap.gap.oddline) {
        // two rows?
        int linegap = gap.gap.line;
        fLCDStripes[4]  = Line;  Line.top  += linegap;
        fLCDStripes[12] = Line2; Line2.top += linegap;
        fLCDStripes[20] = Line;  Line.top  += linegap;
        fLCDStripes[28] = Line2; Line2.top += linegap;
        fLCDStripes[36] = Line;  Line.top  += linegap;
        fLCDStripes[44] = Line2; Line2.top += linegap;
        fLCDStripes[52] = Line;  Line.top  += linegap;
        fLCDStripes[60] = Line2; Line2.top += linegap;
        fLCDStripes[68] = Line;  Line.top  += linegap;
        fLCDStripes[70] = getCoordInfoWithKey(json, "lcd_right");
        fLCDStripes[74] = Line2; // hmm??
    } else {
        int linegap = gap.gap.line; // same gap on each line
        fLCDStripes[4]  = Line; Line.top += linegap;
        fLCDStripes[12] = Line; Line.top += linegap;
        fLCDStripes[20] = Line; Line.top += linegap;
        fLCDStripes[28] = Line; Line.top += linegap;
        fLCDStripes[36] = Line; Line.top += linegap;
        fLCDStripes[44] = Line; Line.top += linegap;
        fLCDStripes[52] = Line; Line.top += linegap;
        fLCDStripes[60] = Line; Line.top += linegap;
        fLCDStripes[68] = Line; Line.top += linegap;
        fLCDStripes[70] = getCoordInfoWithKey(json, "lcd_right");
        fLCDStripes[74] = Line;
    }

    fLCDStripes[38] = getCoordInfoWithKey(json, "lcd_pgup"); // PageUp
    fLCDStripes[37] = getCoordInfoWithKey(json, "lcd_star"); //Star;
    fLCDStripes[39] = getCoordInfoWithKey(json, "lcd_num"); //Num;
    fLCDStripes[40] = getCoordInfoWithKey(json, "lcd_eng"); //Eng;
    fLCDStripes[41] = getCoordInfoWithKey(json, "lcd_caps"); //Caps;
    fLCDStripes[42] = getCoordInfoWithKey(json, "lcd_shift"); //Shift;
    fLCDStripes[46] = getCoordInfoWithKey(json, "lcd_flash"); //Flash;
    fLCDStripes[47] = getCoordInfoWithKey(json, "lcd_sound"); //Sound;
    fLCDStripes[48] = getCoordInfoWithKey(json, "lcd_keyclick"); //KeyClick;
    fLCDStripes[51] = getCoordInfoWithKey(json, "lcd_sharpbell"); //SharpBell;
    fLCDStripes[50] = getCoordInfoWithKey(json, "lcd_speaker"); //Speaker;
    fLCDStripes[49] = getCoordInfoWithKey(json, "lcd_alarm"); //Alarm;
    fLCDStripes[53] = getCoordInfoWithKey(json, "lcd_microphone"); //Microphone;
    fLCDStripes[54] = getCoordInfoWithKey(json, "lcd_tape"); //Tape;
    fLCDStripes[55] = getCoordInfoWithKey(json, "lcd_minus"); //Minus;
    fLCDStripes[58] = getCoordInfoWithKey(json, "lcd_battery"); //Battery;
    fLCDStripes[59] = getCoordInfoWithKey(json, "lcd_secret"); //Secret;
    fLCDStripes[61] = getCoordInfoWithKey(json, "lcd_pgleft"); //PageLeft;
    fLCDStripes[62] = getCoordInfoWithKey(json, "lcd_pgright"); //PageRight;
    fLCDStripes[63] = getCoordInfoWithKey(json, "lcd_left"); //Left;
    fLCDStripes[64] = getCoordInfoWithKey(json, "lcd_pgdown"); //PageDown;

    // vertframe
    int vbargap = VertBar.texture.h;
    int hbargap = HonzBar.texture.w;
    fLCDStripes[65] = getCoordInfoWithKey(json, "lcd_vframe"); //VertFrame;
    fLCDStripes[79] = getCoordInfoWithKey(json, "lcd_up"); //Up;
    fLCDStripes[43] = VertBar; VertBar.top += vbargap;
    fLCDStripes[45] = VertBar; VertBar.top += vbargap;
    fLCDStripes[56] = VertBar; VertBar.top += vbargap;
    fLCDStripes[78] = VertBar; VertBar.top += vbargap;
    fLCDStripes[77] = VertBar; VertBar.top += vbargap;
    fLCDStripes[57] = VertBar; VertBar.top += vbargap;
    fLCDStripes[76] = VertBar; VertBar.top += vbargap;
    fLCDStripes[75] = VertBar; VertBar.top += vbargap;
    fLCDStripes[73] = VertBar;
    fLCDStripes[66] = getCoordInfoWithKey(json, "lcd_down"); //Down;
    fLCDStripes[72] = getCoordInfoWithKey(json, "lcd_hframe"); //HonzFrame;
    fLCDStripes[67] = HonzBar; HonzBar.left += hbargap;
    fLCDStripes[69] = HonzBar; HonzBar.left += hbargap;
    fLCDStripes[71] = HonzBar;

    fLCDWidth = gap.lcd.width;
    fLCDHeight = gap.lcd.height;

    fLCDPixelPoint.x = Pixel.left;
    fLCDPixelPoint.y = Pixel.top;
}

void MyLCDView::setPixel(int x, int y, unsigned char value)
{
    fPixel[y * 160 + x] = value;
}

void MyLCDView::paint(SDL_Renderer* render, bool lcdon, bool draw_stripe)
{
    SDL_SetTextureBlendMode(fLCDTexture, SDL_BLENDMODE_NONE);
    //SDL_SetRenderDrawColor(render, 0xFF, 0xFD, 0xE8, 0xFF);
    SDL_SetRenderDrawColor(render, 0xFF*r_scale, 0xFF*g_scale, 0xFF*b_scale, 0xFF);
    SDL_RenderClear(render);
    SDL_SetTextureBlendMode(fLCDTexture, SDL_BLENDMODE_BLEND);
    if (lcdon) {
        auto a=SDL_Rect{ 0, 0, fLCDEmpty.w,fLCDEmpty.h };
        SDL_RenderCopy(render, fLCDTexture, &fLCDEmpty, &a);
    }

    if(!draw_stripe) return;

    for (int y = 79; y >= 0; y--) {
        unsigned char pixel = fPixel[160 * y];
        int alpha =0;
        if(pixel==3) {
            alpha = 255;
        } else if (pixel==2) {
            alpha = 170;
        } else if (pixel==1) {
            alpha = 85;
        }else if (pixel==0) {
            alpha = 0;
        }else{
            assert(false);
        }

        if (pixel) {
            SDL_SetTextureAlphaMod(fLCDTexture, alpha); 
            TLCDStripe* item = &fLCDStripes[y];
            auto a=SDL_Rect{item->left,item->top,item->texture.w,item->texture.h};
            SDL_RenderCopy(render, fLCDTexture, &item->texture, &a);
            SDL_SetTextureAlphaMod(fLCDTexture, 255); //in case forget restore
        }
    }
    SDL_Rect dest{ fLCDPixelPoint.x, fLCDPixelPoint.y, fLCDPixel.w, fLCDPixel.h };
    
    if(false) for (int y = 0; y < 80; y++) {
        for (int x = 1; x < 160; x++) {
            bool pixel = fPixel[160 * y + x];
            if (pixel) {
                SDL_RenderCopy(render, fLCDTexture, &fLCDPixel, &dest);
            }
            dest.x += fLCDPixel.w;
        }
        dest.x = fLCDPixelPoint.x;
        dest.y += fLCDPixel.h;
    }
}

int MyLCDView::getLCDWidth()
{
    //printf("real width %d\n", fLCDWidth);
    return fLCDWidth;
}

int MyLCDView::getLCDHeight()
{
    //printf("real height %d\n", fLCDHeight);
    return fLCDHeight;
}

TStripeMiscInfo getStripeMiscInfo(json::jobject& json)
{
    TStripeMiscInfo info;
    json::jobject misc = json["misc"];
    if (misc.has_key("gap")) {
        json::jobject gap = misc["gap"];
        info.gap.sevenseg = gap["7seg"];
        info.gap.line = gap["line"];
        info.gap.oddline = gap["oddline"].is_true();
    }
    if (misc.has_key("lcd")) {
        json::jobject lcd = misc["lcd"];
        info.lcd.width = lcd["w"];
        info.lcd.height = lcd["h"];
    }
    return info;
}

TLCDStripe getCoordInfoWithKey(json::jobject& json, const char* key)
{
    TLCDStripe stripe;
    json::jobject sliceframe = json[key];
    if (sliceframe.has_key("slice")) {
        json::jobject slice = sliceframe["slice"];
        stripe.left = slice["x"];
        stripe.top = slice["y"];
    }
    if (sliceframe.has_key("frame")) {
        json::jobject frame = sliceframe["frame"];
        stripe.texture.x = frame["x"];
        stripe.texture.y = frame["y"];
        stripe.texture.w = frame["w"];
        stripe.texture.h = frame["h"];
    }
    return stripe;
}
