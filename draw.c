/*
 * GFX library graphics routines for host (SDL) and framebuffer (embedded).
 *
 * Currently displays rotated bitmap and alpha blended glyphs echoed from keypresses.
 * Newline will result in scrolling screen.
 *
 * Sep 2022 Greg Haerr <greg@censoft.com>
 * Aug 2025 Greg Haerr rewritten, supports bitmap and TTF antialiased fonts
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include "draw.h"
#include <SDL2/SDL.h>

extern int open_pty(void);
static int term_fd;

#define MIN(a,b)      ((a) < (b) ? (a) : (b))
#define MAX(a,b)      ((a) > (b) ? (a) : (b))
#define RGBDEF(r,g,b) {r, g, b, 0}

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

int sdl_key(Uint8 state, SDL_Keysym sym)
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

/* convert character to font glyph index, return default glyph if not present */
static int glyph_offset(Font *font, unsigned int c)
{
    uint16_t first, last, offset = 0;
    uint16_t *r = font->range;

    c -= font->firstchar;
    if (r) {
        do {
            first = r[0]; last = r[1];
            if (c >= first && c <= last)
                return c-first+offset;
            r += 2;
            offset += last - first + 1;
        } while (offset < font->size);
        return font->defaultglyph;
    }
    if (c >= font->size) c = font->defaultglyph;
    return c;
}

/* draw a character from bitmap font, drawbg=2 means fill bg to max width */
int draw_font_bitmap(Drawable *dp, Font *font, int c, int sx, int sy, int xoff, int yoff,
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

    c = glyph_offset(font, c);

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
    return w;
}

/* draw a character from an antialiasing font, drawbg=2 means fill bg to max width */
int draw_font_alpha(Drawable *dp, Font *font, int c, int sx, int sy, int xoff, int yoff,
    Pixel fgpixel, Pixel bgpixel, int drawbg, int rotangle)
{
    int x, y, minx, maxx, w, zerox, dspan;
    int height = font->height;
    Pixel *dst;
    Varptr bits;
    int sin_a, cos_a, s;        /* for rotated bitmaps */

    c = glyph_offset(font, c);

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
    return w;
}

int draw_font_char(Drawable *dp, Font *font, int c, int x, int y, int xoff, int yoff,
    Pixel fg, Pixel bg, int drawbg, int rotangle)
{
    if (font->bpp == 8)
        return draw_font_alpha(dp, font, c, x, y, xoff, yoff, fg, bg, drawbg, rotangle);
    return draw_font_bitmap(dp, font, c, x, y, xoff, yoff, fg, bg, drawbg, rotangle);
}

int draw_font_string(Drawable *dp, Font *font, char *text, int x, int y,
    int xoff, int yoff, Pixel fg, Pixel bg, int drawbg, int rotangle)
{
    int c, adv, xstart = xoff;

    while (*text) {
        c = *text++;
        adv = draw_font_char(dp, font, c, x, y, xoff, yoff, fg, bg, drawbg, rotangle);
        xoff += adv;
    }
    return xoff - xstart;
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

#if OLDWAY
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

/* output character at cursor location*/
void console_textout(struct console *con, int c, int attr)
{
    switch (c) {
    case '\b':  if (--con->curx < 0) con->curx = 0; goto update;
    case '\r':  con->curx = 0; goto update;
    case '\n':  goto scroll;
    case ':':   if (--oversamp <= 0) oversamp = 1;
    same2:      printf("%d\n", oversamp);
                goto same;
    case ';':   ++oversamp;
                goto same2;
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
                x * con->char_width, y * con->char_height, fg, bg, 2, angle);
            j++;
        }
    }
}
#endif

/* draw horizontal line inclusive of (x1, x2) w/clipping */
void draw_hline(Drawable *dp, int x1, int x2, int y)
{
    if ((unsigned)y >= dp->height)
        return;

    Pixel *pixel = (Pixel *)(dp->pixels + y * dp->pitch + x1 * dp->bytespp);
    while (x1++ <= x2) {
        if ((unsigned)x1 >= dp->width)
            return;
        *pixel++ = dp->fgcolor;
    }
}

void draw_clear(Drawable *dp)
{
    Pixel save = dp->fgcolor;

    dp->fgcolor = dp->bgcolor;
    for (int y = 0; y < dp->height; y++)
        draw_hline(dp, 0, dp->width - 1, y);
    dp->fgcolor = save;
}

static void clear_screen(Drawable *dp)
{
    draw_clear(dp);
    if (dp->font) {
        draw_font_string(dp, dp->font, "Use '{' or '}' to rotate text", 20, 20,
            0, 0, dp->fgcolor, dp->bgcolor, 1, 0);
    }
}

#ifndef NOMAIN
void console_write(struct console *con, char *buf, size_t n)
{
    tmt_write(con->vt, buf, n);
    update_dirty_region(con, 0, 0, con->cols, con->lines);
}

void console_putchar(struct console *con, int c)
{
    char buf[1];
    buf[0] = c;
    tmt_write(con->vt, buf, 1);
    update_dirty_region(con, 0, 0, con->cols, con->lines);
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
    const TMTSCREEN *s = tmt_screen(con->vt);
    Pixel fg, bg;

    for (int y = sy; y < ey; y++) {
        int j = y * con->cols + sx;
        for (int x = sx; x < ex; x++) {
            unsigned int ch = s->lines[y]->chars[x].c;
            unsigned int fgattr = s->lines[y]->chars[x].a.fg;
            unsigned int bgattr = s->lines[y]->chars[x].a.bg;
            unsigned int attr = ATTR_DEFAULT;
            if (fgattr != TMT_COLOR_DEFAULT)
                attr = (attr & 0xF0) | fgattr;
            if (bgattr != TMT_COLOR_DEFAULT)
                attr = (attr & 0x0F) | (bgattr << 4);
            if (s->lines[y]->chars[x].a.bold)
                attr += 0x08;
            if (s->lines[y]->chars[x].a.reverse)
                attr = ((attr >> 4) & 0x0f) | ((attr << 4) & 0xF0);
            color_from_attr(dp, attr, &fg, &bg);
            draw_font_char(dp, con->font, ch, x1, y1,
                x * con->char_width, y * con->char_height, fg, bg, 2, angle);
            j++;
        }
    }
}

/* draw console onto drawable, flush=1 writes update rect to SDL, =2 draw whole console */
void draw_console(struct console *con, Drawable *dp, int x, int y, int flush)
{
    Pixel fg, bg;

    con->dp = dp;   // FIXME for testing w/clear_screen()

    if (flush == 2) {
        con->update.x = 0;
        con->update.y = 0;
        con->update.w = con->cols;
        con->update.h = con->lines;
    }
    if (con->update.w >= 0 || con->update.h >= 0) {
        /* draw text bitmaps from adaptor RAM */
        draw_console_ram(dp, con, x, y,
            con->update.x, con->update.y, con->update.w, con->update.h);

        /* draw cursor */
        color_from_attr(dp, ATTR_DEFAULT, &fg, &bg);
#if !OLDWAY
        const TMTPOINT *cursor = tmt_cursor(con->vt);
        con->curx = cursor->c;
        con->cury = cursor->r;
        if (!cursor->hidden)
#endif
        draw_font_char(dp, con->font, '_', x, y,
            con->curx * con->char_width, con->cury * con->char_height, fg, bg, 0, angle);

        if (flush == 1) {
            sdl_draw(dp,
                x + con->update.x * con->char_width,
                y + con->update.y * con->char_height,
                (con->update.w - con->update.x) * con->char_width,
                (con->update.h - con->update.y) * con->char_height);
        }
        reset_dirty_region(con);
    }
}
#endif

#define ARRAYLEN(a)     (sizeof(a)/sizeof(a[0]))

extern Font font_rom_8x16_1;
extern Font font_unifont_8x16_1;
extern Font font_mssans_11x13_8;
extern Font font_cour_11x19_8;
extern Font font_cour_20x37_1;
extern Font font_cour_21x37_8;
extern Font font_times_30x37_8;

static Font *fonts[] = {        /* first font is default font */
#ifdef NOMAIN
    &font_rom_8x16_1,
#else
    &font_unifont_8x16_1,
    &font_rom_8x16_1,
    &font_mssans_11x13_8,
    &font_cour_11x19_8,
    &font_cour_20x37_1,
    &font_cour_21x37_8,
    &font_times_30x37_8,
#endif
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
    if (fd < 0)
        return 0;
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

/* try loading internal, then disk font. fail returns NULL */
Font *font_load_font(char *path)
{
    Font *font = NULL;
    char fontdir[80];

    if (path) font = font_load_internal_font(path);
    if (!font) {
        if (path) {
            font = font_load_disk_font(path);
            if (!font) {
                strcpy(fontdir, "fonts/");
                strcat(fontdir, path);
                font = font_load_disk_font(fontdir);
            }
        }
    }
    if (!font) return NULL;

    /* older Microwindows MWCFONT struct compatibility */
    if (font->bpp == 0)          font->bpp = 1;          /* mwin default 1 bpp */
    if (font->bits_width == 0)   font->bits_width = 2;   /* mwin default 16 bits */
    if (font->offset_width == 0) font->offset_width = 4; /* mwin default 32 bits */

    return font;
}

Font *console_load_font(struct console *con, char *path)
{
    Font *font = font_load_font(path);
    if (!font) {
        if (path)
            printf("Can't find font '%s', using default %s\n", path, fonts[0]->name);
        font = fonts[0];
    }

    con->font = font;
    con->char_height = font->height;
    con->char_width = font->maxwidth;
    return font;
}

#ifndef NOMAIN
static void sendhost(const char *str)
{
    write(term_fd, str, strlen(str));
}

static void callback(tmt_msg_t m, TMT *vt, const void *a, void *p)
{
    if (m == TMT_MSG_ANSWER)
        sendhost(a);
}

struct console *create_console(int width, int height)
{
    struct console *con;
    int size, i;

    size = sizeof(struct console);
#if OLDWAY
    size += width * height * sizeof(uint16_t);
#endif
    con = malloc(size);
    if (!con) return 0;

    memset(con, 0, sizeof(struct console));
#if !OLDWAY
    con->vt = tmt_open(height, width, callback, NULL, NULL);
    if (!con->vt) return 0;
    update_dirty_region(con, 0, 0, width, height);
#endif
    con->cols = width;
    con->lines = height;
    console_load_font(con, NULL);       /* loads default font */
    //console_load_font(con, "VGA-ROM.F16");
    //console_load_font(con, "COMPAQP3.F16");
    //console_load_font(con, "DOSV-437.F16");
    //console_load_font(con, "DOSJ-437.F19");
    //console_load_font(con, "CGA.F08");

#if OLDWAY
    /* init text ram and update rect */
    for (i = 0; i < height; i++)
        clear_line(con, 0, con->cols - 1, i, ATTR_DEFAULT);
    console_movecursor(con, 0, con->lines-1);
#endif
    return con;
}

int console_resize(struct console *con, int width, int height)
{
    con->cols = width;
    con->lines = height;
    clear_screen(con->dp);
    update_dirty_region(con, 0, 0, width, height);
    return tmt_resize(con->vt, height, width);
}
#endif

Drawable *create_drawable(int pixtype, int width, int height)
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
    dp->fgcolor = RGB(255,255,255);
    dp->bgcolor = RGB(0, 0, 255);
    draw_clear(dp);
    return dp;
}

/* draw pixel w/clipping */
void draw_point(Drawable *dp, int x, int y)
{
    Pixel *pixel;

    if ((unsigned)x < dp->width && (unsigned)y < dp->height) {
        pixel = (Pixel *)(dp->pixels + y * dp->pitch + x * dp->bytespp);
        *pixel = dp->fgcolor;
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

/* draw vertical line inclusive of (y1, y2) w/clipping */
void draw_vline(Drawable *dp, int x, int y1, int y2)
{
    if ((unsigned)x >= dp->width)
        return;

    Pixel *pixel = (Pixel *)(dp->pixels + y1 * dp->pitch + x * dp->bytespp);
    while (y1++ <= y2) {
        if ((unsigned)y1 >= dp->height)
            return;
        *pixel = dp->fgcolor;
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

    if (dp->fgcolor == orgColor)
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

void draw_flush(Drawable *dp)
{
    sdl_draw(dp, 0, 0, 0, 0);
}

#if UNUSED
static int overlap(int src_x, int src_y, int dst_x, int dst_y, int width, int height)
{
    int sx2 = src_x + width;
    int sy2 = src_y + height;
    int dx2 = dst_x + width;
    int dy2 = dst_y + height;

    /*
     * The rectangles don't overlap if
     * one rectangle's minimum in some dimension
     * is greater than the other's maximum in
     * that dimension.
     *
     * bool noOverlap = r1.x1 > r2.x2 ||
     *                  r2.x1 > r1.x2 ||
     *                  r1.y1 > r2.y2 ||
     *                  r2.y1 > r1.y2;
     * Here, we use >= since x2,y2 is lower right + 1,1 adding width/height.
     */
  //int no_over = (src_x >= dx2 || sx2 <= dst_x || src_y >= dy2 || sy2 <= dst_y);
    int over =    (src_x < dx2  && sx2 > dst_x  && src_y < dy2  && sy2 > dst_y);

    if (over) {
        printf("Overlap t/l %d,%d %d,%d  b/r %d,%d %d,%d\n",
            src_x, src_y, dst_x, dst_y, sx2, sy2, dx2, dy2);
    }
    return over;
}

/* fast copy blit, no clipping or overlap handling */
void draw_blit_fast(Drawable *td, int dst_x, int dst_y, int width, int height,
    Drawable *ts, int src_x, int src_y)
{
    int span = width * td->bytespp;
    int dspan = td->pitch - span;
    int sspan = ts->pitch - span;
    int y = height;

    /* check for overlap, but do nothing */
    overlap(src_x, src_y, dst_x, dst_y, width, height);

    Pixel *dst = (Pixel *)(td->pixels + dst_y * td->pitch + dst_x * td->bytespp);
    Pixel *src = (Pixel *)(ts->pixels + src_y * ts->pitch + src_x * ts->bytespp);
    do {
        int x = width;
        do {
            *dst++ = *src++;
        } while (--x > 0);
        dst = (Pixel *)((uint8_t *)dst + dspan);
        src = (Pixel *)((uint8_t *)src + sspan);
    } while (--y > 0);
}
#endif

/* copy blit, handles any overlap, clips source and optionally destination */
void draw_blit(Drawable *td, int dst_x, int dst_y, int width, int height,
    Drawable *ts, int src_x, int src_y)
{
    /* clip src to source Drawable */
    if (src_x < 0)
    {
        width += src_x;
        dst_x -= src_x;
        src_x = 0;
    }
    if (src_y < 0)
    {
        height += src_y;
        dst_y -= src_y;
        src_y = 0;
    }
    if (src_x + width > ts->width)
        width = ts->width - src_x;
    if (src_y + height > ts->height)
        height = ts->height - src_y;

#if CLIP_TO_DEST
    /* clip dst to destination Drawable */
    int rx1 = 0;
    int ry1 = 0;
    int rx2 = td->width;
    int ry2 = td->height;

    //rx1 = 200;
    //ry1 = 200;
    //rx2 = 599;
    //ry2 = 599;

    rx1 = MAX(rx1, dst_x);
    ry1 = MAX(ry1, dst_y);
    rx2 = MIN(rx2, dst_x + width);
    ry2 = MIN(ry2, dst_y + height);
    int rw = rx2 - rx1;
    int rh = ry2 - ry1;
    if (rw <= 0 || rh <= 0)
        return;

    dst_x = rx1;
    dst_y = ry1;
    width = rw;
    height = rh;
    src_x = src_x + rx1 - dst_x;
    src_y = src_y + ry1 - dst_y;
#endif
#if 0
    printf("B   w/h %d,%d src %d,%d dst %d,%d max src %d,%d dst %d,%d\n", width, height,
        src_x, src_y, dst_x, dst_y, ts->width, ts->height, td->width, td->height);
    if (src_x < 0) { printf("1\n"); exit(1); }
    if (src_y < 0) { printf("2\n"); exit(1); }
    if (src_x + width > ts->width) { printf("3\n"); exit(1); }
    if (dst_x + width > td->width) { printf("4\n"); exit(1); }
    if (src_y + height > ts->height) { printf("5\n"); exit(1); }
    if (dst_y + height > td->height) { printf("6\n"); exit(1); }
#endif
    int ssz = ts->bytespp;
    int dsz = td->bytespp;
    int src_pitch = ts->pitch;
    int dst_pitch = td->pitch;
    uint8_t *src = ts->pixels + src_y * src_pitch + src_x * ssz;
    uint8_t *dst = td->pixels + dst_y * dst_pitch + dst_x * dsz;

    /* check for backwards copy if dst in src rect */
    if (ts->pixels == td->pixels)
    {
        if (src_y < dst_y)
        {
            /* copy from bottom upwards*/
            src += (height - 1) * ts->pitch;
            dst += (height - 1) * td->pitch;
            src_pitch = -src_pitch;
            dst_pitch = -dst_pitch;
        }
        if (src_x < dst_x)
        {
            /* copy from right to left*/
            src += (width - 1) * ts->bytespp;
            dst += (width - 1) * td->bytespp;
            ssz = -ssz;
            dsz = -dsz;
        }
    }
    while (--height >= 0)
    {
        register unsigned char *d = dst;
        register unsigned char *s = src;
        int w = width;

        while (--w >= 0)
        {
            *(Pixel *)d = *(Pixel *)s;
            d += dsz;
            s += ssz;
        }
        src += src_pitch;
        dst += dst_pitch;
    }
}

#ifndef NOMAIN
static int sdl_nextevent(struct console *con, struct console *con2)
{
    int c;
    SDL_Event event;
    static int w = 20;
    static int h = 10;

    //while (SDL_WaitEvent(&event)) {
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                return 1;

            case SDL_KEYDOWN:
                c = sdl_key(event.key.state, event.key.keysym);
                switch (c) {
                case '\0':  return 0;
                case SDLK_UP:
                    sendhost(TMT_KEY_UP);
                    return 0;
                case SDLK_DOWN:
                    sendhost(TMT_KEY_DOWN);
                    return 0;
                case SDLK_RIGHT:
                    sendhost(TMT_KEY_RIGHT);
                    return 0;
                case SDLK_LEFT:
                    sendhost(TMT_KEY_LEFT);
                    return 0;
                /* test cases follow */
                case '~':   return 1;
                case '_':   console_resize(con, --w, --h); return 0;
                case '+':   console_resize(con, ++w, ++h); return 0;
                case '{':   angle--;
                same:
                            clear_screen(con->dp);
                            update_dirty_region(con, 0, 0, con->cols, con->lines);
                            return 0;
                case '}':   angle++; goto same;
                }
#if 0
                if (c == '\r') {
                    console_textout(con, c, ATTR_DEFAULT);
                    console_textout(con2, c, ATTR_DEFAULT);
                    c = '\n';
                }
                console_textout(con, c, ATTR_DEFAULT);
                console_textout(con2, c, ATTR_DEFAULT);
                return 0;
#else
                char c2 = c;
                write(term_fd, &c2, 1);
#endif
        }
    }
    fd_set fdset;
    struct timeval tv;
    int ret;
    char buf[256];

    FD_ZERO(&fdset);
    FD_SET(term_fd, &fdset);
    tv.tv_sec = 0;
    tv.tv_usec = 30000;
    ret = select(term_fd + 1, &fdset, NULL, NULL, &tv);
    if (ret > 0) {
        if (FD_ISSET(term_fd, &fdset)) {
            int n = read(term_fd, buf, sizeof(buf));
            if (n > 0)
                console_write(con, buf, n);
        }
    }

    return 0;
}

int main(int ac, char **av)
{
    Drawable *dp;
    struct sdl_window *sdl;

    if (!sdl_init()) exit(1);
    //if (!(dp = create_drawable(MWPF_DEFAULT, 640, 400))) exit(2);
    if (!(dp = create_drawable(MWPF_DEFAULT, 1024, 800))) exit(2);
    if (!(sdl = sdl_create_window(dp))) exit(3);

#if 1
    term_fd = open_pty();
    struct console *con;
    struct console *con2 = NULL;
    dp->font = font_load_font("times_30x37_8");
    //dp->font = font_load_font("mssans_11x13_8");
    if (!(con = create_console(80, 24))) exit(4);
    //console_load_font(con, "cour_11x19_8");
    //console_load_font(con, "cour_21x37_8");
    console_load_font(con, "unifont_8x16_1");
    //console_load_font(con, "mssans_11x13_8");
    //console_load_font(con, "rom_8x16_1");
    //console_load_font(con, "cour_20x37_1");
    //console_load_font(con, "DOSJ-437.F19");
    if (0x25C6 - con->font->firstchar >= con->font->size)
        tmt_set_unicode_decode(con->vt, true);

    //if (!(con2 = create_console(14, 8))) exit(4);
    //con2->dp = dp;
    //console_load_font(con2, "cour_20x37_1");
    //console_load_font(con2, "cour_21x37_8");
    //console_load_font(con2, "DOSJ-437.F19");
    clear_screen(dp);
    draw_flush(dp);

#if 2
    /* test invalid UTF-8 */
    //console_textout(con, 0xc0, ATTR_DEFAULT);
    //console_textout(con2, 0xc1, ATTR_DEFAULT);
    /* test undefined glyph */
    //console_textout(con, 127, ATTR_DEFAULT);
    //console_textout(con2, 127, ATTR_DEFAULT);
    /* test unicode output */
    for (int i=0x00A1; i<=0x00AF; i++) {
        char buf[32];
        int n = xwctomb(buf, i);
        if (n > 0)
            console_write(con, buf, n);
    }
#endif

    write(term_fd, "TERM=ansi\n", 10);
    for (;;) {
        //Rect update = con->update;          /* save update rect for dup console */
        int flush = angle? 2: 0;
        draw_console(con, dp, 3*8, 5*15, flush);
        //con->update = update;
        //draw_console(con2, dp, 42*8, 5*15, flush);
        draw_flush(dp);
        if (sdl_nextevent(con, con2))
            break;
        //continue;
        //int x1 = random() % 640;
        //int x2 = random() % 640;
        //int y1 = random() % 400;
        //int y2 = random() % 400;
        //int r = random() % 40;
        //draw_line(dp, x1, y1, x2, y2);
        //draw_thick_line(dp, x1, y1, x2, y2, 6);
        //draw_circle(dp, x1, y1, r);
        //draw_flood_fill(dp, x1, y1);
        //draw_rect(dp, x1, y1, x2, y2);
        //draw_fill_rect(dp, x1, y1, x2, y2);
    }
    free(con);
#endif

    SDL_Quit();
    free(sdl);
    free(dp);
}
#endif
