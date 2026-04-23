CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude
SRC = src/core/parser.c src/cli/main.c
OBJ = $(SRC:.c=.o)
TARGET = nyxj

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

clean:
	rm -f src/core/*.o src/cli/*.o $(TARGET)