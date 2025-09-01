/* quick X11 to GFX conversions */
#include <stdlib.h>
#include <stdio.h>
#include "draw.h"
#include "x11.h"

void ya_rand_init(int);

Display *XOpenDisplay(char *display)
{
    Drawable *dp;
    struct sdl_window *sdl;

    if (!sdl_init()) exit(1);
    //if (!(dp = create_drawable(MWPF_DEFAULT, 640, 400))) exit(2);
    if (!(dp = create_drawable(MWPF_DEFAULT, 800, 800))) exit(2);
    if (!(sdl = sdl_create_window(dp))) exit(3);

    return dp;
}

GC XCreateGC(Display *dpy, Window d, unsigned long valuemask, XGCValues *values)
{
    GC gc = malloc(sizeof(struct gc));

    gc->fg = WhitePixel(dpy, 0);
    gc->bg = BlackPixel(dpy, 0);
    if (valuemask & GCForeground)
        gc->fg = values->foreground;
    if (valuemask & GCbackground)
        gc->bg = values->background;
    return gc;
}

int XCopyGC(Display *dpy, GC src, unsigned long valuemask, GC dest)
{
    *dest = *src;
    return 1;
}

int XFreeGC(Display *dpy, GC gc)
{
    free(gc);
    return 1;
}

int XSetForeground(Display *dpy, GC gc, unsigned long foreground)
{
    gc->fg = foreground;
    return 1;
}

int XSetBackground(Display *dpy, GC gc, unsigned long background)
{
    gc->bg = background;
    return 1;
}

int XAllocColor(Display *dpy, Colormap colormap, XColor *xc)
{
    xc->pixel = RGB(xc->red >> 8, xc->green >> 8, xc->blue >> 8);
    return 1;
}

int XDrawSegments(Display *dpy, Window d, GC gc, XSegment *segments, int nsegments)
{
    int i;

    d->fgcolor = gc->fg;
    for (i = 0; i < nsegments; i++)
        draw_line(d, segments[i].x1, segments[i].y1, segments[i].x2, segments[i].y2);

    return 1;
}

int XDrawLine (Display *dpy, Window d, GC gc, int x1, int y1, int x2, int y2)
{
    d->fgcolor = gc->fg;
    draw_line(d, x1, y1, x2, y2);
    return 1;
}

int XFillRectangle(Display *dpy, Window d, GC gc, int x, int y, int width, int height)
{
    d->fgcolor = gc->fg;
    draw_fill_rect(d, x, y, width, height);
    return 1;
}

int XCopyArea(Display *dpy, Window src, Window dest, GC gc,
    int src_x, int src_y, int width, int height, int dest_x, int dest_y)
{
    //printf("w/h %d,%d src %d,%d dst %d,%d\n", width, height,
        //src_x, src_y, dest_x, dest_y);
    draw_blit(dest, dest_x, dest_y, width, height, src, src_x, src_y);
    return 1;
}

#include <SDL2/SDL.h>
int sdl_key(Uint8 state, SDL_Keysym sym);

static int sdl_pollevent(void)
{
    int c;
    SDL_Event event;

    if (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                return 1;

            case SDL_KEYDOWN:
                c = sdl_key(event.key.state, event.key.keysym);
                if (c == 'q') return 1;
                break;
        }
    }

    return 0;
}

int XSync(Display *dpy, Bool discard)
{
    draw_flush(dpy);
    //draw_clear(dpy);
    if (sdl_pollevent())
        exit(0);
    return 1;
}
