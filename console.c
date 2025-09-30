/* GFX library console terminal display routines */
#include <stdio.h>
#include <string.h>
#include "draw.h"

#define OLDWAY      0

int angle = 0;

/* default display attribute (for testing)*/
//#define ATTR_DEFAULT 0x09   /* bright blue */
//#define ATTR_DEFAULT 0x0F   /* bright white */
//#define ATTR_DEFAULT 0xF0   /* reverse ltgray */
#define ATTR_DEFAULT 0x35

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
    default:
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

#if OLDWAY
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
    con->update.w = con->update.h = 0;
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

/* draw console onto drawable, flush=1 writes update rect to SDL, =2 draw whole console */
void draw_console(struct console *con, Drawable *dp, int x, int y, int flush)
{
    Pixel fg, bg;

    con->dp = dp;   // FIXME for testing w/clear_screen()

    if (flush == 2)
        update_dirty_region(con, 0, 0, con->cols, con->lines);

    if (con->update.w > 0 || con->update.h > 0) {
        /* draw text bitmaps from adaptor RAM */
        draw_console_ram(dp, con, x, y,
            con->update.x, con->update.y, con->update.w, con->update.h);

        /* draw cursor */
        color_from_attr(dp, ATTR_DEFAULT, &fg, &bg);
        draw_font_char(dp, con->font, '_', x, y,
            con->curx * con->char_width, con->cury * con->char_height, fg, bg, 0, angle);

        if (flush == 1) {
            draw_flush(dp,
                x + con->update.x * con->char_width,
                y + con->update.y * con->char_height,
                (con->update.w - con->update.x) * con->char_width,
                (con->update.h - con->update.y) * con->char_height);
        }
        reset_dirty_region(con);
    }
}

/* output character at cursor location*/
void console_putchar(struct console *con, int c)
{
    switch (c) {
    case '\n':  goto scroll;
    case '\r':  con->curx = 0; goto update;
    case '\b':  if (--con->curx < 0) con->curx = 0; goto update;
    case '\t':  while (con->curx < con->cols-1 && (++con->curx & 7))
                    continue; goto update;
    case '\0':  return;
    case '\7':  return;
    }

    con->text_ram[con->cury * con->cols + con->curx] = (c & 255) | (ATTR_DEFAULT << 8);
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

void console_write(struct console *con, char *buf, size_t n)
{
    while (n-- != 0)
        console_putchar(con, *buf++ & 255);
}
#endif

#if !OLDWAY
extern void tmt_callback(tmt_msg_t m, TMT *vt, const void *a, void *p);

void console_write(struct console *con, char *buf, size_t n)
{
    tmt_write(con->vt, buf, n);
}

void console_putchar(struct console *con, int c)
{
    char buf[1];
    buf[0] = c;
    tmt_write(con->vt, buf, 1);
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
    const TMTUPDATE *update = &tmt_screen(con->vt)->update;
    Pixel fg, bg;

    con->dp = dp;   // FIXME for testing w/clear_screen()

    if (flush == 2)
        tmt_dirty(con->vt, 0, 0, con->cols, con->lines);

    if (update->dirty) {
        /* draw text bitmaps from adaptor RAM */
        draw_console_ram(dp, con, x, y,
            update->x, update->y, update->w, update->h);

        /* draw cursor */
        color_from_attr(dp, ATTR_DEFAULT, &fg, &bg);
        const TMTCURSOR *cursor = tmt_cursor(con->vt);
        con->curx = cursor->c;
        con->cury = cursor->r;
        if (!cursor->hidden) {
            draw_font_char(dp, con->font, '_', x, y, con->curx * con->char_width,
                con->cury * con->char_height, fg, bg, 0, angle);
        }

        if (flush == 1) {
            draw_flush(dp,
                x + update->x * con->char_width,
                y + update->y * con->char_height,
                (update->w - update->x) * con->char_width,
                (update->h - update->y) * con->char_height);
        }
        tmt_clean(con->vt);
    }
}
#endif

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
    con->cols = width;
    con->lines = height;
    console_load_font(con, NULL);       /* loads default font */

#if OLDWAY
    /* init text ram and update rect */
    for (i = 0; i < height; i++)
        clear_line(con, 0, con->cols - 1, i, ATTR_DEFAULT);
    console_movecursor(con, 0, 0);
#else
    con->vt = tmt_open(height, width, tmt_callback, NULL, NULL);
    if (!con->vt) return 0;
#endif
    return con;
}

int console_resize(struct console *con, int width, int height)
{
    con->cols = width;
    con->lines = height;
    draw_clear(con->dp);
#if !OLDWAY
    return tmt_resize(con->vt, height, width);
#endif
}
