CFLAGS=-I/usr/local/include/SDL -I/usr/local/include/freetype2 -DHAVE_SDL -O3 -ffast-math -flto -march=native -Wall -Wextra
LFLAGS=-L/usr/local/lib -lSDL -lfreetype -DHAVE_SDL -O3 -ffast-math -flto -march=native -Wall -Wextra

all: imagewriter

main.o: main.c
	$(CC) $(CFLAGS) -c -o main.o main.c

imagewriter.o: imagewriter.cpp
	$(CXX) $(CFLAGS) -c -o imagewriter.o imagewriter.cpp

imagewriter: imagewriter.o main.o
	$(CXX) $(LFLAGS) -o imagewriter main.o imagewriter.o

test: imagewriter
	./imagewriter Printer.txt

clean:
	rm -f *.o imagewriter
