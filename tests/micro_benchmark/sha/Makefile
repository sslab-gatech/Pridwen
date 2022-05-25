# Set according to the environment
UNAME := $(shell uname -s)
ifeq ($(UNAME), Linux)
EMCC := ../../../src/emsdk/emscripten/1.38.12/emcc
else
EMCC := ../../../build/emscripten/emcc
endif

WASM2WAT := ../../../src/wabt/bin/wasm2wat

EMCC_FLAG := --emit-symbol-map

all: test.js

test.js: sha1.bc test.c
	@echo "LINK: $@"
	$(EMCC) $(EMCC_FLAG) $^ -o $@
	$(WASM2WAT) test.wasm > test.wat

%.bc: %.c
	@echo "Generate bytecode: $<"
	$(EMCC) $(EMCC_FLAG) $< -o $@

.PHONY: all clean
clean:
	rm -f *.wasm *.js *.js.symbols
