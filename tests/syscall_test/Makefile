# Set according to the environment
uname := $(shell uname -s)
ifeq ($(uname), Linux)
EMCC := ../../src/emsdk/emscripten/1.38.12/emcc
else
EMCC := ../../build/emscripten/emcc
endif

EMCC_FLAG := --emit-symbol-map

all: test.js

test.js: test.c
	@echo "LINK: $@"
	$(EMCC) $(EMCC_FLAG) $< -o $@

.PHONY: all clean
clean:
	rm -f *.wasm *.js *.js.symbols
