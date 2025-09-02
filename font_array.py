import freetype
face = freetype.Face("fonts/cour.ttf")
#face.set_char_size(16*64)
face.set_pixel_sizes(0,16)
#face.load_char('S',freetype.FT_LOAD_RENDER|freetype.FT_LOAD_TARGET_MONO)
face.load_char('W',freetype.FT_LOAD_RENDER|freetype.FT_LOAD_TARGET_NORMAL)
print(face.glyph.bitmap.width)
print(face.glyph.bitmap.rows)
bitmap = face.glyph.bitmap
print(bitmap.pitch)
print(bitmap.buffer)
