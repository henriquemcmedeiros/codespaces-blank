CC = gcc
CFLAGS = -Wall -Wextra -O2 `pkg-config fuse3 --cflags`
LIBS = `pkg-config fuse3 --libs`

OBJ = main.o fuseSteganografia.o manipuladorBmp.o

all: fuseSteganografia

fuseSteganografia: $(OBJ)
	$(CC) $(CFLAGS) -o fuseSteganografia $(OBJ) $(LIBS)

main.o: main.c fuseSteganografia.h
	$(CC) $(CFLAGS) -c main.c

fuseSteganografia.o: fuseSteganografia.c fuseSteganografia.h manipuladorBmp.h
	$(CC) $(CFLAGS) -c fuseSteganografia.c

manipuladorBmp.o: manipuladorBmp.c manipuladorBmp.h
	$(CC) $(CFLAGS) -c manipuladorBmp.c

clean:
	rm -f *.o fuseSteganografia
