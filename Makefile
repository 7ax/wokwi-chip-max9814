WASI_SDK_PATH ?= /opt/wasi-sdk

CC = $(WASI_SDK_PATH)/bin/clang
CFLAGS = --target=wasm32-wasi --sysroot=$(WASI_SDK_PATH)/share/wasi-sysroot \
         -Wall -Wextra -Werror -O2 -I./include
LDFLAGS = -nostartfiles -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined

.PHONY: all clean

all: dist/chip.wasm

dist/chip.wasm: src/main.c
	mkdir -p dist
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -lm

clean:
	rm -rf dist
