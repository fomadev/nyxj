# NyxJ Engine - cross-platform build (Linux, macOS, Windows/MinGW, WebAssembly)
#
#   make                 library, CLI, server, plugin example and examples
#   make test            build and run the test suite
#   make SAN=1 test      same with AddressSanitizer + UBSan
#   make wasm            WebAssembly build of the core (requires emcc)
#   make dist PLATFORM=linux-x64   stage a release package in dist/
#   make CC=aarch64-linux-gnu-gcc AR=aarch64-linux-gnu-ar   cross-compile

ifeq ($(origin CC),default)
CC = gcc
endif
AR ?= ar
EMCC ?= emcc
PREFIX ?= /usr/local

VERSION := $(shell sed -n 's/^\#define NYXJ_VERSION "\(.*\)"/\1/p' include/nyxj.h)

ifeq ($(OS),Windows_NT)
  EXE = .exe
  SO = .dll
  LIBS = -lws2_32 -lpsapi
  LDFLAGS += -static
  PIC =
  PLATFORM ?= windows-x64
else
  EXE =
  SO = .so
  PIC = -fPIC
  UNAME_S := $(shell uname -s)
  ifeq ($(UNAME_S),Darwin)
    LIBS = -lpthread -lm
    PLATFORM ?= macos-$(shell uname -m)
  else
    LIBS = -lpthread -lm -ldl -lrt
    PLATFORM ?= linux-$(shell uname -m)
  endif
endif

DEFS = -DMG_TLS=MG_TLS_BUILTIN -DMG_ENABLE_CHACHA20=0 -DMG_MAX_RECV_SIZE=67108864UL -DNYXJ_WITH_NET
OPT ?= -O2
WARN = -Wall -Wextra
CFLAGS_BASE = -std=gnu11 $(OPT) $(PIC) -Iinclude -Ivendor/mongoose $(DEFS)

# Sanitizers: UBSan covers NyxJ sources; the vendored Mongoose code only gets ASan.
SAN_FLAGS =
VENDOR_SAN_FLAGS =
ifeq ($(SAN),1)
  OPT = -O1 -g -fno-omit-frame-pointer
  SAN_FLAGS = -fsanitize=address,undefined -fno-sanitize-recover=undefined
  VENDOR_SAN_FLAGS = -fsanitize=address
  LDFLAGS += -fsanitize=address,undefined
endif

CFLAGS ?=
ALL_CFLAGS = $(CFLAGS_BASE) $(SAN_FLAGS) $(WARN) $(CFLAGS)

BUILD = build
OBJ = $(BUILD)/obj

CORE_SRC = $(wildcard src/common/*.c) $(wildcard src/core/*.c) $(wildcard src/modules/*.c)
RUNTIME_SRC = $(wildcard src/runtime/*.c)
NET_SRC = $(wildcard src/net/*.c)
VENDOR_SRC = vendor/mongoose/mongoose.c
LIB_SRC = $(CORE_SRC) $(RUNTIME_SRC) $(NET_SRC)
LIB_OBJ = $(patsubst %.c,$(OBJ)/%.o,$(LIB_SRC)) $(OBJ)/vendor/mongoose/mongoose.o
TEST_SRC = $(wildcard tests/*.c)
TEST_OBJ = $(patsubst %.c,$(OBJ)/%.o,$(TEST_SRC))
HEADERS = $(wildcard include/*.h) $(wildcard src/*/*.h)

LIB = $(BUILD)/libnyxj.a
CLI = $(BUILD)/nyxj$(EXE)
SERVER = $(BUILD)/nyxj-server$(EXE)
TESTS = $(BUILD)/nyxj_tests$(EXE)
PLUGIN = $(BUILD)/plugins/hello_plugin$(SO)
EXAMPLES = $(BUILD)/examples/api_example$(EXE) $(BUILD)/examples/client_test$(EXE) \
           $(BUILD)/examples/struct_mapping$(EXE) $(BUILD)/examples/streaming$(EXE)

.PHONY: all lib cli server test examples plugin wasm dist install clean re version

all: lib cli server plugin examples

lib: $(LIB)
cli: $(CLI)
server: $(SERVER)
plugin: $(PLUGIN)
examples: $(EXAMPLES)

version:
	@echo $(VERSION)

$(OBJ)/vendor/mongoose/mongoose.o: $(VENDOR_SRC) vendor/mongoose/mongoose.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_BASE) $(VENDOR_SAN_FLAGS) -w $(CFLAGS) -c $< -o $@

$(OBJ)/%.o: %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(LIB): $(LIB_OBJ)
	@mkdir -p $(dir $@)
	@rm -f $@
	$(AR) rcs $@ $^

$(CLI): $(OBJ)/src/cli/main.o $(LIB)
	$(CC) $^ -o $@ $(LDFLAGS) $(LIBS)

$(SERVER): $(OBJ)/src/server/main.o $(LIB)
	$(CC) $^ -o $@ $(LDFLAGS) $(LIBS)

$(TESTS): $(TEST_OBJ) $(LIB)
	$(CC) $^ -o $@ $(LDFLAGS) $(LIBS)

$(BUILD)/examples/%$(EXE): $(OBJ)/examples/%/main.o $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS) $(LIBS)

$(BUILD)/examples/api_example$(EXE): $(OBJ)/examples/api_example/api_example.o $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS) $(LIBS)

$(BUILD)/examples/client_test$(EXE): $(OBJ)/examples/client_test/client_test.o $(LIB)
	@mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS) $(LIBS)

# Plugins only talk to the host through the nyxj_host_api table: no link step against NyxJ.
$(PLUGIN): examples/plugin_hello/hello_plugin.c include/nyxj_plugin.h
	@mkdir -p $(dir $@)
	$(CC) -std=gnu11 $(OPT) -fPIC -shared -Iinclude $(WARN) $(CFLAGS) $< -o $@

test: $(TESTS) $(PLUGIN)
	./$(TESTS)

WASM_EXPORTS = _malloc,_free,_nyxj_wasm_version,_nyxj_wasm_free,_nyxj_wasm_format,_nyxj_wasm_minify,_nyxj_wasm_validate,_nyxj_wasm_query,_nyxj_wasm_repair,_nyxj_wasm_schema,_nyxj_wasm_diff,_nyxj_wasm_msgpack_size

wasm:
	@mkdir -p $(BUILD)/wasm
	$(EMCC) -std=gnu11 -O2 -DNYXJ_NO_THREADS -Iinclude $(CORE_SRC) src/wasm/nyxj_wasm.c \
		-s MODULARIZE=1 -s EXPORT_NAME=NyxJ -s ALLOW_MEMORY_GROWTH=1 \
		-s EXPORTED_FUNCTIONS=$(WASM_EXPORTS) \
		-s EXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString,stringToNewUTF8 \
		-o $(BUILD)/wasm/nyxj.js
	@echo "WebAssembly build: $(BUILD)/wasm/nyxj.js + nyxj.wasm"

DIST_DIR = dist/nyxj-$(VERSION)-$(PLATFORM)

dist: all
	@rm -rf $(DIST_DIR)
	@mkdir -p $(DIST_DIR)/bin $(DIST_DIR)/lib $(DIST_DIR)/include $(DIST_DIR)/plugins $(DIST_DIR)/examples
	cp $(CLI) $(SERVER) $(DIST_DIR)/bin/
	cp $(EXAMPLES) $(DIST_DIR)/examples/
	cp $(LIB) $(DIST_DIR)/lib/
	cp include/*.h $(DIST_DIR)/include/
	cp $(PLUGIN) $(DIST_DIR)/plugins/
	cp README.md DOCUMENTATION.md LICENSE CHANGELOG.md $(DIST_DIR)/
	@echo "Staged $(DIST_DIR)"

install: lib cli server
	mkdir -p $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(PREFIX)/include/nyxj
	cp $(CLI) $(SERVER) $(DESTDIR)$(PREFIX)/bin/
	cp $(LIB) $(DESTDIR)$(PREFIX)/lib/
	cp include/*.h $(DESTDIR)$(PREFIX)/include/nyxj/

clean:
	rm -rf $(BUILD) dist

re: clean all
