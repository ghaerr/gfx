#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "draw.h"

static int oversamp = 24;           /* min 20 for no holes in diagonal oversampling */

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

/* convert character to font glyph index, return default glyph if not present */
static int glyph_offset(Font *font, unsigned int c)
{
    uint16_t first, last, offset = 0;
    uint16_t *r = font->range;

    if (r) {                        /* charcode range ordered by glyph index */
        do {
            first = r[0]; last = r[1];
            if (c >= first && c <= last)
                return c-first+offset;
            r += 2;
            offset += last - first + 1;
        } while (offset < font->size);
        return font->defaultglyph;
    }
    c -= font->firstchar;
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
