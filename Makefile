CFLAGS=-I/usr/local/include/SDL -I/usr/local/include/freetype2 -DHAVE_SDL -O3 -ffast-math -flto -march=native -Wall -Wextra
LFLAGS=-L/usr/local/lib -lSDL -lfreetype -DHAVE_SDL -O3 -ffast-math -flto -march=native -Wall -Wextra

all: imagewriter

main.o: main.c serial.h
	$(CC) $(CFLAGS) -c -o main.o main.c

serial_posix.o: serial_posix.c serial.h
	$(CC) $(CFLAGS) -c -o serial_posix.o serial_posix.c

imagewriter.o: imagewriter.cpp
	$(CXX) $(CFLAGS) -c -o imagewriter.o imagewriter.cpp

imagewriter: imagewriter.o main.o serial_posix.o
	$(CXX) $(LFLAGS) -o imagewriter main.o serial_posix.o imagewriter.o

test: imagewriter
	./imagewriter Printer.txt

clean:
	rm -f *.o imagewriter
