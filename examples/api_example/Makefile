# --- Détection de l'OS ---
ifeq ($(OS),Windows_NT)
    # Configuration Windows
    LDFLAGS = -lws2_32
    RM = del /f /q
    EXT = .exe
else
    # Configuration Linux / macOS
    LDFLAGS = -lpthread
    RM = rm -f
    EXT =
endif

# --- Configuration du compilateur ---
CC = gcc
CFLAGS = -Wall -I../../include -I../../vendor/mongoose

# --- Fichiers (S'adapte au nom du dossier) ---
# On récupère le nom du fichier .c présent dans le dossier
SRC = $(wildcard *.c)
TARGET = $(SRC:.c=$(EXT))

# Références vers le moteur NyxJ et Mongoose
CORE_SRC = ../../src/core/parser.c ../../vendor/mongoose/mongoose.c

# --- Règles ---

all: $(TARGET)

$(TARGET): $(SRC) $(CORE_SRC)
	$(CC) $(CFLAGS) $(SRC) $(CORE_SRC) -o $(TARGET) $(LDFLAGS)

clean:
	$(RM) *$(EXT) *.o

re: clean all