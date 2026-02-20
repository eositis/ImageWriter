# Use portable flags to avoid SIGILL on some macOS/SDL setups.
# -march=native and -flto can trigger illegal instruction crashes with SDL/FreeType.
# For max perf on your machine only: make CFLAGS="$(CFLAGS) -march=native -flto" LFLAGS="$(LFLAGS) -march=native -flto"
CFLAGS=-I/usr/local/include/SDL -I/usr/local/include/freetype2 -DHAVE_SDL -O2 -Wall -Wextra
LFLAGS=-L/usr/local/lib -lSDL -lfreetype -DHAVE_SDL -O2 -Wall -Wextra

all: imagewriter

main.o: main.c serial.h
	@if [ ! -f .build_number ]; then echo 0 > .build_number; fi; \
	echo $$(($$(cat .build_number) + 1)) > .build_number; \
	echo "#define BUILD_NUMBER $$(cat .build_number)" > build_number.h
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
