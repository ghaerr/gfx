/*
 * GFX library graphics routines for host (SDL) and framebuffer (embedded).
 *
 * Currently displays rotated bitmap and alpha blended glyphs echoed from keypresses.
 * Newline will result in scrolling screen.
 *
 * Sep 2022 Greg Haerr <greg@censoft.com>
 * Aug 2025 Greg Haerr rewritten, supports bitmap and TTF antialiased fonts
 * Sep 2025 Greg Haerr added ANSI terminal emulation and unicode
 */

//#include <stdint.h>
#include <stdio.h>
#include <string.h>
//#include <fcntl.h>
//#include <unistd.h>
//#include <sys/select.h>
#include "draw.h"

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

void draw_clear(Drawable *dp)
{
    Pixel save = dp->fgcolor;

    dp->fgcolor = dp->bgcolor;
    for (int y = 0; y < dp->height; y++)
        draw_hline(dp, 0, dp->width - 1, y);
    dp->fgcolor = save;
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
