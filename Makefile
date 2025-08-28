# GFX graphics library

CFLAGS = -Wall -Wno-missing-braces
LDLIBS += -lSDL2

# generated font files
GENFONTSRCS = cour_32.c cour_32_tt.c times_32.c times_32_tt.c lucida_32.c lucida_32_tt.c
GENFONTOBJS = $(GENFONTSRCS:.c=.o)

all: draw

%.o: %.ttf
	python3 writefont.py $*.ttf 32 -bpp 1 -c 0x20-0x7e > $*.c
	#python3 writefont.py $*.ttf 32 -bpp 1 -s" S" > $*.c
	$(CC) -c $*.c

cour_32.c: cour.ttf
	python3 writefont.py $^ 32 -bpp 1 -c 0x20-0x7e > $*.c

cour_32_tt.c: cour.ttf
	python3 writefont.py $^ 32 -bpp 8 -c 0x20-0x7e > $*.c

times_32.c: times.ttf
	python3 writefont.py $^ 32 -bpp 1 -c 0x20-0x7e > $*.c

times_32_tt.c: times.ttf
	python3 writefont.py $^ 32 -bpp 8 -c 0x20-0x7e > $*.c

lucida_32.c: lucida.ttf
	python3 writefont.py $^ 32 -bpp 1 -c 0x20-0x7e > $*.c

lucida_32_tt.c: lucida.ttf
	python3 writefont.py $^ 32 -bpp 8 -c 0x20-0x7e > $*.c

draw: draw.o rom8x16.o $(GENFONTOBJS)
	$(CC) -o $@ $^ $(LDLIBS)

clean:
	rm -f *.o draw $(GENFONTSRCS)
