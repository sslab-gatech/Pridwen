#include <wasmjit/ast.h>
#include <wasmjit/compile.h>
#include <wasmjit/ast_dump.h>
#include <wasmjit/parse.h>
#include <wasmjit/runtime.h>
#include <wasmjit/instantiate.h>
#include <wasmjit/emscripten_runtime.h>
#include <wasmjit/dynamic_emscripten_runtime.h>
#include <wasmjit/elf_relocatable.h>
#include <wasmjit/util.h>
#include <wasmjit/high_level.h>

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <regex.h>
#include <unistd.h>

#include <sys/types.h>

int load_and_invoke(const char* filename, char* output, int argc, char** argv, char** environ);
