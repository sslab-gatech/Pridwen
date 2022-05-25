# Set according to the environment
uname := $(shell uname -s)
ifeq ($(uname), Linux)
EMCC := ../../../emsdk/emscripten/1.38.12/emcc
else
EMCC := ../../../../build/emscripten/emcc
endif

EMCC_FLAG := --emit-symbol-map
EMCC_LINKFLAG := --emit-symbol-map

all: nbench.js

nbench.js: nbench0.bc nbench1.bc misc.bc sysspec.bc emfloat.bc
	@echo "LINK: $@"
	$(EMCC) $(EMCC_LINKFLAG) $^ -o $@

%.bc: %.c
	@echo "Generate bytecode: $<"
	$(EMCC) $(EMCC_FLAG) $< -o $@

.PHONY: all clean
clean:
	rm -f *.bc *.wasm *.js *.js.symbols
