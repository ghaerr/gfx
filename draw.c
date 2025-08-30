/*
 * SDL text and graphics console emulator.
 * For use in validating draw routines for all supported pixel formats.
 *
 * Currently just displays bitmap glyphs echoed from keypresses.
 * Newline will result in scrolling screen.
 *
 * Sep 2022 Greg Haerr <greg@censoft.com>
 * Aug 2025 Greg Haerr rewritten
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include "font.h"

/* supported internal framebuffer pixel formats */
#define MWPF_DEFAULT       MWPF_TRUECOLORARGB
#define MWPF_TRUECOLORARGB 0    /* 32bpp, memory byte order B, G, R, A */
#define MWPF_TRUECOLORABGR 1    /* 32bpp, memory byte order R, G, B, A */

#define MIN(a,b)      ((a) < (b) ? (a) : (b))
#define MAX(a,b)      ((a) > (b) ? (a) : (b))

typedef uint32_t Pixel;     /* internal pixel format (ARGB or ABGR) */
typedef uint8_t Alpha;      /* size of alpha channel components */

typedef struct point {
    int x, y;
} Point;

typedef struct rect {
    int x, y;
    int w, h;
} Rect;

struct palentry {           /* palette entry */
    unsigned char r, g, b, a;
};

typedef struct drawable {
    int pixtype;            /* pixel format */
    int bpp;                /* bits per pixel */
    int bytespp;            /* bytes per pixel */
    int width;              /* width in pixels */
    int height;             /* height in pixels */
    int pitch;              /* stride in bytes, offset to next pixel row */
    Pixel color;            /* rgb color for glyph blit mode */
    void *window;           /* opaque pointer for associated window */
    int32_t size;           /* total size in bytes */
    uint8_t *pixels;        /* pixel data, normally points to data[] below */
    Pixel data[];           /* drawable memory allocated in single malloc */
} Drawable, Texture;

struct console {
    /* configurable parameters */
    int cols;               /* # text columns */
    int lines;              /* # text rows */
    int char_width;         /* glyph width in pixels */
    int char_height;        /* glyph height in pixels */
    Font *font;             /* associated font */

    int curx;               /* cursor x position */
    int cury;               /* cursor y position */
    int lastx;
    int lasty;
    Rect update;            /* console update region in cols/lines coordinates */
    Drawable *dp;            //FIXME for testing only
    uint16_t text_ram[];    /* adaptor RAM (= cols * lines * 2) in single malloc */
};

/*
 * Conversion from 8-bit RGB components to pixel value
 */
/* create 32 bit 8/8/8/8 format pixel (0xAARRGGBB) from RGB triplet*/
#define RGB2PIXELARGB(r,g,b)    \
    (0xFF000000UL | ((r) << 16) | ((g) << 8) | (b))

/* create 32 bit 8/8/8/8 format pixel (0xAABBGGRR) from RGB triplet*/
#define RGB2PIXELABGR(r,g,b)    \
    (0xFF000000UL | ((b) << 16) | ((g) << 8) | (r))

#define RGBDEF(r,g,b)   {r, g, b, 0}

/* palette tables used for console attribute color conversions */
#if 1
/*
 * CGA palette for 16 color systems.
 */
static struct palentry ega_colormap[16] = {
    RGBDEF( 0   , 0   , 0    ), /* black*/
    RGBDEF( 0   , 0   , 0xAA ), /* blue*/
    RGBDEF( 0   , 0xAA, 0    ), /* green*/
    RGBDEF( 0   , 0xAA, 0xAA ), /* cyan*/
    RGBDEF( 0xAA, 0   , 0    ), /* red*/
    RGBDEF( 0xAA, 0   , 0xAA ), /* magenta*/
    RGBDEF( 0xAA, 0x55, 0    ), /* brown*/
    RGBDEF( 0xAA, 0xAA, 0xAA ), /* ltgray*/
    RGBDEF( 0x55, 0x55, 0x55 ), /* gray*/
    RGBDEF( 0x55, 0x55, 0xFF ), /* ltblue*/
    RGBDEF( 0x55, 0xFF, 0x55 ), /* ltgreen*/
    RGBDEF( 0x55, 0xFF, 0xFF ), /* ltcyan*/
    RGBDEF( 0xFF, 0x55, 0x55 ), /* ltred*/
    RGBDEF( 0xFF, 0x55, 0xFF ), /* ltmagenta*/
    RGBDEF( 0xFF, 0xFF, 0x55 ), /* yellow*/
    RGBDEF( 0xFF, 0xFF, 0xFF ), /* white*/
};
#elif 0
/*
 * Standard palette for 16 color systems.
 */
static struct palentry ega_colormap[16] = {
    /* 16 EGA colors, arranged in VGA standard palette order*/
    RGBDEF( 0  , 0  , 0   ),    /* black*/
    RGBDEF( 0  , 0  , 128 ),    /* blue*/
    RGBDEF( 0  , 128, 0   ),    /* green*/
    RGBDEF( 0  , 128, 128 ),    /* cyan*/
    RGBDEF( 128, 0  , 0   ),    /* red*/
    RGBDEF( 128, 0  , 128 ),    /* magenta*/
    RGBDEF( 128, 64 , 0   ),    /* adjusted brown*/
    RGBDEF( 192, 192, 192 ),    /* ltgray*/
    RGBDEF( 128, 128, 128 ),    /* gray*/
    RGBDEF( 0  , 0  , 255 ),    /* ltblue*/
    RGBDEF( 0  , 255, 0   ),    /* ltgreen*/
    RGBDEF( 0  , 255, 255 ),    /* ltcyan*/
    RGBDEF( 255, 0  , 0   ),    /* ltred*/
    RGBDEF( 255, 0  , 255 ),    /* ltmagenta*/
    RGBDEF( 255, 255, 0   ),    /* yellow*/
    RGBDEF( 255, 255, 255 ),    /* white*/
};
#elif 0
/* 16 color palette for attribute mapping */
static struct palentry ega_colormap[16] = {
    RGBDEF( 0  , 0  , 0   ),    /* 0 black*/
    RGBDEF( 0  , 0  , 192 ),    /* 1 blue*/
    RGBDEF( 0  , 192, 0   ),    /* 2 green*/
    RGBDEF( 0  , 192, 192 ),    /* 3 cyan*/
    RGBDEF( 192, 0  , 0   ),    /* 4 red*/
    RGBDEF( 192, 0  , 192 ),    /* 5 magenta*/
    RGBDEF( 192, 128 , 0  ),    /* 6 adjusted brown*/
    RGBDEF( 192, 192, 192 ),    /* 7 ltgray*/
    RGBDEF( 128, 128, 128 ),    /* gray*/
    RGBDEF( 0  , 0  , 255 ),    /* ltblue*/
    RGBDEF( 0  , 255, 0   ),    /* 10 ltgreen*/
    RGBDEF( 0  , 255, 255 ),    /* ltcyan*/
    RGBDEF( 255, 0  , 0   ),    /* ltred*/
    RGBDEF( 255, 0  , 255 ),    /* ltmagenta*/
    RGBDEF( 255, 255, 0   ),    /* yellow*/
    RGBDEF( 255, 255, 255 ),    /* 15 white*/
};
#endif

struct sdl_window {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    float zoom;
};

int sdl_init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        printf("Can't initialize SDL\n");
        return 0;
    }
    return 1;
}

struct sdl_window *sdl_create_window(Drawable *dp)
{
    struct sdl_window *sdl;
    int pixelformat;

    sdl = malloc(sizeof(struct sdl_window));
    if (!sdl) return 0;

    sdl->zoom = 1.0;
    sdl->window = SDL_CreateWindow("Graphics Console",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        dp->width*sdl->zoom, dp->height*sdl->zoom,
        SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if (!sdl->window) {
        printf("SDL: Can't create window\n");
        return 0;
    }

    sdl->renderer = SDL_CreateRenderer(sdl->window, -1, 0);
    if (!sdl->renderer) {
        printf("SDL: Can't create renderer\n");
        return 0;
    }

    /* 
     * Set the SDL texture pixel format to match the framebuffer format
     * to eliminate pixel conversions.
     */
    switch (dp->pixtype) {
    case MWPF_TRUECOLORARGB:
        pixelformat = SDL_PIXELFORMAT_ARGB8888;
        break;
    case MWPF_TRUECOLORABGR:
        pixelformat = SDL_PIXELFORMAT_ABGR8888;
        break;
    default:
        printf("SDL: Unsupported pixel format %d\n", dp->pixtype);
        return 0;
    }

    sdl->texture = SDL_CreateTexture(sdl->renderer, pixelformat,
        SDL_TEXTUREACCESS_STREAMING, dp->width, dp->height);
    if (!sdl->texture) {
        printf("SDL: Can't create texture\n");
        return 0;
    }
    dp->window = sdl;

    SDL_RenderSetLogicalSize(sdl->renderer, dp->width, dp->height);
    SDL_RenderSetScale(sdl->renderer, sdl->zoom, sdl->zoom);
    SDL_SetRenderDrawColor(sdl->renderer, 0x00, 0x00, 0x00, 0x00);
    //SDL_ShowCursor(SDL_DISABLE);    /* hide SDL cursor */
    SDL_PumpEvents();

    return sdl;
}

/* update SDL from drawable */
void sdl_draw(Drawable *dp, int x, int y, int width, int height)
{
    struct sdl_window *sdl = (struct sdl_window *)dp->window;
    SDL_Rect r;

    r.x = x;
    r.y = y;
    r.w = width? width: dp->width;
    r.h = height? height: dp->height;
    unsigned char *pixels = dp->pixels + y * dp->pitch + x * dp->bytespp;
    SDL_UpdateTexture(sdl->texture, &r, pixels, dp->pitch);

    /* copy texture to display*/
    SDL_RenderClear(sdl->renderer);
    SDL_RenderCopy(sdl->renderer, sdl->texture, NULL, NULL);
    SDL_RenderPresent(sdl->renderer);
}

/* convert keycode to shift value */
static SDL_Keycode key_shift(SDL_Keycode kc)
{
    if (kc >= 'a' && kc < 'z')
        return (kc ^ 0x20);  // upper case

    switch (kc) {
        case '`':  return '~';
        case '1':  return '!';
        case '2':  return '@';
        case '3':  return '#';
        case '4':  return '$';
        case '5':  return '%';
        case '6':  return '^';
        case '7':  return '&';
        case '8':  return '*';
        case '9':  return '(';
        case '0':  return ')';
        case '-':  return '_';
        case '=':  return '+';
        case '[':  return '{';
        case ']':  return '}';
        case '\\': return '|';
        case ';':  return ':';
        case '\'': return '"';
        case ',':  return '<';
        case '.':  return '>';
        case '/':  return '?';

        default: break;
    }

    return kc;
}

static int sdl_key(Uint8 state, SDL_Keysym sym)
{
    SDL_Scancode sc = sym.scancode;
    SDL_Keycode kc = sym.sym;
    Uint16 mod = sym.mod;

    switch (sc) {
    case SDL_SCANCODE_MINUS:  kc = '-'; break;
    case SDL_SCANCODE_PERIOD: kc = '.'; break;
    case SDL_SCANCODE_SLASH:  kc = '/'; break;
    default: break;
    }

    if (kc == SDLK_LSHIFT || kc == SDLK_RSHIFT ||
        kc == SDLK_LCTRL || kc == SDLK_RCTRL)
        return 0;

    if (kc < 256 && (mod & (KMOD_SHIFT | KMOD_CAPS)))
        kc = key_shift(kc);

    if (kc < 256 && (mod & KMOD_CTRL))
        kc &= 0x1f;  // convert to control char

    if (kc == 0x007F) kc = '\b';  // convert DEL to BS

    return kc;
}

/* default display attribute (for testing)*/
//#define ATTR_DEFAULT 0x09   /* bright blue */
//#define ATTR_DEFAULT 0x0F   /* bright white */
//#define ATTR_DEFAULT 0xF0   /* reverse ltgray */
#define ATTR_DEFAULT 0x35

static int fast_sin_table[180] = {
0,   1,  2,  3,  4,  5,  6,  7,  8, 10, 11, 12, 13, 14, 15, /*  0 */
16, 17, 18, 19, 20, 21, 22, 23, 25, 26, 27, 28, 29, 30, 31, /* 15 */
31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 41, 42, 43, 44, /* 30 */
45, 46, 46, 47, 48, 49, 49, 50, 51, 51, 52, 53, 53, 54, 54, /* 45 */
55, 55, 56, 57, 57, 58, 58, 58, 59, 59, 60, 60, 60, 61, 61, /* 60 */
61, 62, 62, 62, 62, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, /* 75 */
64, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 62, 62, 62, 62, /* 90 */
61, 61, 61, 60, 60, 60, 59, 59, 58, 58, 58, 57, 57, 56, 55, /* 105 */
55, 54, 54, 53, 53, 52, 51, 51, 50, 49, 49, 48, 47, 46, 46, /* 120 */
45, 44, 43, 42, 41, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, /* 135 */
31, 31, 30, 29, 28, 27, 26, 25, 23, 22, 21, 20, 19, 18, 17, /* 150 */
16, 15, 14, 13, 12, 11, 10,  8,  7,  6,  5,  4,  3,  2,  1  /* 165 */
};

/* fast fixed point 26.6 sin/cos for angles in degrees */
static int fast_sin(int angle)
{
    //angle %= 360;
    if (angle >= 360) angle -= 360;
    else if (angle < 0) angle += 360;
    if (angle >= 180)               /* for angles >= 180 invert sign*/
        return -fast_sin_table[angle - 180];
    return fast_sin_table[angle];
}

static int fast_cos(int angle)
{
    return fast_sin(angle + 90);    /* cos is sin plus 90 degrees */
}

static int angle = 0;
static int oversamp = 24;           /* min 20 for no holes in diagonal oversampling */

/* draw a character from bitmap font */
void draw_font_bitmap(Drawable *dp, Font *font, int c, int sx, int sy, int xoff, int yoff,
    Pixel fgpixel, Pixel bgpixel, int drawbg, int rotangle)
{
    int x, y, minx, maxx, w, zerox, dspan;
    int height = font->height;
    int bitcount = 0;
    uint32_t word;
    uint32_t bitmask = 1 << ((font->bits_width << 3) - 1);  /* MSB first */
    Pixel *dst;
    Varptr bits;
    int sin_a, cos_a, s;        /* for rotated bitmaps */

    c -= font->firstchar;
    if (c < 0 || c > font->size)
        c = font->defaultchar - font->firstchar;

    /* get glyph bitmap start */
    if (font->offset.ptr8) {
        switch (font->offset_width) {
        case 1:
            bits.ptr8 = font->bits.ptr8 + font->offset.ptr8[c];
            break;
        case 2:
            bits.ptr8 = font->bits.ptr8 + font->offset.ptr16[c];
            break;
        case 4:
        default:
            bits.ptr8 = font->bits.ptr8 + font->offset.ptr32[c];
            break;
        }
    } else  bits.ptr8 = font->bits.ptr8 + c * font->bits_width * font->height;

    if (rotangle) {
        sin_a = fast_sin(rotangle);
        cos_a = fast_cos(rotangle);
    }

    x = y = 0;
    minx = x;
    w = font->width? font->width[c]: font->maxwidth;
    /* draw max font width background if proportional font and console output */
    if (drawbg == 2 && w != font->maxwidth) {
        zerox = minx + w;
        maxx = zerox + (font->maxwidth - w);
        dspan = dp->pitch - ((maxx - minx) * dp->bytespp);
    } else {
        maxx = minx + w;
        zerox = 9999;
        dspan = dp->pitch - (w * dp->bytespp);
    }
    dst = (uint32_t *)(dp->pixels + (sy+yoff) * dp->pitch + (sx+xoff) * dp->bytespp);

    do {
        if (bitcount <= 0) {
            bitcount = font->bits_width << 3;
            switch (font->bits_width) {
            case 1:
                word = *bits.ptr8++;
                break;
            case 2:
            default:
                word = *bits.ptr16++;
                break;
            case 4:
                word = *bits.ptr32++;
                break;
            }
        }

        s = 0;
        do {
            if (rotangle) {
                int dx = sx + ((cos_a * (((x+xoff) << 6) + s)
                              - sin_a * (((y+yoff) << 6) + s) + (1 << 11)) >> 12);
                int dy = sy + ((sin_a * (((x+xoff) << 6) + s)
                              + cos_a * (((y+yoff) << 6) + s) + (1 << 11)) >> 12);
                if (dx < 0 || dx >= dp->width || dy < 0 || dy >= dp->height)
                    continue;
                dst = (uint32_t *)(dp->pixels + dy * dp->pitch + dx * dp->bytespp);
            }
            else {
                int dx = sx + x+xoff;
                int dy = sy + y+yoff;
                if (dx < 0 || dx >= dp->width || dy < 0 || dy >= dp->height)
                    continue;
            }

            /* write destination pixel */
            if (word & bitmask)
                *dst = fgpixel;
            else if (drawbg)
                *dst = bgpixel;

        } while(rotangle && (s += oversamp) < oversamp+1);

        dst++;
        word <<= 1;
        --bitcount;
        if (++x == zerox) {         /* start drawing extra background bits? */
            word = 0;
            bitcount = 9999;
            continue;
        }
        if (x == maxx) {            /* finished with bitmap row? */
            x = minx;
            ++y;
            bitcount = 0;
            dst = (Pixel *)((uint8_t *)dst + dspan);
            --height;
        }
    } while (height > 0);
}

/* draw a character from an antialiasing font */
void draw_font_alpha(Drawable *dp, Font *font, int c, int sx, int sy, int xoff, int yoff,
    Pixel fgpixel, Pixel bgpixel, int drawbg, int rotangle)
{
    int x, y, minx, maxx, w, zerox, dspan;
    int height = font->height;
    Pixel *dst;
    Varptr bits;
    int sin_a, cos_a, s;        /* for rotated bitmaps */

    c -= font->firstchar;
    if (c < 0 || c > font->size)
        c = font->defaultchar - font->firstchar;

    /* get glyph alpha bytes */
    if (font->offset.ptr8) {
        switch (font->offset_width) {
        case 1:
            bits.ptr8 = font->bits.ptr8 + font->offset.ptr8[c];
            break;
        case 2:
            bits.ptr8 = font->bits.ptr8 + font->offset.ptr16[c];
            break;
        case 4:
        default:
            bits.ptr8 = font->bits.ptr8 + font->offset.ptr32[c];
            break;
        }
    } else  bits.ptr8 = font->bits.ptr8 + c * font->bits_width * font->height;

    if (rotangle) {
        sin_a = fast_sin(rotangle);
        cos_a = fast_cos(rotangle);
    }

    x = y = 0;
    minx = x;
    w = font->width? font->width[c]: font->maxwidth;
    /* draw max font width background if proportional font and console output */
    if (drawbg == 2 && w != font->maxwidth) {
        zerox = minx + w;
        maxx = zerox + (font->maxwidth - w);
        dspan = dp->pitch - ((maxx - minx) * dp->bytespp);
    } else {
        maxx = minx + w;
        zerox = 9999;
        dspan = dp->pitch - (w * dp->bytespp);
    }
    dst = (Pixel *)(dp->pixels + (sy+yoff) * dp->pitch + (sx+xoff) * dp->bytespp);

    do {
        s = 0;
        Alpha sa = (x < zerox)? *bits.ptr8++: 0;

        do {
            if (rotangle) {
                int dx = sx + ((cos_a * (((x+xoff) << 6) + s)
                              - sin_a * (((y+yoff) << 6) + s) + (1 << 11)) >> 12);
                int dy = sy + ((sin_a * (((x+xoff) << 6) + s)
                              + cos_a * (((y+yoff) << 6) + s) + (1 << 11)) >> 12);
                if (dx < 0 || dx >= dp->width || dy < 0 || dy >= dp->height)
                    continue;
                dst = (uint32_t *)(dp->pixels + dy * dp->pitch + dx * dp->bytespp);
                if (!s && sa == 255) sa = 192;  /* experimental oversampled blend */
            }
            else {
                int dx = sx + x+xoff;
                int dy = sy + y+yoff;
                if (dx < 0 || dx >= dp->width || dy < 0 || dy >= dp->height)
                    continue;
            }

            /* blend src alpha with destination */
            if (sa == 0xff) {
                *dst = fgpixel;
            } else {
                if (drawbg) *dst = bgpixel;
                if (sa != 0) {
                    Pixel srb = ((sa * (fgpixel & 0xff00ff)) >> 8) & 0xff00ff;
                    Pixel sg =  ((sa * (fgpixel & 0x00ff00)) >> 8) & 0x00ff00;
                    Pixel da = 0xff - sa;
                    Pixel drb = *dst;
                    Pixel dg = drb & 0x00ff00;
                        drb = drb & 0xff00ff;
                    drb = ((drb * da >> 8) & 0xff00ff) + srb;
                    dg =   ((dg * da >> 8) & 0x00ff00) + sg;
                    *dst = drb + dg;
                }
            }

        } while(rotangle && (s += oversamp) < oversamp+1);

        dst++;
        if (++x == zerox)
            continue;
        if (x == maxx) {            /* finished with bitmap row? */
            x = minx;
            ++y;
            dst = (Pixel *)((uint8_t *)dst + dspan);
            --height;
        }
    } while (height > 0);
}

void draw_font_char(Drawable *dp, Font *font, int c, int x, int y, int xoff, int yoff,
    Pixel fg, Pixel bg, int drawbg)
{
    if (font->bpp == 8)
        draw_font_alpha(dp, font, c, x, y, xoff, yoff, fg, bg, drawbg, angle);
    else draw_font_bitmap(dp, font, c, x, y, xoff, yoff, fg, bg, drawbg, angle);
}

/* update dirty rectangle in console coordinates */
static void update_dirty_region(struct console *con, int x, int y, int w, int h)
{
    con->update.x = MIN(x, con->update.x);
    con->update.y = MIN(y, con->update.y);
    con->update.w = MAX(con->update.w, x+w);
    con->update.h = MAX(con->update.h, y+h);
}

static void reset_dirty_region(struct console *con)
{
    con->update.x = con->update.y = 32767;
    con->update.w = con->update.h = -1;
}

/* clear line y from x1 up to and including x2 to attribute attr */
static void clear_line(struct console *con, int x1, int x2, int y, unsigned char attr)
{
    int x;

    for (x = x1; x <= x2; x++)
        con->text_ram[y * con->cols + x] = ' ' | (attr << 8);
    update_dirty_region(con, x1, y, x2-x1+1, 1);
}

/* scroll adapter RAM up from line y1 up to and including line y2 */
static void scrollup(struct console *con, int y1, int y2, unsigned char attr)
{
    unsigned char *vid = (unsigned char *)(con->text_ram + y1 * con->cols);
    int pitch = con->cols * 2;

    memcpy(vid, vid + pitch, (con->lines - y1) * pitch);
    clear_line(con, 0, con->cols-1, y2, attr);
    update_dirty_region(con, 0, y1, con->cols, y2-y1+1);
}


/* scroll adapter RAM down from line y1 up to and including line y2 */
void scrolldn(struct console *con, int y1, int y2, unsigned char attr)
{
    unsigned char *vid = (unsigned char *)(con->text_ram + (con->lines-1) * con->cols);
    int pitch = con->cols * 2;
    int y = y2;

    while (--y >= y1) {
        memcpy(vid, vid - pitch, pitch);
        vid -= pitch;
    }
    clear_line(con, 0, con->cols-1, y1, attr);
    update_dirty_region(con, 0, 0, con->cols, y2-y1+1);
}

void console_movecursor(struct console *con, int x, int y)
{
    update_dirty_region(con, con->lastx, con->lasty, 1, 1);
    con->curx = con->lastx = x;
    con->cury = con->lasty = y;
    update_dirty_region(con, x, y, 1, 1);
}

static void clear_screen(Drawable *dp, struct console *con)
{
    memset(dp->pixels, 0, dp->size);
    update_dirty_region(con, 0, 0, con->cols, con->lines);
}

/* output character at cursor location*/
void console_textout(struct console *con, int c, int attr)
{
    switch (c) {
    case '\0':  return;
    case '\b':  if (--con->curx < 0) con->curx = 0; goto update;
    case '\r':  con->curx = 0; goto update;
    case '\n':  goto scroll;
    //case '-':   scrolldn(con, 0, con->lines-1, ATTR_DEFAULT); return;   //FIXME
    //case '=':   scrollup(con, 0, con->lines-1, ATTR_DEFAULT); return;   //FIXME
    case '-':   angle--; clear_screen(con->dp, con); return;
    case '=':   angle++; clear_screen(con->dp, con); return;
    case ':':   if (--oversamp <= 0) oversamp = 1; clear_screen(con->dp, con);
                printf("%d\n", oversamp);
                return;
    case ';':   ++oversamp;                        clear_screen(con->dp, con);
                printf("%d\n", oversamp);
                return;
    }

    con->text_ram[con->cury * con->cols + con->curx] = (c & 255) | ((attr & 255) << 8);
    update_dirty_region (con, con->curx, con->cury, 1, 1);

    if (++con->curx >= con->cols) {
        con->curx = 0;
scroll:
        if (++con->cury >= con->lines) {
            scrollup(con, 0, con->lines - 1, ATTR_DEFAULT);
            con->cury = con->lines - 1;
        }
    }

update:
    console_movecursor(con, con->curx, con->cury);
}

/* convert EGA attribute to pixel value */
static void color_from_attr(Drawable *dp, unsigned int attr, Pixel *pfg, Pixel *pbg)
{
    int fg = attr & 0x0F;
    int bg = (attr & 0x70) >> 4;
    unsigned char fg_red = ega_colormap[fg].r;
    unsigned char fg_green = ega_colormap[fg].g;
    unsigned char fg_blue = ega_colormap[fg].b;
    unsigned char bg_red = ega_colormap[bg].r;
    unsigned char bg_green = ega_colormap[bg].g;
    unsigned char bg_blue = ega_colormap[bg].b;
    Pixel fgpixel, bgpixel;

    switch (dp->pixtype) {
    case MWPF_TRUECOLORARGB:    /* byte order B G R A */
        fgpixel = RGB2PIXELARGB(fg_red, fg_green, fg_blue);
        bgpixel = RGB2PIXELARGB(bg_red, bg_green, bg_blue);
        break;
    case MWPF_TRUECOLORABGR:    /* byte order R G B A */
        fgpixel = RGB2PIXELABGR(fg_red, fg_green, fg_blue);
        bgpixel = RGB2PIXELABGR(bg_red, bg_green, bg_blue);
        break;
    }
    *pfg = fgpixel;
    *pbg = bgpixel;
}

/* draw characters from console text RAM */
static void draw_console_ram(Drawable *dp, struct console *con, int x1, int y1,
    int sx, int sy, int ex, int ey)
{
    uint16_t *vidram = con->text_ram;
    Pixel fg, bg;

    for (int y = sy; y < ey; y++) {
        int j = y * con->cols + sx;
        for (int x = sx; x < ex; x++) {
            uint16_t chattr = vidram[j];
            color_from_attr(dp, chattr >> 8, &fg, &bg);
            draw_font_char(dp, con->font, chattr & 255, x1, y1,
                x * con->char_width, y * con->char_height, fg, bg, 2);
            j++;
        }
    }
}

/* Called periodically from the main loop */
void draw_console(struct console *con, Drawable *dp, int x, int y, int flush)
{
    Pixel fg, bg;

    con->dp = dp;   // FIXME for testing w/clear_screen()

    if (con->update.w >= 0 || con->update.h >= 0) {
        /* draw text bitmaps from adaptor RAM */
        draw_console_ram(dp, con, x, y,
            con->update.x, con->update.y, con->update.w, con->update.h);

        /* draw cursor */
        color_from_attr(dp, ATTR_DEFAULT, &fg, &bg);
        draw_font_char(dp, con->font, '_', x, y,
            con->curx * con->char_width, con->cury * con->char_height, fg, bg, 0);

        if (flush) {
            sdl_draw(dp,
                x + con->update.x * con->char_width,
                y + con->update.y * con->char_height,
                (con->update.w - con->update.x) * con->char_width,
                (con->update.h - con->update.y) * con->char_height);
        }
        reset_dirty_region(con);
    }
}

#define ARRAYLEN(a)     (sizeof(a)/sizeof(a[0]))

extern Font font_rom8x16;
extern Font font_cour_32;
extern Font font_cour_32_tt;
extern Font font_times_32;
extern Font font_times_32_tt;
extern Font font_lucida_32;
extern Font font_lucida_32_tt;

static Font *fonts[] = {
    &font_rom8x16,              /* first font is default font */
    &font_cour_32,
    &font_cour_32_tt,
    &font_times_32,
    &font_times_32_tt,
    &font_lucida_32,
    &font_lucida_32_tt,
};

Font *font_load_internal_font(char *name)
{
    int i;

    for (i = 0; i < ARRAYLEN(fonts); i++) {
        if (!strcmp(fonts[i]->name, name))
            return fonts[i];
    }
    return NULL;
}

/*
 * load console font, works for:
 *  *.Fnn ROM font files e.g. VGA-ROM.F16, DOSJ-437.F19
 */
Font *font_load_disk_font(char *path)
{
    int width = 8;
    int height = 16;
    int fd, size;
    char *s;
    Font *font;

    s = strrchr(path, '.');
    if (s && s[1] == 'F')
        height = atoi(s+2);
    size = height * 256;
    if (width > 8) size *= 2;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        printf("Can't open %s\n", path);
        return 0;
    }
    printf("Loading %s %dx%d size %d\n", path, width, height, size);
    font = malloc(sizeof(Font)+size);
    memset(font, 0, sizeof(Font));
    if (read(fd, font->data, size) != size) {
        printf("read error");
        return 0;
    }
    close(fd);

    font->maxwidth = width;
    font->height = height;
    font->size = 256;
    font->bits.ptr8 = font->data;
    font->bits_width = (width > 8)? 2: 1;
    return font;
}

Font *console_load_font(struct console *con, char *path)
{
    Font *font = NULL;

    if (path) font = font_load_internal_font(path);
    if (!font) {
        if (path) font = font_load_disk_font(path);
        if (!font) {
            if (path)
                printf("Can't find font '%s', using default %s\n", path, fonts[0]->name);
            font = fonts[0];
        }
    }

    /* older Microwindows MWCFONT struct compatibility */
    if (font->bpp == 0)          font->bpp = 1;          /* mwin default 1 bpp */
    if (font->bits_width == 0)   font->bits_width = 2;   /* mwin default 16 bits */
    if (font->offset_width == 0) font->offset_width = 4; /* mwin default 32 bits */

    con->font = font;
    con->char_height = font->height;
    con->char_width = font->maxwidth;
    return font;
}

struct console *create_console(int width, int height)
{
    struct console *con;
    int size, i;

    size = sizeof(struct console) + width * height * sizeof(uint16_t);
    con = malloc(size);
    if (!con) return 0;

    memset(con, 0, sizeof(struct console));
    con->cols = width;
    con->lines = height;
    console_load_font(con, NULL);       /* loads default font */
    //console_load_font(con, "cour_32");
    //console_load_font(con, "cour_32_tt");
    //console_load_font(con, "VGA-ROM.F16");
    //console_load_font(con, "COMPAQP3.F16");
    //console_load_font(con, "DOSV-437.F16");
    //console_load_font(con, "DOSJ-437.F19");
    //console_load_font(con, "CGA.F08");

    /* init text ram and update rect */
    for (i = 0; i < height; i++)
        clear_line(con, 0, con->cols - 1, i, ATTR_DEFAULT);
    console_movecursor(con, 0, con->lines-1);

    return con;
}

Drawable *create_pixmap(int pixtype, int width, int height)
{
    Drawable *dp;
    int bpp, pitch, size;

    switch (pixtype) {
    case MWPF_TRUECOLORARGB:
    case MWPF_TRUECOLORABGR:
        bpp = 32;
        break;
    default:
        printf("Invalid pixel format: %d\n", pixtype);
        return 0;
    }
    pitch = width * (bpp >> 3);
    size = sizeof(Drawable) + height * pitch;

    dp = malloc(size);
    if (!dp) {
            printf("Can't allocate pixmap\n");
            return 0;
    }

    memset(dp, 0, size);
    dp->pixtype = pixtype;
    dp->bpp = bpp;
    dp->bytespp = bpp >> 3;
    dp->width = width;
    dp->height = height;
    dp->pitch = pitch;
    dp->pixels = (uint8_t *)&dp->data[0];
    dp->size = height * pitch;
    dp->color = 0x00ffffff;
    return dp;
}

/* draw pixel w/clipping */
void draw_point(Drawable *dp, int x, int y)
{
    Pixel *pixel;

    if ((unsigned)x < dp->width && (unsigned)y < dp->height) {
        pixel = (Pixel *)(dp->pixels + y * dp->pitch + x * dp->bytespp);
        *pixel = dp->color;
    }
}

Pixel read_pixel(Drawable *dp, int x, int y)
{
    Pixel *pixel;

    if ((unsigned)x < dp->width && (unsigned)y < dp->height) {
        pixel = (Pixel *)(dp->pixels + y * dp->pitch + x * dp->bytespp);
        return *pixel;
    }
    return 0;
}

/* draw horizontal line inclusive of (x1, x2) w/clipping */
void draw_hline(Drawable *dp, int x1, int x2, int y)
{
    if ((unsigned)y >= dp->height)
        return;

    Pixel *pixel = (Pixel *)(dp->pixels + y * dp->pitch + x1 * dp->bytespp);
    while (x1++ <= x2) {
        if ((unsigned)x1 >= dp->width)
            return;
        *pixel++ = dp->color;
    }
}

/* draw vertical line inclusive of (y1, y2) w/clipping */
void draw_vline(Drawable *dp, int x, int y1, int y2)
{
    if ((unsigned)x >= dp->width)
        return;

    Pixel *pixel = (Pixel *)(dp->pixels + y1 * dp->pitch + x * dp->bytespp);
    while (y1++ <= y2) {
        if ((unsigned)y1 >= dp->height)
            return;
        *pixel = dp->color;
        pixel += dp->pitch >> 2;    /* pixels not bytes */
    }
}

/* draw rectangle inclusive of (x1,y1; x2,y2) w/clipping */
void draw_rect(Drawable *dp, int x1, int y1, int x2, int y2)
{
    /* normalize coordinates */
    int xmin = (x1 <= x2) ? x1 : x2;
    int xmax = (x1 > x2) ? x1 : x2;
    int ymin = (y1 <= y2) ? y1 : y2;
    int ymax = (y1 > y2) ? y1 : y2;

    draw_hline(dp, xmin, xmax, ymin);
    draw_hline(dp, xmin, xmax, ymax);

    draw_vline(dp, xmin, ymin+1, ymax-1);
    draw_vline(dp, xmax, ymin+1, ymax-1);
}

/* draw filled rectangle inclusive of (x1,y1; x2,y2) w/clipping */
void draw_fill_rect(Drawable *dp, int x1, int y1, int x2, int y2)
{
    /* normalize coordinates */
    int xmin = (x1 <= x2) ? x1 : x2;
    int xmax = (x1 > x2) ? x1 : x2;
    int ymin = (y1 <= y2) ? y1 : y2;
    int ymax = (y1 > y2) ? y1 : y2;

    while (ymin <= ymax)
        draw_hline(dp, xmin, xmax, ymin++);
}

#if UNUSED
/* draw filled rectangle inclusive of (x1,y1; x2,y2) w/clipping */
void draw_fill_rect_fast(Drawable *dp, int x1, int y1, int x2, int y2)
{
    while (y1 <= y2)
        draw_hline(dp, x1, x2, y1++);
}
#endif

void draw_line(Drawable *dp, int x1, int y1, int x2, int y2)
{
    /* Bresenham's line algorithm for efficient line drawing */
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (x1 != x2 || y1 != y2) {
        draw_point(dp, x1, y1);

        int e2 = err << 1;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
    draw_point(dp, x2, y2);
}

/* Based on algorithm http://members.chello.at/easyfilter/bresenham.html */
void draw_circle(Drawable *dp, int x0, int y0, int r)
{
    int x = -r, y = 0, err = 2-2*r;             /* II. Quadrant */

    while (-x >= y) {                           /* Draw symmetry points */
        int x1 = (x0 + x >= 0) ? x0 + x : 0;
        int x2   = (x0 - x <= dp->width) ? x0 - x : dp->width;
        int y1 = (y0 - y >= 0) ? y0 - y : 0;
        int y2   = (y0 + y < dp->height) ? y0 + y : dp->height-1;

        draw_point(dp, x1, y2);
        draw_point(dp, x2, y2);
        draw_point(dp, x1, y1);
        draw_point(dp, x2, y1);

        x1 = (x0 - y >= 0) ? x0 - y : 0;
        x2   = (x0 + y <= dp->width) ? x0 + y : dp->width;
        y1 = (y0 + x >= 0) ? y0 + x : 0;
        y2   = (y0 - x < dp->height) ? y0 - x : dp->height-1;

        draw_point(dp, x2, y1);
        draw_point(dp, x1, y1);
        draw_point(dp, x2, y2);
        draw_point(dp, x1, y2);

        r = err;
        if (r <= y) err += ++y*2+1;             /* e_xy+e_y < 0 */
        if (r > x || err > y) err += ++x*2+1;   /* e_xy+e_x > 0 or no 2nd y-step */
    }
}

/* Based on algorithm http://members.chello.at/easyfilter/bresenham.html */
void draw_fill_circle(Drawable *dp, int x0, int y0, int r)
{
    if (r <= 1) {
        draw_point(dp, x0, y0);
        return;
    }
    int x = -r, y = 0, err = 2-2*r;             /* II. Quadrant */
    int X_lim = dp->width;
    do {
        int xstart = (x0 + x >= 0) ? x0 + x : 0;
        int xend   = (x0 - x <= X_lim) ? x0 - x : X_lim;

        if (y0 + y < dp->height)
            draw_hline(dp, xstart, xend, y0 + y);

        if (y0 - y >= 0 && y > 0)
            draw_hline(dp, xstart, xend, y0 - y);

        r = err;
        if (r <= y) err += ++y*2+1;             /* e_xy+e_y < 0 */
        if (r > x || err > y) err += ++x*2+1;   /* e_xy+e_x > 0 or no 2nd y-step */
    } while (x < 0);
}

void draw_thick_line(Drawable *dp, int x1, int y1, int x2, int y2, int r)
{
    // Bresenham's line algorithm for efficient line drawing
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (x1 != x2 || y1 != y2) {
        draw_fill_circle(dp, x1, y1, r);

        int e2 = err << 1;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
    draw_fill_circle(dp, x2, y2, r);
}

/* Flood fill code originally from https://github.com/silvematt/TomentPainter.git
 * MIT License
 * Copyright (c) [2022] [silvematt]
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#define FLOOD_FILL_STACKSZ  200
#define false               0
#define true                1
typedef int boolean_t;

static void push(Point stack[], int x, int y, int *top)
{
    if (*top < FLOOD_FILL_STACKSZ - 1) {
        (*top)++;
        stack[*top].x = x;
        stack[*top].y = y;
    }
}

static Point pop(Point stack[], int *top)
{
    return stack[(*top)--];
}

void draw_flood_fill(Drawable *dp, int x, int y)
{
    boolean_t mRight, alreadyCheckedAbove, alreadyCheckedBelow;
    Point curElement;
    Point stack[FLOOD_FILL_STACKSZ];
    int stackTop = -1;
    Pixel orgColor = read_pixel(dp, x, y);

    if (dp->color == orgColor)
        return;

    /* Push the first element */
    push(stack, x, y, &stackTop);
    while (stackTop >= 0)   /* While there are elements */
    {
        /* Take the first one */
#ifdef __C86__     /* FIXME C86 compiler bug pop returning struct */
        curElement = stack[stackTop];
        stackTop--;
#else
        curElement = pop(stack, &stackTop);
#endif
        mRight = false;
        int leftestX = curElement.x;

        /* Find leftest */
        while (leftestX >= 0 && read_pixel(dp, leftestX, curElement.y) == orgColor)
            leftestX--;
        leftestX++;

        alreadyCheckedAbove = false;
        alreadyCheckedBelow = false;

        /* While this line has not finished to be drawn */
        while (mRight == false)
        {
            /* Fill right */
            if (leftestX < dp->width
                && read_pixel(dp, leftestX, curElement.y) == orgColor)
            {
                draw_point(dp, leftestX, curElement.y);

                /* Check above this pixel */
                if (alreadyCheckedBelow == false && (curElement.y-1) >= 0
                    && (curElement.y-1) < dp->height
                    && read_pixel(dp, leftestX, curElement.y-1) == orgColor)
                    {
                        /* If we never checked it, add it to the stack */
                        push(stack, leftestX, curElement.y-1, &stackTop);
                        alreadyCheckedBelow = true;
                    }
                else if (alreadyCheckedBelow == true && (curElement.y-1) > 0
                        && read_pixel(dp, leftestX, curElement.y-1) != orgColor)
                    {
                        /* Skip now, but check next time */
                        alreadyCheckedBelow = false;
                    }

                /* Check below this pixel */
                if (alreadyCheckedAbove == false && (curElement.y+1) >= 0
                    && (curElement.y+1) < dp->height
                    && read_pixel(dp, leftestX, curElement.y+1) == orgColor)
                    {
                        /* If we never checked it, add it to the stack */
                        push(stack, leftestX, curElement.y+1, &stackTop);
                        alreadyCheckedAbove = true;
                    }
                else if (alreadyCheckedAbove == true
                    && (curElement.y+1) < dp->height
                    && read_pixel(dp, leftestX, curElement.y+1) != orgColor)
                    {
                        /* Skip now, but check next time */
                        alreadyCheckedAbove = false;
                    }

                /* Keep going on the right */
                leftestX++;
            }
            else /* Done */
                mRight = true;
        }
    }
}
/* end flood fill code */

static int sdl_nextevent(struct console *con, struct console *con2)
{
    int c;
    SDL_Event event;

    if (SDL_WaitEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                return 1;

            case SDL_KEYDOWN:
                c = sdl_key(event.key.state, event.key.keysym);
                if (c == 'q') return 1;     //FIXME
                if (c == '\r') {
                    console_textout(con, c, ATTR_DEFAULT);
                    console_textout(con2, c, ATTR_DEFAULT);
                    c = '\n';
                }
                console_textout(con, c, ATTR_DEFAULT);
                console_textout(con2, c, ATTR_DEFAULT);
                break;
        }
    }

    return 0;
}

int main(int ac, char **av)
{
    Drawable *bb;
    struct sdl_window *sdl;
    struct console *con;
    struct console *con2;

    if (!sdl_init()) exit(1);
    if (!(bb = create_pixmap(MWPF_DEFAULT, 640, 400))) exit(2);
    if (!(sdl = sdl_create_window(bb))) exit(3);
    if (!(con = create_console(14, 8))) exit(4);
    console_load_font(con, "cour_32_tt");
    //console_load_font(con, "cour_32");
    //console_load_font(con, "DOSJ-437.F19");
    if (!(con2 = create_console(14, 8))) exit(4);
    con2->dp = bb;
    console_load_font(con2, "cour_32");
    //console_load_font(con2, "cour_32_tt");
    //console_load_font(con2, "DOSJ-437.F19");

    for (;;) {
        //Rect update = con->update;          /* save update rect for dup console */
        draw_console(con, bb, 3*8, 5*15, 1);
        //con->update = update;
        draw_console(con2, bb, 42*8, 5*15, 1);
        if (sdl_nextevent(con, con2))
            break;
        //continue;
        //int x1 = random() % 640;
        //int x2 = random() % 640;
        //int y1 = random() % 400;
        //int y2 = random() % 400;
        //int r = random() % 40;
        //draw_line(bb, x1, y1, x2, y2);
        //draw_thick_line(bb, x1, y1, x2, y2, 6);
        //draw_circle(bb, x1, y1, r);
        //draw_flood_fill(bb, x1, y1);
        //draw_rect(bb, x1, y1, x2, y2);
        //draw_fill_rect(bb, x1, y1, x2, y2);
        sdl_draw(bb, 0, 0, 0, 0);
    }

    SDL_Quit();
    free(sdl);
    free(con);
    free(bb);
}
