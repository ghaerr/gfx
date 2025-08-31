# GFX graphics library

CFLAGS = -Wall -Wno-missing-braces
LDLIBS += -lSDL2

# generated font files
GENFONTSRCS = fonts/cour_32.c fonts/cour_32_tt.c
#GENFONTSRCS += fonts/times_32.c
GENFONTSRCS += fonts/times_32_tt.c
#GENFONTSRCS += fonts/lucida_32.c fonts/lucida_32_tt.c
GENFONTOBJS = $(GENFONTSRCS:.c=.o)

gfx: draw

all: gfx swarm

%.o: %.ttf
	python3 writefont.py $*.ttf 32 -bpp 1 -c 0x20-0x7e > $*.c
	#python3 writefont.py $*.ttf 32 -bpp 1 -s" S" > $*.c
	$(CC) -c $*.c

fonts/cour_32.o: fonts/cour.ttf
	python3 writefont.py $^ 32 -bpp 1 -c 0x20-0x7e > $*.c
	$(CC) -I. -c $*.c -o $*.o

fonts/cour_32_tt.o: fonts/cour.ttf
	python3 writefont.py $^ 32 -bpp 8 -c 0x20-0x7e > $*.c
	$(CC) -I. -c $*.c -o $*.o

fonts/times_32.o: fonts/times.ttf
	python3 writefont.py $^ 32 -bpp 1 -c 0x20-0x7e > $*.c
	$(CC) -I. -c $*.c -o $*.o

fonts/times_32_tt.o: fonts/times.ttf
	python3 writefont.py $^ 32 -bpp 8 -c 0x20-0x7e > $*.c
	$(CC) -I. -c $*.c -o $*.o

fonts/lucida_32.o: fonts/lucida.ttf
	python3 writefont.py $^ 32 -bpp 1 -c 0x20-0x7e > $*.c
	$(CC) -I. -c $*.c -o $*.o

fonts/lucida_32_tt.o: fonts/lucida.ttf
	python3 writefont.py $^ 32 -bpp 8 -c 0x20-0x7e > $*.c
	$(CC) -I. -c $*.c -o $*.o

draw: draw.o rom8x16.o $(GENFONTOBJS)
	$(CC) -o $@ $^ $(LDLIBS)

swarm: swarm.c x11.c draw.c rom8x16.c
	cc -DNOMAIN -o $@ $^ -lSDL2

kumppa: kumppa.c x11.c draw.c rom8x16.c
	cc -DNOMAIN -o $@ $^ -lSDL2

xswarm: xswarm.c
	cc -DNOMAIN -o $@ $^ -lX11

clean:
	rm -f *.o fonts/*.o draw $(GENFONTSRCS) swarm xswarm
