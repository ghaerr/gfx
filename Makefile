# GFX graphics library

CFLAGS = -Wall
LDLIBS += -lSDL2

all: draw

%.o: %.ttf
	python3 writefont.py $*.ttf 32 -bpp 1 -c 0x20-0x7e > $*.c
	#python3 writefont.py $*.ttf 32 -bpp 1 -s" S" > $*.c
	$(CC) -c $*.c

draw: draw.o rom8x16.o cour.o cambria.o Vera.o
	$(CC) -o $@ $^ $(LDLIBS)

clean:
	rm -f *.o draw
