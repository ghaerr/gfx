/* GFX library header file */
#include <stdint.h>
#include "font.h"

/* supported internal framebuffer pixel formats */
#define MWPF_DEFAULT        MWPF_TRUECOLORARGB
#define RGB(r,g,b)          RGB2PIXELARGB(r,g,b)

#define MWPF_TRUECOLORARGB  0   /* 32bpp, memory byte order B, G, R, A */
#define MWPF_TRUECOLORABGR  1   /* 32bpp, memory byte order R, G, B, A */

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
    int32_t size;           /* total size in bytes */
    Pixel fgcolor;          /* foregrond draw color */
    Pixel bgcolor;          /* backgrond draw color */
    void *window;           /* opaque pointer for associated (SDL) window */
    Font *font;             /* default font for drawable */
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

/* create 32 bit 8/8/8/8 format pixel (0xAARRGGBB) from RGB triplet*/
#define RGB2PIXELARGB(r,g,b)    \
    (0xFF000000UL | ((r) << 16) | ((g) << 8) | (b))

/* create 32 bit 8/8/8/8 format pixel (0xAABBGGRR) from RGB triplet*/
#define RGB2PIXELABGR(r,g,b)    \
    (0xFF000000UL | ((b) << 16) | ((g) << 8) | (r))

struct sdl_window;

int sdl_init(void);
struct sdl_window *sdl_create_window(Drawable *dp);
void sdl_draw(Drawable *dp, int x, int y, int width, int height);

Drawable *create_drawable(int pixtype, int width, int height);
void draw_clear(Drawable *dp);
void draw_flush(Drawable *dp);
void draw_line(Drawable *dp, int x1, int y1, int x2, int y2);
void draw_fill_rect(Drawable *dp, int x1, int y1, int x2, int y2);
void draw_blit(Drawable *dst, int dst_x, int dst_y, int width, int height,
               Drawable *src, int src_x, int src_y);
