# text and graphics console emulator using SDL

#CFLAGS = -g -Wall -Werror
LDLIBS += -lSDL2

all: draw

file.c:
	python3 writefont.py cour.ttf 16 -c 0x20-0x7e > file.c

draw: draw.o rom8x16.o file.o
	$(CC) -o $@ $^ $(LDLIBS)

clean:
	rm -f *.o draw
