# Set according to the environment
uname := $(shell uname -s)
ifeq ($(uname), Linux)
EMCC := ../../src/emsdk/emscripten/1.38.12/emcc
else
EMCC := ../../build/emscripten/emcc
endif

WASM2WAT = ../../tests/wabt/bin/wasm2wat

EMCC_FLAG := --emit-symbol-map -Os
#EMCC_FLAG := --emit-symbol-map -O1
#EMCC_FLAG := --emit-symbol-map

all: test.js

test: test.c
	$(CC) -o $@ $<

test.js: test.c
	@echo "LINK: $@"
	$(EMCC) $(EMCC_FLAG) $< -o $@
	$(WASM2WAT) test.wasm > test.wat

.PHONY: all clean
clean:
	rm -f *.wasm *.js *.js.symbols *.wat
