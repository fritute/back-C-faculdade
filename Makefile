CC = gcc
PG_PATH = C:/msys64/ucrt64
CFLAGS = -Iinclude -Ilibs -I"$(PG_PATH)/include" -Wall
LIBS = -lws2_32 -L"$(PG_PATH)/lib" -lpq

SRC_DIR = src
LIB_DIR = libs
BIN_DIR = bin

SOURCES = $(SRC_DIR)/main.c \
          $(SRC_DIR)/db.c \
          $(SRC_DIR)/http/server.c \
          $(SRC_DIR)/http/handlers/produtos.c \
          $(SRC_DIR)/http/handlers/clientes.c \
          $(SRC_DIR)/http/handlers/fornecedores.c \
          $(SRC_DIR)/http/handlers/pedidos.c \
          $(SRC_DIR)/http/handlers/dashboard.c \
          $(SRC_DIR)/http/handlers/estoque.c \
          $(SRC_DIR)/http/handlers/auth.c \
          $(SRC_DIR)/http/handlers/config.c \
          $(SRC_DIR)/db/repo.c \
          $(SRC_DIR)/utils/common.c \
          $(LIB_DIR)/mongoose.c \
          $(LIB_DIR)/cJSON.c

TARGET = $(BIN_DIR)/distribpro.exe

all: $(TARGET)

$(TARGET): $(SOURCES)
	@if not exist bin mkdir bin
	$(CC) $(SOURCES) $(CFLAGS) $(LIBS) -o $(TARGET)

clean:
	@if exist bin rmdir /s /q bin

run: all
	./$(TARGET)
