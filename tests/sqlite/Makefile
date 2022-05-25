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


EMCC_FLAG := --emit-symbol-map -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_MUTEX_NOOP -DSQLITE_OMIT_WAL
EMCC_LINKFLAG := --emit-symbol-map

# generic options for gcc
BASE_CFLAGS = -fPIC -fno-asynchronous-unwind-tables \
              -fno-jump-tables \
              -Wall -pedantic -Wno-unused-function -std=gnu11


CFLAGS = $(BASE_CFLAGS) \
         -fno-stack-protector -fvisibility=hidden \
	 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_MUTEX_NOOP -DSQLITE_OMIT_WAL

all: sqlite.js

sqlite.js: $(bc_objs)
	@echo "LINK $$@"
	$(EMCC) $(EMCC_LINKFLAG) $^ -o $@

%.bc: %.c
	@echo "Generate bytecode: $<"
	$(EMCC) $(EMCC_FLAG) $< -o $@

sqlite: $(objs)
	gcc -o $@ $^

$(objs): %.o: %.c
	gcc $(CFLAGS) -c $< -o $@
	echo "CC <= $<"

print-%  : ; @echo $* = $($*)

.PHONY: clean

clean:
	rm -f sqlite *.js *.wasm $(bc_objs) $(objs)

