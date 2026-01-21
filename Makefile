CFLAGS=-I/usr/local/include/SDL -I/usr/local/include/freetype2 -DHAVE_SDL -O3 -ffast-math -flto -march=native -Wall -Wextra
LFLAGS=-L/usr/local/lib -lSDL -lfreetype -DHAVE_SDL -O3 -ffast-math -flto -march=native -Wall -Wextra

all: main.c imagewriter.cpp
	c++ $(CFLAGS) -c -o imagewriter.o imagewriter.cpp
	cc $(CFLAGS) -c -o main.o main.c
	c++ $(LFLAGS) -o imagewriter main.o imagewriter.o

clean:
	rm -f *.o imagewriter
