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

TARGETS := iirc iircd iirc-attach

W := -Wno-unused-parameter -Wall -Wextra

CFLAGS := $(shell pkg-config --cflags ncurses)
CFLAGS += -Isrc -D_POSIX_SOURCE -D_GNU_SOURCE
CFLAGS += -g -pedantic -std=c99 $(W) -Werror

LDFLAGS := $(shell pkg-config --libs ncurses)

.PHONY: all clean install

all: $(TARGETS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

iircd: $(BACKEND_OBJ) $(COMMON_OBJ)
	$(CC) $^ -o $@

iirc-attach: $(PROXY_OBJ) $(COMMON_OBJ)
	$(CC) $^ -o $@

iirc: $(FRONT_OBJ) $(COMMON_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

install: $(TARGETS)
	mkdir -p $(PREFIX)/bin
	install -m 0755 $(TARGETS) $(PREFIX)/bin

clean:
	rm -rf $(OBJ_DIR)
	rm -rf $(TARGETS)
