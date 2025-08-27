# text and graphics console emulator using SDL

#CFLAGS = -g -Wall -Werror
LDLIBS += -lSDL2

all: draw

%.o: %.ttf
	python3 writefont.py $*.ttf 32 -bpp 1 -c 0x20-0x7e > $*.c
	#python3 writefont.py cour.ttf 32 -bpp 1 -s" S" > file.c
	$(CC) -c $*.c

draw: draw.o rom8x16.o cambria.o
	$(CC) -o $@ $^ $(LDLIBS)

clean:
	rm -f *.o draw
