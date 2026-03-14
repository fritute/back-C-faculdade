CC = gcc

# No Linux (Render) não há MSYS2 — libpq vem do sistema (apt install libpq-dev)
# No Windows local, ajuste PG_PATH para seu MSYS2
ifeq ($(OS),Windows_NT)
    PG_PATH    = C:/msys64/ucrt64
    CFLAGS     = -Iinclude -Ilibs -I"$(PG_PATH)/include" -Wall
    LIBS       = -lws2_32 -L"$(PG_PATH)/lib" -lpq
    TARGET     = $(BIN_DIR)/distribpro.exe
else
    CFLAGS     = -Iinclude -Ilibs -I$(shell pg_config --includedir) -Wall
    LIBS       = -lpq
    TARGET     = $(BIN_DIR)/distribpro
endif

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

all: $(TARGET)

$(TARGET): $(SOURCES)
	mkdir -p $(BIN_DIR)
	$(CC) $(SOURCES) $(CFLAGS) $(LIBS) -o $(TARGET)

clean:
	rm -rf $(BIN_DIR)

run: all
	./$(TARGET)
