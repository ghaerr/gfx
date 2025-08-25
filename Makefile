# text and graphics console emulator using SDL

#CFLAGS = -g -Wall -Werror
LDLIBS += -lSDL2

all: draw

draw: draw.o rom8x16.o
	$(CC) -o $@ $^ $(LDLIBS)

clean:
	rm -f *.o draw
