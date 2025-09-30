# GFX graphics library

ifdef ELKS
CC = ia16-elf-gcc
ELKSDIR = /Users/greg/net/elks-gh
CFLAGS += -melks-libc -mtune=i8086 -Os -mcmodel=small -DELKS=1
CFLAGS += -I$(ELKSDIR)/libc/include -I$(ELKSDIR)/elks/include
CFLAGS += -Wno-packed-bitfield-compat
CFLAGS += -Os
endif
CFLAGS += -O3
CFLAGS += -Wall -Wno-missing-braces -Wno-unused-variable
LDLIBS += -lSDL2

# GFX files
GFXOBJS = font.o console.o draw.o
TERMOBJS = tmt.o mb.o openpty.o

# generated font files
GENFONTSRCS = fonts/cour_20x37_1.c fonts/cour_21x37_8.c fonts/cour_11x19_8.c
GENFONTSRCS += fonts/unifont_8x16_1.c
GENFONTSRCS += fonts/mssans_11x13_8.c
GENFONTSRCS += fonts/times_30x37_8.c
GENFONTOBJS = $(GENFONTSRCS:.c=.o)
GENFONTOBJS += rom_8x16_1.o

gfx: draw

all: gfx swarm kumppa

%.o: %.ttf
	python3 conv_ttf_to_c.py $*.ttf 32 -bpp 1 -c 0x20-0x7e > $*.c
	#python3 conv_ttf_to_c.py $*.ttf 32 -bpp 1 -s" S" > $*.c
	$(CC) -c $*.c

fonts/cour_20x37_1.o: fonts/cour.ttf
	python3 conv_ttf_to_c.py $^ 37 -bpp 1 -c 0x20-0x17F,0x2500-0x25FF,0x2610 > $*.c
	$(CC) -I. -c $*.c -o $*.o

fonts/cour_21x37_8.o: fonts/cour.ttf
	python3 conv_ttf_to_c.py $^ 37 -bpp 8 -c 0x20-0x17F,0x2500-0x25FF,0x2610 > $*.c
	$(CC) -I. -c $*.c -o $*.o

fonts/cour_11x19_8.o: fonts/cour.ttf
	python3 conv_ttf_to_c.py $^ 19 -bpp 8 -c 0x20-0x17F,0x2500-0x25FF,0x2610 > $*.c
	$(CC) -I. -c $*.c -o $*.o

# GNU Unifont 17.0.0.1 (Basic Latin thru Spacing Modifier + Box Drawing 0x2500)
# Box Drawing 0x2500-0x25ff = 9472-9727 and Ballot Box = 0x2610
# Excepting 16x16 glyphs: 173 SHY, 9711 O
fonts/unifont_8x16_1.o: fonts/unifont.otf
	python3 conv_ttf_to_c.py $^ 16 -bpp 1 -c 32-126,160-172,174-767,9472-9710,9712-9727,0x2610 > $*.c
	$(CC) -I. -c $*.c -o $*.o

# Microsoft Sans Serif micross.ttf
fonts/mssans_11x13_8.o: fonts/mssans.ttf
	python3 conv_ttf_to_c.py $^ 13 -bpp 8 -c 0x20-0x17F,0x2610 > $*.c
	$(CC) -I. -c $*.c -o $*.o

fonts/times_30x37_8.o: fonts/times.ttf
	python3 conv_ttf_to_c.py $^ 37 -bpp 8 -c 0x20-0x17F,0x2610 > $*.c
	$(CC) -I. -c $*.c -o $*.o

draw.o tmt.o: tmt.h

draw: main.o sdl.o $(GFXOBJS) $(GENFONTOBJS) tmt.o mb.o openpty.o
	$(CC) -o $@ $^ $(LDLIBS)

swarm: swarm.o x11.o draw.o sdl.o
	$(CC) -o $@ $^ $(LDLIBS)

kumppa: kumppa.o yarandom.o x11.o draw.o sdl.o
	$(CC) -o $@ $^ $(LDLIBS)

clean:
	rm -f *.o fonts/*.o draw $(GENFONTSRCS) swarm kumppa
