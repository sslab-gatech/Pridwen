# Set according to the environment
uname := $(shell uname -s)
ifeq ($(uname), Linux)
EMCC := ../../src/emsdk/emscripten/1.38.12/emcc
else
EMCC := ../../build/emscripten/emcc
endif

src = $(wildcard src/*.c)
objs = $(src:.c=.o)
bc_objs = $(src:.c=.bc)

EMCC_FLAG := --emit-symbol-map
EMCC_LINKFLAG := --emit-symbol-map

# generic options for gcc
BASE_CFLAGS = -fPIC -fno-asynchronous-unwind-tables \
              -fno-jump-tables \
              -Wall -pedantic -Wno-unused-function -std=gnu11

CFLAGS = $(BASE_CFLAGS)  \
         -fno-stack-protector -fvisibility=hidden

all: jpeg.js

jpeg.js: $(bc_objs)
	@echo "LINK $$@"
	$(EMCC) $(EMCC_LINKFLAG) $^ -o $@
%.bc: %.c
	@echo "Generate bytecode: $<"
	$(EMCC) $(EMCC_FLAG) $< -o $@

jpeg: $(objs)
	gcc -o $@ $^

$(objs): %.o: %.c
	gcc -c $< -o $@

print-%  : ; @echo $* = $($*)

.PHONY: clean

clean:
	rm -f jpeg *.js *.wasm $(bc_objs) $(objs)

