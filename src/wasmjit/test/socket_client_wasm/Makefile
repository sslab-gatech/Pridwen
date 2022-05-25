# Set according to the environment
#EMCC := ../../../emsdk/emscripten/1.38.12/emcc
EMCC := ../../../../build/emscripten/emcc

EMCC_FLAG := --emit-symbol-map

all: client.js

client.js: client.c
	@echo "LINK: $@"
	$(EMCC) $(EMCC_FLAG) $< -o $@

.PHONY: all clean
clean:
	rm -f *.wasm *.js *.js.symbols
