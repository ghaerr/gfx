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

/* supported framebuffer pixel formats */
#define MWPF_DEFAULT       MWPF_TRUECOLORARGB
#define MWPF_TRUECOLORARGB 0    /* 32bpp, memory byte order B, G, R, A */
#define MWPF_TRUECOLORABGR 1    /* 32bpp, memory byte order R, G, B, A */
#define MWPF_TRUECOLOR565  2    /* 16bpp, le unsigned short 5/6/5 RGB */

#define MIN(a,b)      ((a) < (b) ? (a) : (b))
#define MAX(a,b)      ((a) > (b) ? (a) : (b))

typedef uint32_t pixel_t;   /* largest hardware pixel size */
typedef uint8_t alpha_t;    /* size of alpha channel */
typedef uint32_t color_t;   /* ARGB device independent color */

typedef struct point {
    int x, y;
} Point;

typedef struct rect {
    int x, y;
    int w, h;
} Rect;

struct palette {            /* palette entry */
    unsigned char r, g, b, a;
};

typedef struct drawable {
    int pixtype;            /* pixel format */
    int bpp;                /* bits per pixel */
    int bytespp;            /* bytes per pixel */
    int width;              /* width in pixels */
    int height;             /* height in pixels */
    int pitch;              /* stride in bytes, offset to next pixel row */
    pixel_t color;          /* rgb color for glyph blit mode */
    //int mode;               /* blit mode: 1 = colormod */
    void *window;           /* opaque pointer for associated window */
    int32_t size;           /* total size in bytes */
    uint8_t *pixels;        /* pixel data, normally points to data[] below */
    pixel_t data[];         /* drawable memory allocated in single malloc */
} Drawable, Texture;

struct console {
    /* configurable parameters */
    int cols;               /* # text columns */
    int lines;              /* # text rows */
    int char_width;         /* glyph width in pixels */
    int char_height;        /* glyph height in pixels */
    uint8_t *fontbits;      /* glyph bits (currently for 256 glyphs) */
    int font_bps;           /* glyph bits bytes per scanline */

    int curx;               /* cursor x position */
    int cury;               /* cursor y position */
    int lastx;
    int lasty;
    Rect update;            /* console update region in cols/lines coordinates */
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

/* create 16 bit 5/6/5 format pixel from RGB triplet */
#define RGB2PIXEL565(r,g,b) \
    ((((r) & 0xf8) << 8) | (((g) & 0xfc) << 3) | (((b) & 0xf8) >> 3))

#define RGBDEF(r,g,b)   {r, g, b, 0}

#if 1
/*
 * CGA palette for 16 color systems.
 */
static struct palette ega_colormap[16] = {
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
static struct palette ega_colormap[16] = {
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
static struct palette ega_colormap[16] = {
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
    case MWPF_TRUECOLOR565:
        pixelformat = SDL_PIXELFORMAT_RGB565;
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
#define ATTR_DEFAULT 0xF0   /* reverse ltgray */

/* convert EGA attribute to pixel value */
static void color_from_attr(Drawable *dp, unsigned int attr, pixel_t *pfg, pixel_t *pbg)
{
    int fg = attr & 0x0F;
    int bg = (attr & 0x70) >> 4;
    unsigned char fg_red = ega_colormap[fg].r;
    unsigned char fg_green = ega_colormap[fg].g;
    unsigned char fg_blue = ega_colormap[fg].b;
    unsigned char bg_red = ega_colormap[bg].r;
    unsigned char bg_green = ega_colormap[bg].g;
    unsigned char bg_blue = ega_colormap[bg].b;
    pixel_t fgpixel, bgpixel;

    switch (dp->pixtype) {
    case MWPF_TRUECOLORARGB:    /* byte order B G R A */
        fgpixel = RGB2PIXELARGB(fg_red, fg_green, fg_blue);
        bgpixel = RGB2PIXELARGB(bg_red, bg_green, bg_blue);
        break;
    case MWPF_TRUECOLORABGR:    /* byte order R G B A */
        fgpixel = RGB2PIXELABGR(fg_red, fg_green, fg_blue);
        bgpixel = RGB2PIXELABGR(bg_red, bg_green, bg_blue);
        break;
    case MWPF_TRUECOLOR565:
        fgpixel = RGB2PIXEL565(fg_red, fg_green, fg_blue);
        bgpixel = RGB2PIXEL565(bg_red, bg_green, bg_blue);
        break;
    }
    *pfg = fgpixel;
    *pbg = bgpixel;
}

/* draw a character bitmap */
static void drawbitmap(Drawable *dp, struct console *con, int c, unsigned int attr,
    int x, int y, int drawbg)
{
    int minx = x;
    int maxx = x + con->char_width - 1;
    int height = con->char_height;
    int bitcount = 0;
    uint16_t word = 0;
    uint16_t bitmask = (con->font_bps == 1)? 0x80: 0x8000;
    uint8_t *imagebits = con->fontbits + con->char_height * c * con->font_bps;
    uint16_t *imagebits16 = (uint16_t *)imagebits;
    pixel_t fgpixel, bgpixel;

    color_from_attr(dp, attr, &fgpixel, &bgpixel);
    while (height > 0) {
        uint32_t *pixel32;
        uint16_t *pixel16;
        if (bitcount <= 0) {
            if (con->font_bps == 1) {
                bitcount = 8;
                word = *imagebits++;
            } else {
                bitcount = 16;
                word = *imagebits16++;
            }
            pixel32 = (uint32_t *)(dp->pixels + y * dp->pitch + x * dp->bytespp);
            pixel16 = (uint16_t *)pixel32;
        }
        switch (dp->pixtype) {
        case MWPF_TRUECOLORARGB:    /* byte order B G R A */
        case MWPF_TRUECOLORABGR:    /* byte order R G B A */
            if (word & bitmask)
                *pixel32++ = fgpixel;
            else if (drawbg)
                *pixel32++ = bgpixel;
            break;
        case MWPF_TRUECOLOR565:
            if (word & bitmask)
                *pixel16++ = fgpixel;
            else if (drawbg)
                *pixel16++ = bgpixel;
            break;
        }
        word <<= 1;
        bitcount--;
        if (x++ == maxx) {
            x = minx;
            ++y;
            --height;
            bitcount = 0;
        }
    }
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
static void scrolldn(struct console *con, int y1, int y2, unsigned char attr)
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

void con_movecursor(struct console *con, int x, int y)
{
    update_dirty_region(con, con->lastx, con->lasty, 1, 1);
    con->curx = con->lastx = x;
    con->cury = con->lasty = y;
    update_dirty_region(con, x, y, 1, 1);
}

/* output character at cursor location*/
void con_textout(struct console *con, int c, int attr)
{
    switch (c) {
    case '\0':  return;
    case '\b':  if (--con->curx < 0) con->curx = 0; goto update;
    case '\r':  con->curx = 0; goto update;
    case '\n':  goto scroll;
    case '-':   scrolldn(con, 0, con->lines-1, ATTR_DEFAULT); return;   //FIXME
    case '=':   scrollup(con, 0, con->lines-1, ATTR_DEFAULT); return;   //FIXME
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
    con_movecursor(con, con->curx, con->cury);
}

/* draw characters from console text RAM */
static void draw_video_ram(Drawable *dp, struct console *con, int x1, int y1,
    int sx, int sy, int ex, int ey)
{
    uint16_t *vidram = con->text_ram;

    for (int y = sy; y < ey; y++) {
        int j = y * con->cols + sx;
        for (int x = sx; x < ex; x++) {
            uint16_t chattr = vidram[j];
            drawbitmap(dp, con, chattr & 255, chattr >> 8,
                x1 + x * con->char_width, y1 + y * con->char_height, 1);
            j++;
        }
    }
}

/* Called periodically from the main loop */
void draw_console(struct console *con, Drawable *dp, int x, int y, int flush)
{
    if (con->update.w >= 0 || con->update.h >= 0) {
        /* draw text bitmaps from adaptor RAM */
        draw_video_ram(dp, con, x, y,
            con->update.x, con->update.y, con->update.w, con->update.h);

        /* draw cursor */
        drawbitmap(dp, con, '_', ATTR_DEFAULT,
            x + con->curx * con->char_width, y + con->cury * con->char_height, 0);

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

/*
 * load console font, works for:
 *  *.Fnn ROM font files e.g. VGA-ROM.F16, DOSJ-437.F19
 */
void *console_load_font(struct console *con, char *path)
{
    int width = 8;
    int height = 16;
    int fd, size;
    char *s;
    void *bits;

    s = strrchr(path, '.');
    if (s && s[1] == 'F')
        height = atoi(s+2);
    size = height * 256;
    if (width > 8) size *= 2;
    fd = open(path, O_RDONLY);
    if (fd < 0) { printf("Can't open %s\n", path); return 0; }
    printf("Loading %s %dx%d size %d\n", path, width, height, size);
    bits = malloc(size);
    if (read(fd, bits, size) != size) { printf("read error"); return 0; }
    close(fd);
    con->char_width = width;
    con->char_height = height;
    con->fontbits = bits;
    con->font_bps = (width > 8)? 2: 1;
    return bits;
}

struct console *create_console(int width, int height)
{
    struct console *con;
    int size, i;
    extern uint8_t rom8x16_bits[];     /* MWIMAGEBITS */

    size = sizeof(struct console) + width * height * sizeof(uint16_t);
    con = malloc(size);
    if (!con) return 0;

    memset(con, 0, sizeof(struct console));
    con->cols = width;
    con->lines = height;
    con->fontbits = rom8x16_bits;       /* default console font */
    con->font_bps = 2;                  /* MW compiled fonts are 2 bytes per scanline */
    con->char_width = 8;
    con->char_height = 16;
    //console_load_font(con, "VGA-ROM.F16");
    //console_load_font(con, "COMPAQP3.F16");
    console_load_font(con, "DOSV-437.F16");
    //console_load_font(con, "DOSJ-437.F19");
    //console_load_font(con, "CGA.F08");

    /* init text ram and update rect */
    for (i = 0; i < height; i++)
        clear_line(con, 0, con->cols - 1, i, ATTR_DEFAULT);
    con_movecursor(con, 0, con->lines-1);

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
    case MWPF_TRUECOLOR565:
        bpp = 16;
        break;
    default:
        printf("Invalid pixel format\n");
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
    dp->size = size;
    dp->color = 0x00ffffff;
    return dp;
}

void draw_point(Drawable *dp, int x, int y)
{
    pixel_t *pixel;

    if ((unsigned)x < dp->width && (unsigned)y < dp->height) {
        pixel = (pixel_t *)(dp->pixels + y * dp->pitch + x * dp->bytespp);
        *pixel = dp->color;
    }
}

pixel_t read_pixel(Drawable *dp, int x, int y)
{
    pixel_t *pixel;

    if ((unsigned)x < dp->width && (unsigned)y < dp->height) {
        pixel = (pixel_t *)(dp->pixels + y * dp->pitch + x * dp->bytespp);
        return *pixel;
    }
    return 0;
}

void draw_hline(Drawable *dp, int x1, int x2, int y)
{
    if ((unsigned)y >= dp->height)
        return;

    pixel_t *pixel = (pixel_t *)(dp->pixels + y * dp->pitch + x1 * dp->bytespp);
    do {
        if ((unsigned)x1 >= dp->width)
            return;
        *pixel++ = dp->color;
    } while (++x1 != x2);
}

void draw_vline(Drawable *dp, int x, int y1, int y2)
{
    if ((unsigned)x >= dp->width)
        return;

    pixel_t *pixel = (pixel_t *)(dp->pixels + y1 * dp->pitch + x * dp->bytespp);
    do {
        if ((unsigned)y1 >= dp->height)
            return;
        *pixel = dp->color;
		pixel += dp->pitch >> 2;    /* pixels not bytes */
    } while (++y1 != y2);
}

void draw_rect(Drawable *dp, int x1, int y1, int x2, int y2)
{
    /* normalize coordinates */
    int xmin = (x1 <= x2) ? x1 : x2;
    int xmax = (x1 > x2) ? x1 : x2;
    int ymin = (y1 <= y2) ? y1 : y2;
    int ymax = (y1 > y2) ? y1 : y2;

    /* top and bottom horizontal lines */
    draw_hline(dp, xmin, xmax, ymin);
    draw_hline(dp, xmin, xmax, ymax);

    /* left and right vertical lines */
    draw_vline(dp, xmin, ymin, ymax);
    draw_vline(dp, xmax, ymin, ymax);
}

void draw_filled_rect(Drawable *dp, int x1, int y1, int x2, int y2)
{
    /* normalize coordinates */
    int xmin = (x1 <= x2) ? x1 : x2;
    int xmax = (x1 > x2) ? x1 : x2;
    int ymin = (y1 <= y2) ? y1 : y2;
    int ymax = (y1 > y2) ? y1 : y2;

    while (ymin <= ymax)
        draw_hline(dp, xmin, xmax, ymin++);
}

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
void draw_filled_circle(Drawable *dp, int x0, int y0, int r)
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
        draw_filled_circle(dp, x1, y1, r);

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
    draw_filled_circle(dp, x2, y2, r);
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
    pixel_t orgColor = read_pixel(dp, x, y);

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

static int sdl_nextevent(struct console *con)
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
                    con_textout(con, c, ATTR_DEFAULT);
                    c = '\n';
                }
                con_textout(con, c, ATTR_DEFAULT);
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

    if (!sdl_init()) exit(1);
    if (!(bb = create_pixmap(MWPF_DEFAULT, 640, 400))) exit(2);
    if (!(sdl = sdl_create_window(bb))) exit(3);
    if (!(con = create_console(20, 10))) exit(4);

    for (;;) {
        Rect update = con->update;          /* save update rect for dup console */
        draw_console(con, bb, 5*8, 5*15, 1);
        con->update = update;
        draw_console(con, bb, 35*8, 5*15, 1);
        if (sdl_nextevent(con))
            break;
        continue;
        int x1 = random() % 640;
        int x2 = random() % 640;
        int y1 = random() % 400;
        int y2 = random() % 400;
        int r = random() % 40;
        draw_line(bb, x1, y1, x2, y2);
        //draw_thick_line(bb, x1, y1, x2, y2, 6);
        //draw_circle(bb, x1, y1, r);
        //draw_flood_fill(bb, x1, y1);
        //draw_rect(bb, x1, y1, x2, y2);
        //draw_filled_rect(bb, x1, y1, x2, y2);
        sdl_draw(bb, 0, 0, 0, 0);
    }

    SDL_Quit();
    free(sdl);
    free(con);
    free(bb);
}
