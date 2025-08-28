#!/usr/bin/env python3
"""
Convert characters from a truetype font to a python bitmap for use with the bitmap or write method.
The chango, noto_fonts and proverbs examples use converted TrueType fonts.

.. seealso::
    - :ref:`chango.py<chango>`.
    - :ref:`noto_fonts.py<noto_fonts>`.
    - :ref:`proverbs.py<proverbs>`.

Example
^^^^^^^

.. code-block:: console

    # convert the Chango-Regular.ttf to a python bitmap module with approximately 32 pixel high characters
    ./write_font_converter.py Chango-Regular.ttf 32 -c 0x20-0x7f > chango_32.py

.. code-block:: python

    import tft_config
    import chango_32
    tft = tft_config.config(1)
    tft.write(chango_32, "Hello World!", 0, 0)

Usage
^^^^^

.. code-block:: console

    usage: write_font_converter.py [-h] [-width FONT_WIDTH] (-c CHARACTERS | -s STRING) font_file font_height

    Convert characters from a truetype font to a python bitmap for use with the bitmap method in the st7789 and ili9342 drivers.

    positional arguments:
    font_file             name of font file to convert.
    font_height           size of font to create bitmaps from.

    optional arguments:
    -h, --help            show this help message and exit
    -width FONT_WIDTH, --font_width FONT_WIDTH
                            width of font to create bitmaps from.

    character selection:
    characters from the font to include in the bitmap.

    -c CHARACTERS, --characters CHARACTERS
                            integer or hex character values and/or ranges to include. For example: "65, 66, 67" or "32-127" or "0x30-0x39,
                            0x41-0x5a"
    -s STRING, --string STRING
                            string of characters to include For example: "1234567890-."

"""


# -*- coding: utf-8 -*-
# Needs freetype-py>=1.0

# Font handling classes are from Dan Bader blog post on using freetype
# http://dbader.org/blog/monochrome-font-rendering-with-freetype-and-python
#
# Modified by Russ Hughes, Mar 2021 to write bitmap modules compatible with
# the MicroPython ili9342c driver at https://github.com/russhughes/ili9342c_mpy
# and the st7789 driver at https://github.com/russhughes/st7789_mpy.
#
# The Negative glyph.left fix is from peterhinch's font conversion program
# https://github.com/peterhinch/micropython-font-to-py
# https://github.com/peterhinch/micropython-font-to-py/issues/21
# Handle negative glyph.left correctly (capital J),
# also glyph.width > advance (capital K and R).
#

# The MIT License (MIT)
#
# Copyright (c) 2013 Daniel Bader (http://dbader.org)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.


import sys
import shlex
import argparse
import bisect
import itertools
import freetype
import struct
import os


def to_int(string):
    """
    Convert a string to an integer.

    Args:
        str (str): The string to convert to an integer.

    Returns:
        int: The converted integer value.

    """

    return int(string, base=16) if string.startswith("0x") else int(string)


def get_chars(string):
    """
    Get a string of characters based on the given input.

    Args:
        string (str): The input string containing character ranges separated by commas.

    Returns:
        str: A string of characters formed by combining the character ranges.
    """
    chars = []
    for ele in string.split(","):
        char_range = list(map(to_int, ele.split("-")))
        chars.extend(chr(char) for char in range(char_range[0], char_range[-1] + 1))
    return "".join(chars)


def wrap_list(lst, items_per_line=8):
    """
    Wrap a list of items into a formatted string representation.

    Args:
        lst (list): The list of items to wrap.
        items_per_line (int): The number of items to include per line (default: 8).

    Returns:
        str: A formatted string representation of the wrapped list.
    """
    lines = [
        ", ".join(f"0x{x:02x}" for x in lst[i : i + items_per_line])
        for i in range(0, len(lst), items_per_line)
    ]
    return "[\n    " + ",\n    ".join(lines) + "]"


def wrap_bytes(lst, items_per_line=16):
    """
    Wrap a list of bytes into a formatted string representation.

    Args:
        lst (list): The list of bytes to wrap.
        items_per_line (int): The number of bytes to include per line (default: 16).

    Returns:
        str: A formatted string representation of the wrapped bytes.
    """
    lines = [
        "".join(f"\\x{x:02x}" for x in lst[i : i + items_per_line])
        for i in range(0, len(lst), items_per_line)
    ]
    return "    b'" + "'\\\n    b'".join(lines) + "'"


def wrap_short(lst, items_per_line=8):
    lines = [
        "".join(f"0x{x:04x}," for x in lst[i : i + items_per_line])
        for i in range(0, len(lst), items_per_line)
    ]
    return "    " + "\n    ".join(lines)


def wrap_long(lst, items_per_line=4):
    lines = [
        "".join(f"0x{x:08x}," for x in lst[i : i + items_per_line])
        for i in range(0, len(lst), items_per_line)
    ]
    return "    " + "\n    ".join(lines)


def wrap_hex(lst, items_per_line=8):
    lines = [
        "".join(f"0x{x:02x}," for x in lst[i : i + items_per_line])
        for i in range(0, len(lst), items_per_line)
    ]
    return "    " + "\n    ".join(lines)


class Bitmap(object):
    """
    A 2D bitmap image represented as a list of byte values. Each byte indicates
    the state of a single pixel in the bitmap. A value of 0 indicates that the
    pixel is `off` and any other value indicates that it is `on`.
    """

    def __init__(self, width, height, pixels=None):
        self.width = int(width)
        self.height = int(height)
        self.pixels = pixels or bytearray(width * height)

    def __repr__(self):
        """Return a string representation of the bitmap's pixels."""
        rows = ""
        for y in range(self.height):
            row = "// "
            for x in range(self.width):
                row += "#" if self.pixels[y * self.width + x] else "."
            row += "\n"
            rows += row
        return rows

    def bit_string(self):
        """Return a binary string representation of the bitmap's pixels."""
        pitch = (self.width + 7) & ~7
        if bpp == 1:
            return "".join(
                "1" if x < self.width and self.pixels[y * self.width + x] else "0"
                for y, x in itertools.product(range(self.height), range(pitch))
            )
        else:
            return "".join(
                f"{self.pixels[y * self.width + x]:02x}"
                for y, x in itertools.product(range(self.height), range(self.width))
            )

    def bitblt(self, src, x, y):
        """Copy all pixels from `src` into this bitmap"""
        srcpixel = 0
        dstpixel = y * self.width + x
        row_offset = self.width - src.width

        for _ in range(src.height):
            for _ in range(src.width):
                # Perform an OR operation on the destination pixel and the
                # source pixel because glyph bitmaps may overlap if character
                # kerning is applied, e.g. in the string "AVA", the "A" and "V"
                # glyphs must be rendered with overlapping bounding boxes.
                self.pixels[dstpixel] = self.pixels[dstpixel] or src.pixels[srcpixel]
                srcpixel += 1
                dstpixel += 1
            dstpixel += row_offset


class Glyph(object):
    """
    A single character glyph representation.
    """

    def __init__(self, pixels, width, height, top, left, advance_width):
        self.bitmap = Bitmap(width, height, pixels)

        # The glyph bitmap's top-side bearing, i.e. the vertical distance from
        # the baseline to the bitmap's top-most scanline.
        self.top = top
        self.left = left
        # Ascent and descent determine how many pixels the glyph extends
        # above or below the baseline.
        self.descent = max(0, self.height - self.top)
        self.ascent = max(0, max(self.top, self.height) - self.descent)

        # The advance width determines where to place the next character
        # horizontally, that is, how many pixels we move to the right to draw
        # the next glyph.
        self.advance_width = advance_width

    @property
    def width(self):
        """
        Get the width of the glyph bitmap.

        Returns:
            int: The width of the glyph bitmap.
        """

        return self.bitmap.width

    @property
    def height(self):
        """
        Get the height of the glyph bitmap.

        Returns:
            int: The height of the glyph bitmap.
        """

        return self.bitmap.height

    @staticmethod
    def from_glyphslot(slot):
        """Construct and return a Glyph object from a FreeType GlyphSlot."""
        pixels = Glyph.unpack_mono_bitmap(slot.bitmap)
        width, height = slot.bitmap.width, slot.bitmap.rows
        top = slot.bitmap_top
        left = slot.bitmap_left

        # The advance width is given in FreeType's 26.6 fixed point format,
        # which means that the pixel values are multiples of 64.
        advance_width = slot.advance.x // 64

        return Glyph(pixels, width, height, top, left, advance_width)

    @staticmethod
    def unpack_mono_bitmap(bitmap):
        """
        Unpack a freetype FT_LOAD_TARGET_MONO glyph bitmap into a bytearray
        where each pixel is represented by a single byte.
        """
        # Allocate a bytearray of sufficient size to hold the glyph bitmap.
        data = bytearray(bitmap.rows * bitmap.width)

        # Iterate over every byte in the glyph bitmap. Note that we're not
        # iterating over every pixel in the resulting unpacked bitmap --
        # we're iterating over the packed bytes in the input bitmap.
        for y, byte_index in itertools.product(range(bitmap.rows), range(bitmap.pitch)):
            # Read the byte that contains the packed pixel data.
            byte_value = bitmap.buffer[y * bitmap.pitch + byte_index]

            # We've processed this many bits (=pixels) so far. This
            # determines where we'll read the next batch of pixels from.
            num_bits_done = byte_index * 8

            if bpp == 8:
                data[y * bitmap.width + byte_index] = byte_value
            else:
                # Pre-compute where to write the pixels that we're going
                # to unpack from the current byte in the glyph bitmap.
                rowstart = y * bitmap.width + byte_index * 8

                # Iterate over every bit (=pixel) that's still a part of the
                # output bitmap. Sometimes we're only unpacking a fraction of a
                # byte because glyphs may not always fit on a byte boundary. So
                # we make sure to stop if we unpack past the current row of
                # pixels.
                for bit_index in range(min(8, bitmap.width - num_bits_done)):
                    # Unpack the next pixel from the current glyph byte.
                    bit = byte_value & (1 << (7 - bit_index))

                    # Write the pixel to the output bytearray. We ensure that
                    # `off` pixels have a value of 0 and `on` pixels have a
                    # value of 1.
                    data[rowstart + bit_index] = 1 if bit else 0

        return data


class Font(object):
    """
    A truetype font representation.
    """
    def __init__(self, filename, width, height):
        self.face = freetype.Face(filename)
        self.face.set_pixel_sizes(width, height)

    def glyph_for_character(self, char):
        """
        Get the glyph representation for the given character.

        Args:
            char (str): The character to get the glyph for.

        Returns:
            Glyph: The glyph representation of the character.
        """

        # Let FreeType load the glyph for the given character and tell it to
        # render a monochromatic bitmap representation.
        if bpp == 1:
            flags = freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_MONO
        else:
            flags = freetype.FT_LOAD_RENDER | freetype.FT_LOAD_TARGET_NORMAL

        self.face.load_char(char, flags)

        return Glyph.from_glyphslot(self.face.glyph)

    def render_character(self, char):
        """
        Render the given character into a bitmap representation.

        Args:
            char (str): The character to render.

        Returns:
            Bitmap: The bitmap representation of the rendered character.
        """

        glyph = self.glyph_for_character(char)
        return glyph.bitmap

    def text_dimensions(self, text):
        """
        Return (width, height, baseline) of `text` rendered in the current
        font.
        """
        width = 0
        max_ascent = 0
        max_descent = 0

        # For each character in the text string we get the glyph
        # and update the overall dimensions of the resulting bitmap.
        for char in text:
            glyph = self.glyph_for_character(char)
            max_ascent = max(max_ascent, glyph.ascent)
            max_descent = max(max_descent, glyph.descent)

            if glyph.left >= 0:
                char_width = int(max(glyph.advance_width, glyph.width + glyph.left))
            else:
                char_width = int(max(glyph.advance_width - glyph.left, glyph.width))

            width += char_width

        height = max_ascent + max_descent
        return (width, height, max_descent)

    def write_python(self, text, font_file):
        """
        Render the given `text` into a python bitmap module.
        """
        _, height, baseline = self.text_dimensions(text)

        bits = []
        widths = []
        offsets = []
        offset = 0
        startchar = text[0]
        numchars = len(text)

        # write out C source
        cmd_line = " ".join(map(shlex.quote, sys.argv))
        print( "/*")
        print(f" * Converted using: {cmd_line}")
        print( " */")
        print('#include "font.h"')
        print()

        for char in text:
            glyph = self.glyph_for_character(char)

            # Negative glyph.left fix from peterhinch
            # https://github.com/peterhinch/micropython-font-to-py
            #
            # https://github.com/peterhinch/micropython-font-to-py/issues/21
            # Handle negative glyph.left correctly (capital J),
            # also glyph.width > advance (capital K and R).

            if glyph.left >= 0:
                char_width = int(max(glyph.advance_width, glyph.width + glyph.left))
                left = glyph.left
            else:
                char_width = int(max(glyph.advance_width - glyph.left, glyph.width))
                left = 0

            # save the bit offset and width of the current glyph
            offsets.append(offset)
            widths.append(char_width)
            outbuffer = Bitmap(char_width, height)

            # The vertical drawing position should place the glyph
            # on the baseline as intended.
            y = height - glyph.ascent - baseline
            outbuffer.bitblt(glyph.bitmap, left, y)
            print(f"// index: {ord(char)} '{char}' {char_width}x{height}")
            print(outbuffer)

            # convert bitmap to ascii bitmap string
            bit_string = outbuffer.bit_string()
            bits.append(bit_string)
            if bpp == 1:
                offset += len(bit_string) // 8
            else:
                offset += len(bit_string) // 2

        # join all the bitmap strings together
        bit_string = "".join(bits)

        # escape '\' and '"' characters for char_map
        char_map = text.replace("\\", "\\\\").replace('"', '\\"')

        max_width = max(widths)

        print("static unsigned char widths[] = {")
        print(wrap_hex(widths))
        print("};\n")

        byte_offsets = bytearray()
        bytes_table = [0xFF, 0xFFFF, 0xFFFFFF, 0xFFFFFFFF]
        offset_width = bisect.bisect_left(bytes_table, offset, 0, 3) + 1
        if offset_width == 3:
            offset_width = 4
        for offset in offsets:
            byte_offsets.extend(offset.to_bytes(offset_width, "little"))

        print(f"// OFFSET_WIDTH = {offset_width}")
        if offset_width == 1:
            print("static unsigned char offsets[] = {")
            print(wrap_hex(byte_offsets))
        if offset_width == 2:
            print("static unsigned short offsets[] = {")
            byte_offsets16 = struct.unpack("<" + "H" * (len(byte_offsets) // 2), byte_offsets)
            print(wrap_short(byte_offsets16))
        if offset_width == 4:
            print("static unsigned int offsets[] = {")
            byte_offsets32 = struct.unpack("<" + "I" * (len(byte_offsets) // 4), byte_offsets)
            print(wrap_long(byte_offsets32))
        print("};\n")

        print("static unsigned char bits[] = {")
        if bpp == 1:
            byte_values = [
                int(bit_string[i : i + 8], 2) for i in range(0, len(bit_string), 8)
            ]
        else:
            byte_values = [
                int(bit_string[i : i + 2], 16) for i in range(0, len(bit_string), 2)
            ]
        print(wrap_hex(byte_values))
        print("};\n")

        fontname,_ = os.path.splitext(os.path.basename(font_file))
        fontname += "_" + str(args_height)
        if bpp == 8: fontname += "_tt"
        print(f"// NAME = {fontname}")
        print(f"// HEIGHT = {args_height}")
        print(f"// MAX_HEIGHT = {height}")
        print(f"// MAX_WIDTH = {max_width}")
        print(f"// BPP = {bpp}")
        print(f'// MAP = "{char_map}"')
        print()

        print(f"struct font font_{fontname} =", "{")
        print(f'    "{fontname}",')
        print(f"    {max_width},     /* maxwidth */")
        print(f"    {height},     /* height */")
        print(f"    {height-baseline},     /* ascent */")
        print(f"    {ord(startchar)},     /* firstchar */")
        print(f"    {numchars},     /* size */")
        print( "    bits,")
        print( "    (unsigned char *)offsets,")
        print( "    widths,")
        print(f"    {ord(startchar)},     /* defaultchar */")
        print( "    0,      /* bits_size */")
        print(f"    {bpp},      /* bpp */")
        print(f"    1,      /* bits_width */")
        print(f"    {offset_width}       /* offset_width */")
        print( "};")

def main():
    """
    Convert characters from a truetype font to a python bitmap for use with the bitmap method.
    """
    parser = argparse.ArgumentParser(
        description=(
            "Convert characters from a truetype font to a python bitmap for use "
            "with the bitmap method in the st7789 and ili9342 drivers."
        ),
    )

    parser.add_argument("font_file", help="name of font file to convert.")

    parser.add_argument(
        "font_height", type=int, default=8, help="size of font to create bitmaps from."
    )

    global bpp
    parser.add_argument(
        "-bpp",
        "--bpp",
        type=int,
        default=None,
        help="bitmap or alpha blended font.",
    )

    parser.add_argument(
        "-width",
        "--font_width",
        type=int,
        default=None,
        help="width of font to create bitmaps from.",
    )

    group = parser.add_argument_group(
        "character selection", "characters from the font to include in the bitmap."
    )

    excl = group.add_mutually_exclusive_group(required=True)
    excl.add_argument(
        "-c",
        "--characters",
        help='''integer or hex character values and/or ranges to include.
        For example: "65, 66, 67" or "32-127" or "0x30-0x39, 0x41-0x5a"''',
    )

    excl.add_argument(
        "-s",
        "--string",
        help='''string of characters to include
        For example: "1234567890-."''',
    )

    args = parser.parse_args()
    font_file = args.font_file
    global args_height
    args_height = height = args.font_height
    width = args.font_height if args.font_width is None else args.font_width
    bpp = 8 if args.bpp is None else args.bpp
    characters = get_chars(args.characters) if args.string is None else args.string

    fnt = Font(font_file, width, height)
    fnt.write_python(characters, font_file)


main()
