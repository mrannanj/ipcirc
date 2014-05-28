SRC_DIR := src
OBJ_DIR := build

BACKEND_DIR := $(SRC_DIR)/backend
BACKEND_SRC := $(shell find $(BACKEND_DIR) -name '*.c')
BACKEND_OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(BACKEND_SRC))

FRONT_DIR := $(SRC_DIR)/front
FRONT_SRC := $(shell find $(FRONT_DIR) -name '*.c')
FRONT_OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(FRONT_SRC))

TARGETS := front backend

CFLAGS := -std=c99

.PHONY: all clean

all: $(TARGETS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

front: $(FRONT_OBJ)
	$(CC) $^ -o $@

backend: $(BACKEND_OBJ)
	$(CC) $^ -o $@

clean:
	rm -rf $(OBJ_DIR)
	rm -rf $(TARGETS)
