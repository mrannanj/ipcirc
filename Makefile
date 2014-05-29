SRC_DIR := src
OBJ_DIR := build

BACKEND_DIR := $(SRC_DIR)/backend
BACKEND_SRC := $(shell find $(BACKEND_DIR) -name '*.c')
BACKEND_OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(BACKEND_SRC))

PROXY_DIR := $(SRC_DIR)/proxy
PROXY_SRC := $(shell find $(PROXY_DIR) -name '*.c')
PROXY_OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(PROXY_SRC))

FRONT_DIR := $(SRC_DIR)/front
FRONT_SRC := $(shell find $(FRONT_DIR) -name '*.c')
FRONT_OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(FRONT_SRC))

COMMON_DIR := $(SRC_DIR)/common
COMMON_SRC := $(shell find $(COMMON_DIR) -name '*.c')
COMMON_OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(COMMON_SRC))

TARGETS := ipc-back ipc-proxy ipc-front

W := -Wno-unused-parameter -Wall -Wextra
CFLAGS := -Isrc -D_POSIX_SOURCE -g -pedantic -std=c99 $(W) -Werror

.PHONY: all clean

all: $(TARGETS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

ipc-back: $(BACKEND_OBJ) $(COMMON_OBJ)
	$(CC) $^ -o $@

ipc-proxy: $(PROXY_OBJ) $(COMMON_OBJ)
	$(CC) $^ -o $@

ipc-front: $(FRONT_OBJ) $(COMMON_OBJ)
	$(CC) $^ -o $@

clean:
	rm -rf $(OBJ_DIR)
	rm -rf $(TARGETS)
