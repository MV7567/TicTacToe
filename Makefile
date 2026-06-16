# Kompilator i flagi
CC = gcc
CFLAGS = -Wall -Wextra -pthread -I./include
LDFLAGS = -pthread

# Katalogi
SRC_DIR = src
OBJ_DIR = src
BIN_DIR = .
DATA_DIR = data

# Pliki obiektowe
COMMON_OBJS = $(OBJ_DIR)/network.o
SERVER_OBJS = $(OBJ_DIR)/server.o $(OBJ_DIR)/auth.o $(COMMON_OBJS)
CLIENT_OBJS = $(OBJ_DIR)/client.o $(COMMON_OBJS)

# Nazwy plików binarnych
SERVER_BIN = $(BIN_DIR)/server
CLIENT_BIN = $(BIN_DIR)/client

# Cel domyślny: buduje serwer i klienta
all: prepare $(SERVER_BIN) $(CLIENT_BIN)

# Tworzenie folderu na bazę danych, jeśli nie istnieje
prepare:
	@mkdir -p $(DATA_DIR)

# Kompilacja serwera
$(SERVER_BIN): $(SERVER_OBJS)
	$(CC) $(SERVER_OBJS) -o $@ $(LDFLAGS)

# Kompilacja klienta
$(CLIENT_BIN): $(CLIENT_OBJS)
	$(CC) $(CLIENT_OBJS) -o $@ $(LDFLAGS)

# Reguła kompilacji plików .c do .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Czyszczenie plików tymczasowych i binarnych
clean:
	rm -f $(SRC_DIR)/*.o $(SERVER_BIN) $(CLIENT_BIN)
	@echo "Oczyszczono pliki binarne i obiekty."

.PHONY: all clean prepare