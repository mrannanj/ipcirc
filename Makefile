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

PROTO_SRC := $(SRC_DIR)/common/iirc.pb-c.c
PROTO_OBJ := $(OBJ_DIR)/common/iirc.pb-c.o

COMMON_DIR := $(SRC_DIR)/common
COMMON_SRC := $(PROTO_SRC)
COMMON_SRC += $(shell find $(COMMON_DIR) -name '*.c')
COMMON_OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(COMMON_SRC))

DEPS := $(COMMON_OBJ:.o=.d) $(DAEMON_OBJ:.o=.d) $(ATTACH_OBJ:.o=.d)
DEPS += $(CLIENT_OBJ:.o=.d)

TARGETS := iirc iircd iirc-attach

W := -Wno-unused-parameter -Wall -Wextra

CFLAGS := $(shell pkg-config --cflags ncurses)
CFLAGS += -Isrc -D_POSIX_SOURCE -D_GNU_SOURCE
CFLAGS += -g -pedantic -std=c99 $(W) -Werror

LDFLAGS := -Wl,-Bstatic -lprotobuf-c
LDFLAGS += -Wl,-Bdynamic $(shell pkg-config --libs ncurses)

.PHONY: all clean install indent

all: $(TARGETS)

.SECONDARY:
$(SRC_DIR)/%.pb-c.h $(SRC_DIR)/%.pb-c.c: $(SRC_DIR)/%.proto
	@echo "PROCOC $@ <- $<"
	@cd src; \
	 protoc-c --c_out=./ common/iirc.proto

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(PROTO_SRC)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(W) -MMD -MP -c $< -o $@

iircd: $(DAEMON_OBJ) $(COMMON_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

iirc-attach: $(ATTACH_OBJ) $(COMMON_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

iirc: $(CLIENT_OBJ) $(COMMON_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS)

indent: $(COMMON_SRC) $(CLIENT_SRC) $(ATTACH_SRC) $(DAEMON_SRC)
	indent -linux $^

install: $(TARGETS)
	mkdir -p $(PREFIX)/bin
	install -m 0755 $(TARGETS) $(PREFIX)/bin

clean:
	rm -rf $(OBJ_DIR)
	rm -rf $(TARGETS)
	rm -rf src/common/iirc.pb-c.c src/common/iirc.pb-c.h

ifneq ($(MAKECMDGOALS), clean)
-include $(DEPS)
endif
