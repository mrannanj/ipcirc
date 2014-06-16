SRC_DIR := src
OBJ_DIR := build

DAEMON_DIR := $(SRC_DIR)/daemon
DAEMON_SRC := $(wildcard $(DAEMON_DIR)/*.c)
DAEMON_OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(DAEMON_SRC))

ATTACH_DIR := $(SRC_DIR)/attach
ATTACH_SRC := $(wildcard $(ATTACH_DIR)/*.c)
ATTACH_OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(ATTACH_SRC))

CLIENT_DIR := $(SRC_DIR)/client
CLIENT_SRC := $(wildcard $(CLIENT_DIR)/*.c)
CLIENT_OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(CLIENT_SRC))

OBJ = $(DAEMON_OBJ) $(ATTACH_OBJ) $(CLIENT_OBJ)

COMMON_DIR := $(SRC_DIR)/common
COMMON_SRC := $(wildcard $(COMMON_DIR)/*.c)
COMMON_OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(COMMON_SRC))

PROTO_DIR := $(SRC_DIR)/proto
PROTO_DEF := $(wildcard $(PROTO_DIR)/*.proto)
PROTO_SRC := $(patsubst %.proto, %.pb-c.c, $(PROTO_DEF))
PROTO_OBJ := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(PROTO_SRC))

LIBIIRC := libiirc.so

DEPS := $(PROTO_OBJ:.o=.d) $(COMMON_OBJ:.o=.d)
DEPS += $(DAEMON_OBJ:.o=.d) $(ATTACH_OBJ:.o=.d) $(CLIENT_OBJ:.o=.d)

TARGETS := iirc iircd iirc-attach

W := -Wno-unused-parameter -Wno-variadic-macros -Wall -Wextra

CFLAGS := $(shell pkg-config --cflags ncurses)
CFLAGS += -Isrc -D_POSIX_SOURCE -D_GNU_SOURCE
CFLAGS += -g -pedantic -std=c89 $(W) -Werror

LDFLAGS := -lprotobuf-c $(shell pkg-config --libs ncurses)
LDFLAGS += -Wl,-rpath=\$$ORIGIN:\$$ORIGIN/../lib -L. -liirc

.PHONY: all clean libiirc install indent

CUR_SRC = $(patsubst $(OBJ_DIR)/%, $(SRC_DIR)/%.c, $*)

all: $(TARGETS)

.SECONDARY:
$(SRC_DIR)/%.pb-c.h $(SRC_DIR)/%.pb-c.c: $(SRC_DIR)/%.proto
	@echo PROTO-C $@
	@cd $(SRC_DIR); \
	 protoc-c --c_out=./ $(patsubst $(SRC_DIR)/%, %, $<)

$(OBJ): $(DAEMON_SRC) $(ATTACH_SRC) $(COMMON_SRC) $(PROTO_SRC)
	@mkdir -p $(@D)
	@echo CC $@
	@$(CC) $(CFLAGS) -MMD -MP -c $(CUR_SRC) -o $@

$(LIBIIRC): libiirc($(COMMON_OBJ) $(PROTO_OBJ))
	@echo LD $@
	@$(CC) -shared -Wl,-soname,$(LIBIIRC) -o $(LIBIIRC) $^

libiirc($(COMMON_OBJ) $(PROTO_OBJ)): $(COMMON_SRC) $(PROTO_SRC)
	@mkdir -p $(dir $*)
	@echo CC $%
	@$(CC) $(CFLAGS) -fPIC -MMD -MP -c $(CUR_SRC) -o $%

libiirc: $(LIBIIRC)

iircd: $(DAEMON_OBJ) $(LIBIIRC)
	@echo LD $@
	@$(CC) $^ -o $@ $(LDFLAGS)

iirc-attach: $(ATTACH_OBJ) $(LIBIIRC)
	@echo LD $@
	@$(CC) $^ -o $@ $(LDFLAGS)

iirc: $(CLIENT_OBJ) $(LIBIIRC)
	@echo LD $@
	@$(CC) $^ -o $@ $(LDFLAGS)

indent:
	indent -linux $(wildcard $(SRC_DIR)/*/*.c)
	indent -linux $(wildcard $(SRC_DIR)/*/*.h)

install: $(TARGETS)
	mkdir -p $(PREFIX)/lib
	mkdir -p $(PREFIX)/bin
	install -m 0755 $(LIBIIRC) $(PREFIX)/lib
	install -m 0755 $(TARGETS) $(PREFIX)/bin

clean:
	rm -rf $(OBJ_DIR)
	rm -rf $(TARGETS)
	rm -rf $(LIBIIRC)
	rm -rf $(PROTO_SRC)
	rm -rf $(patsubst %.c, %.h, $(PROTO_SRC))

ifneq ($(MAKECMDGOALS), clean)
-include $(DEPS)
endif
