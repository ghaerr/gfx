/* GFX library graphics drawing routines */
#include <stdint.h>
#include "font.h"
#include "tmt.h"

/* supported internal framebuffer pixel formats */
#define MWPF_DEFAULT        MWPF_TRUECOLORARGB
#define RGB(r,g,b)          RGB2PIXELARGB(r,g,b)

#define MWPF_TRUECOLORARGB  0   /* 32bpp, memory byte order B, G, R, A */
#define MWPF_TRUECOLORABGR  1   /* 32bpp, memory byte order R, G, B, A */

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
    TMT *vt;
    uint16_t text_ram[];    /* adaptor RAM (= cols * lines * 2) in single malloc OLDWAY */
};

/* create 32 bit 8/8/8/8 format pixel (0xAARRGGBB) from RGB triplet*/
#define RGB2PIXELARGB(r,g,b)    \
    ((uint32_t)0xFF000000 | (((uint32_t)r) << 16) | ((g) << 8) | (b))

/* create 32 bit 8/8/8/8 format pixel (0xAABBGGRR) from RGB triplet*/
#define RGB2PIXELABGR(r,g,b)    \
    ((uint32_t)0xFF000000 | (((uint32_t)b) << 16) | ((g) << 8) | (r))

/* draw.c */
Drawable *create_drawable(int pixtype, int width, int height);
void draw_clear(Drawable *dp);
void draw_line(Drawable *dp, int x1, int y1, int x2, int y2);
void draw_fill_rect(Drawable *dp, int x1, int y1, int x2, int y2);
void draw_blit(Drawable *dst, int dst_x, int dst_y, int width, int height,
    Drawable *src, int src_x, int src_y);
void draw_blit_fast(Drawable *dst, int dst_x, int dst_y, int width, int height,
    Drawable *src, int src_x, int src_y);
void draw_flush(Drawable *dp, int x, int y, int width, int height);     /* in sdl.c */

/* font.c */
int draw_font_string(Drawable *dp, Font *font, char *text, int x, int y,
    int xoff, int yoff, Pixel fg, Pixel bg, int drawbg, int rotangle);
int draw_font_char(Drawable *dp, Font *font, int c, int x, int y, int xoff, int yoff,
    Pixel fg, Pixel bg, int drawbg, int rotangle);
Font *font_load_font(char *path);
Font *console_load_font(struct console *con, char *path);

/* console.c */
struct console *create_console(int width, int height);
int console_resize(struct console *con, int width, int height);
void console_dirty(struct console *con, int x, int y, int w, int h);
void console_write(struct console *con, char *buf, size_t n);
void draw_console(struct console *con, Drawable *dp, int x, int y, int flush);
