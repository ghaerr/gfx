# GFX

A simple but powerful internal framebuffer based graphics and event library with portability across SDL, bare metal and ELKS platforms.

## What can it do?

- Drawing - lines, rectangles, circles, area fills and blits, with clipping
- Fonts - antialiased Truetype fonts converted to C source, disk-loaded ROM fonts
- Text Rotation - Bitmap or antialiased text at any angle with rotated background bits
- Text Console - scrolled text regions using any font
- Backbuffered drawing in 32bpp ARGB or ABGR pixel format
- Simple platform independent API
- Event Handling - keyboard and mouse event handling (coming)
- Limited X11 function conversion, used for testing X11 graphics with library
- Backend - SDL or hardware framebuffer, non-buffered ELKS VGA (coming)

## Library design

The drawing functions all manipulate pixels in allocated Drawables, in fixed ARGB or ABGR format for speed, after which the back buffer is copied once per frame to SDL or the hardware framebuffer, usually without a conversion blit.

Fonts can be 1bpp bitmaps or 8bpp antialiased, using compiled-in font data converted using the conv_ttf_to_c.py Python script for a specific font height. ROM fonts using the .F16 (or .F19, etc) binary format can also be loaded from disk.

Text display can be rotated dynamically, and oversampling is used to eliminate unwanted drdropouts of foreground or background pixels.

The GFX library is under construction; currently it is contained entirely in the file draw.c.

## Thanks to the work of others

Retrieving Truetype font data using Python is made possible using Nicolas P. Rougier's freetype-py binding at https://github.com/rougier/freetype-py. (Install using 'pip freetype-py').

The conv_ttf_to_c.py font conversion script comes originally from Russ Hughes' MicroPython LCD Driver project at https://github.com/russhughes/st7789py_mpy and documentation at https://russhughes.github.io/st7789py_mpy/fonts.html#true-type-fonts.

That python script uses font handling classes from Dan Bader's blog post on using freetype at https://dbader.org/blog/monochrome-font-rendering-with-freetype-and-python.

Negative glyph.left fixes are from Stephen Irons' contribution to Peter Hinch's MicroPython font handling project at https://github.com/peterhinch/micropython-font-to-py.

The C font rotation algorithm is based on Salvatore Sanfilippo's Python MicroFont rendering library at https://github.com/antirez/microfont.

Some of the included *.F16 and *.F19 fonts are from Joseph Gil's fntcol16.zip package available from http://ftp.lanet.lv/ftp/mirror/x2ftp/msdos/programming/misc/fntcol16.zip and https://github.com/dokkaebi-project/font-archive.

The Kumppa demo was converted from Teemu Suutari's X11 screensaver, included in Jamie Zawinski's [XScreenSaver collection](https://www.jwz.org/xscreensaver/screenshots) available from  https://www.jwz.org/xscreensaver/xscreensaver-6.12.tar.gz and https://github.com/Zygo/xscreensaver. The X11 demos are being used along with some quick-and-dirty X11-to-GFX conversion functions to test the GFX graphics routines for correctness.

## Gallery

Twin consoles showing antialiased vs bitmap fonts
![1](https://github.com/ghaerr/gfx/blob/master/Screenshots/courier.png)

Rotated text
![1](https://github.com/ghaerr/gfx/blob/master/Screenshots/courier_rotated.png)

Kumppa X11 screensaver converted using x11 conversion sublibrary, uses source clipping
![1](https://github.com/ghaerr/gfx/blob/master/Screenshots/kumppa.png)

Converted Xswarm screensaver
![1](https://github.com/ghaerr/gfx/blob/master/Screenshots/swarm.png)

Unicode ANSI terminal emulator
![1](https://github.com/ghaerr/gfx/blob/master/Screenshots/unicode_terminal_emulator.png)
