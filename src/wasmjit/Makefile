# *** THIS IS NOT A LONG TERM SOLUTION ***

LCFLAGS ?= -Isrc -g -Wall -Wextra -Werror
UNAME := $(shell uname -s)
OSFLAGS :=
ifeq ($(UNAME), Linux)
	OSFLAGS += -pthread -lm
endif

all: sgxwasm

clean:
	rm -f src/sgxwasm/vector.o src/sgxwasm/ast.o src/sgxwasm/ast_dump.o src/sgxwasm/main.o src/sgxwasm/parse.o src/sgxwasm/compile.o sgxwasm src/sgxwasm/runtime.o src/sgxwasm/util.o src/sgxwasm/dynamic_emscripten_runtime.o src/sgxwasm/emscripten_runtime_sys_posix.o src/sgxwasm/instantiate.o src/sgxwasm/high_level.o src/sgxwasm/dynamic_runtime.o src/sgxwasm/register.o src/sgxwasm/assembler.o src/sgxwasm/test.o src/sgxwasm/emscripten.o src/sgxwasm/pass.o src/sgxwasm/pass_tsgx.o src/sgxwasm/pass_aslr.o src/sgxwasm/pass_test.o src/sgxwasm/relocate.o src/sgxwasm/pass_cfg.o src/sgxwasm/pass_caslr.o

sgxwasm: src/sgxwasm/main.o src/sgxwasm/vector.o src/sgxwasm/ast.o src/sgxwasm/parse.o src/sgxwasm/ast_dump.o src/sgxwasm/compile.o src/sgxwasm/runtime.o src/sgxwasm/util.o src/sgxwasm/dynamic_emscripten_runtime.o src/sgxwasm/emscripten_runtime_sys_posix.o src/sgxwasm/instantiate.o src/sgxwasm/high_level.o src/sgxwasm/dynamic_runtime.o src/sgxwasm/register.o src/sgxwasm/assembler.o src/sgxwasm/test.o src/sgxwasm/emscripten.o src/sgxwasm/pass.o src/sgxwasm/pass_tsgx.o src/sgxwasm/pass_aslr.o src/sgxwasm/pass_test.o src/sgxwasm/relocate.o src/sgxwasm/pass_cfg.o src/sgxwasm/pass_caslr.o
	$(CC) -o $@ $^ $(LCFLAGS) $(OSFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(LCFLAGS)

