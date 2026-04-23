CC = gcc
CFLAGS = -Wall -Iinclude -Ivendor/mongoose
LDFLAGS = -lws2_32

# On utilise des patterns pour trouver les sources
SRCS = src/server.c src/core/parser.c vendor/mongoose/mongoose.c
# On génère les noms d'objets
OBJS = $(SRCS:.c=.o)
TARGET = nyxj_server.exe

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Règle générique pour les .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean compatible avec le shell de w64devkit (rm au lieu de del)
clean:
	rm -f src/*.o src/core/*.o vendor/mongoose/*.o *.exe

re: clean all