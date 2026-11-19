CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -Isrc -Isrc/scanner -Isrc/ast -Isrc/generator -Isrc/symtable -Isrc/parser -Isrc/stack
SRC = $(wildcard src/*.c) $(wildcard src/*/*.c)
OBJ = $(SRC:.c=.o)
TARGET = compiler

build: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: build clean