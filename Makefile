CC = gcc
CFLAGS = -Wall -g -Iinclude `pkg-config fuse3 --cflags` -D_FILE_OFFSET_BITS=64
LDFLAGS = `pkg-config fuse3 --libs`
TARGET = fusefs

SRC = src/main.c src/fuse_operations.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f $(OBJ) $(TARGET)
