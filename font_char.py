import sys
import itertools
import freetype

class Font(object):
  def __init__(self, filename, size):
    self.face = freetype.Face(filename)
    self.face.set_pixel_sizes(0, size)

  def glyph_for_character(self, char):
    # Let FreeType load the glyph for the given character and
    # tell it to render a monochromatic bitmap representation.
    self.face.load_char(char, freetype.FT_LOAD_RENDER |
                              freetype.FT_LOAD_TARGET_MONO)
    return Glyph.from_glyphslot(self.face.glyph)

  def render_character(self, char):
    glyph = self.glyph_for_character(char)
    return glyph.bitmap

class Glyph(object):
  def __init__(self, pixels, width, height):
    self.bitmap = Bitmap(width, height, pixels)

  @staticmethod
  def from_glyphslot(slot):
    """Construct and return a Glyph object from a FreeType GlyphSlot."""
    pixels = Glyph.unpack_mono_bitmap(slot.bitmap)
    width, height = slot.bitmap.width, slot.bitmap.rows
    return Glyph(pixels, width, height)

  @staticmethod
  def unpack_mono_bitmap(bitmap):
    """
    Unpack a freetype FT_LOAD_TARGET_MONO glyph bitmap into a bytearray where
    each pixel is represented by a single byte.
    """
    # Allocate a bytearray of sufficient size to hold the glyph bitmap.
    data = bytearray(bitmap.rows * bitmap.width)

    # Iterate over every byte in the glyph bitmap. Note that we're not
    # iterating over every pixel in the resulting unpacked bitmap --
    # we're iterating over the packed bytes in the input bitmap.
    for y in range(bitmap.rows):
      for byte_index in range(bitmap.pitch):

        # Read the byte that contains the packed pixel data.
        byte_value = bitmap.buffer[y * bitmap.pitch + byte_index]

        # We've processed this many bits (=pixels) so far. This determines
        # where we'll read the next batch of pixels from.
        num_bits_done = byte_index * 8

        # Pre-compute where to write the pixels that we're going
        # to unpack from the current byte in the glyph bitmap.
        rowstart = y * bitmap.width + byte_index * 8

        # Iterate over every bit (=pixel) that's still a part of the
        # output bitmap. Sometimes we're only unpacking a fraction of a byte
        # because glyphs may not always fit on a byte boundary. So we make sure
        # to stop if we unpack past the current row of pixels.
        for bit_index in range(min(8, bitmap.width - num_bits_done)):

          # Unpack the next pixel from the current glyph byte.
          bit = byte_value & (1 << (7 - bit_index))

          # Write the pixel to the output bytearray. We ensure that `off`
          # pixels have a value of 0 and `on` pixels have a value of 1.
          data[rowstart + bit_index] = 1 if bit else 0

    return data

class Bitmap(object):
  """
  A 2D bitmap image represented as a list of byte values. Each byte indicates
  the state of a single pixel in the bitmap. A value of 0 indicates that
  the pixel is `off` and any other value indicates that it is `on`.
  """
  def __init__(self, width, height, pixels=None):
    self.width = width
    self.height = height
    self.pixels = pixels or bytearray(width * height)

  def __repr__(self):
    """Return a string representation of the bitmap's pixels."""
    rows = ''
    for y in range(self.height):
        for x in range(self.width):
            rows += '*' if self.pixels[y * self.width + x] else ' '
        rows += '\n'
    return rows

fnt = Font("fonts/cour.ttf", 16)
ch = fnt.render_character("S")
print(repr(ch))
