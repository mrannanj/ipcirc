SRC_DIR := src
OBJ_DIR := build

DAEMON_DIR := $(SRC_DIR)/daemon
DAEMON_SRC := $(shell find $(DAEMON_DIR) -name '*.c')
DAEMON_OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(DAEMON_SRC))

ATTACH_DIR := $(SRC_DIR)/attach
ATTACH_SRC := $(shell find $(ATTACH_DIR) -name '*.c')
ATTACH_OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(ATTACH_SRC))

CLIENT_DIR := $(SRC_DIR)/client
CLIENT_SRC := $(shell find $(CLIENT_DIR) -name '*.c')
CLIENT_OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(CLIENT_SRC))

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

iircd: $(DAEMON_OBJ) $(COMMON_OBJ)
	$(CC) $^ -o $@

iirc-attach: $(ATTACH_OBJ) $(COMMON_OBJ)
	$(CC) $^ -o $@

iirc: $(CLIENT_OBJ) $(COMMON_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

install: $(TARGETS)
	mkdir -p $(PREFIX)/bin
	install -m 0755 $(TARGETS) $(PREFIX)/bin

clean:
	rm -rf $(OBJ_DIR)
	rm -rf $(TARGETS)
