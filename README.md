# GFX

A simple but powerful internal framebuffer based graphics and event library with portability across SDL, bare metal and ELKS platforms.

## What can it do?

- Drawing - lines, rectangles, circles, area fills and blits, with clipping
- Fonts - antialiased Truetype fonts converted to C source, disk-loaded ROM fonts
- Text Rotation - Bitmap or antialiased text at any angle with rotated background bits
- Text Console - scrolled text regions using any font
- Backbuffered drawing in 32bpp ARGB or ABGR pixel format
- Simple platform independent API, some X11 conversion functions
- Backend - SDL or hardware framebuffer, non-buffered ELKS VGA coming

## Library design

The drawing functions all manipulate pixels in allocated Drawables, in fixed ARGB or ABGR format for speed, after which the back buffer is copied once per frame to SDL or the hardware framebuffer, usually without a conversion blit.

Fonts can be 1bpp bitmaps or 8bpp antialiased, using compiled-in font data converted using the conv_ttf_to_c.py Python script for a specific font height. ROM fonts using the .F16 binary format can also be loaded from disk.

Text display can be rotated dynamically, and oversampling is used to eliminate unwanted drdropouts of foreground or background pixels.

## Thanks to the work of others

Retrieving Truetype font data using Python is made possible using Nicolas P. Rougier's freetype-py binding at https://github.com/rougier/freetype-py. (Install using 'pip freetype-py').

The conv_ttf_to_c.py font conversion script comes originally from Russ Hughes' MicroPython LCD Driver project at https://github.com/russhughes/st7789py_mpy.

That python script uses font handling classes from Dan Bader's blog post on using freetype at https://dbader.org/blog/monochrome-font-rendering-with-freetype-and-python.

Negative glyph.left fixes are from Peter Hinch's MicroPython font handling project at https://github.com/peterhinch/micropython-font-to-py.

The C font rotation algorithm is based on Salvatore Sanfilippo's Python MicroFont rendering library at https://github.com/antirez/microfont.

## Gallery

Twin consoles showing antialiased vs bitmap fonts
![1](https://github.com/ghaerr/gfx/blob/master/Screenshots/courier.png)

Rotated text
![1](https://github.com/ghaerr/gfx/blob/master/Screenshots/courier_rotated.png)

Kumppa X11 screensaver converted using x11 conversion sublibrary
![1](https://github.com/ghaerr/gfx/blob/master/Screenshots/kumppa.png)

Converted Xswarm screensaver
![1](https://github.com/ghaerr/gfx/blob/master/Screenshots/swarm.png)
