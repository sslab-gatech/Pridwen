/*
 * Copyright (C) 2011-2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */

#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "ocall_stub.h"

#include "sgx_trts.h"

#include "Enclave.h"
#include "Enclave_t.h"  /* print_string */

#include "wasmjit/sys.h"
#include "wasmjit/util.h"
#include "wasmjit/ast.h"
#include "wasmjit/parse.h"
#include "wasmjit/runtime.h"
#include "wasmjit/high_level.h"
#include "wasmjit/instantiate.h"
#include "wasmjit/emscripten_runtime.h"

static void *get_stack_top(void)
{
  return NULL;
}

static int parse_module(const char *filename, struct Module *module)
{
  char *buf = NULL;
  int ret, result;
  size_t size;
  struct ParseState pstate;

  buf = wasmjit_load_file(filename, &size);

  if (!buf)
    goto error;

  ret = init_pstate(&pstate, buf, size);
  if (!ret)
    goto error;

  ret = read_module(&pstate, module, NULL, 0);
  if (!ret)
    goto error;

  result = 0;

  if (0) {
    error:
    result = -1;
  }

  if (buf)
    wasmjit_unload_file(buf, size);

  return result;
}

static int dump_wasm_module(const char *filename)
{
  uint32_t i;
  struct Module module;
  int res = -1;

  wasmjit_init_module(&module);

  if (parse_module(filename, &module))
    goto error;

  /* the most basic validation */
  if (module.code_section.n_codes != module.function_section.n_typeidxs) {
    printf("# Functions != # Codes %" PRIu32 " != %" PRIu32 "\n",
           module.function_section.n_typeidxs,
           module.code_section.n_codes);
    goto error;
  }

  for (i = 0; i < module.import_section.n_imports; i++) {
    struct ImportSectionImport *import = &module.import_section.imports[i];
    printf("import \"%s\"\n", import->name);
  }

  for (i = 0; i < module.export_section.n_exports; i++) {
    struct ExportSectionExport *export = &module.export_section.exports[i];
    printf("export \"%s\"\n", export->name);
  }

  for (i = 0; i < module.code_section.n_codes; ++i) {
    uint32_t j;
    struct TypeSectionType *type;
    struct CodeSectionCode *code = &module.code_section.codes[i];

    type = &module.type_section.types[module.function_section.typeidxs[i]];

    printf("Code #%" PRIu32 "\n", i);

    printf("Locals (%" PRIu32 "):\n", code->n_locals);
    for (j = 0; j < code->n_locals; ++j) {
      printf("  %s (%" PRIu32 ")\n",
             wasmjit_valtype_repr(code->locals[j].
             valtype),
             code->locals[j].count);
    }

    printf("Signature: [");
    for (j = 0; j < type->n_inputs; ++j) {
      printf("%s,",
             wasmjit_valtype_repr(type->
             input_types[j]));
    }

    printf("] -> [");
    for (j = 0; j < FUNC_TYPE_N_OUTPUTS(type); ++j) {
      printf("%s,",
             wasmjit_valtype_repr(FUNC_TYPE_OUTPUT_IDX(type, j)));
    }
    printf("]\n");

    printf("Instructions:\n");
    dump_instructions(module.code_section.codes[i].
                      instructions,
                      module.code_section.codes[i].
                      n_instructions, 1);
    printf(" \n");
  }

  res = 0;

 error:
   wasmjit_free_module(&module);

   return res;
}

static int run_emscripten_file(const char *filename,
                               uint32_t static_bump,
                               int has_table,
                               size_t tablemin, size_t tablemax,
                               int argc, char **argv, char **envp)
{
//#if 0
  struct WasmJITHigh high;
  int ret;
  void *stack_top;
  int high_init = 0;
  const char *msg;
  uint32_t flags = 0;

  // TODO: Hardcode stack top
  stack_top = get_stack_top();
  if (!stack_top) {
    printf("warning: running without a stack limit\n");
  }

  wasmjit_set_stack_top(stack_top);

  if (wasmjit_high_init(&high)) {
    msg = "failed to initialize";
    goto error;
  }

  high_init = 1;

  if (!has_table)
    flags |= WASMJIT_HIGH_INSTANTIATE_EMSCRIPTEN_RUNTIME_FLAGS_NO_TABLE;

  if (wasmjit_high_instantiate_emscripten_runtime(&high,
                                                  static_bump,
                                                  tablemin, tablemax, flags)) {
    msg = "failed to instantiate emscripten runtime";
    goto error;
  }

  if (wasmjit_high_instantiate(&high, filename, "asm", 0)) {
    msg = "failed to instantiate module";
    goto error;
  }

  ret = wasmjit_high_emscripten_invoke_main(&high, "asm",
                                            argc, argv, envp, 0);

  if (WASMJIT_IS_TRAP_ERROR(ret)) {
    printf("TRAP: %s\n",
                        wasmjit_trap_reason_to_string(WASMJIT_DECODE_TRAP_ERROR(ret)));
  } else if (ret < 0) {
    msg = "failed to invoke main";
    goto error;
  }

  if (0) {
    char error_buffer[256];

    error:
      ret = wasmjit_high_error_message(&high,
                                       error_buffer,
                                       sizeof(error_buffer));
      if (!ret) {
        printf("%s: %s\n", msg, error_buffer);
                           ret = -1;
      }
  }

  if (high_init)
    wasmjit_high_close(&high);

  return ret;
//#endif
//  return 0;
}

void enclave_main(const char *path)
{
  char *buf = NULL;
  int has_table;
  size_t tablemin = 0, tablemax = 0;
  uint32_t static_bump = 0;

  printf("hello enclave: %s\n", path);

  //dump_wasm_module(path);

  // Hard-coded
  static_bump = 10000;
  has_table = 1;
  tablemin = 10;
  tablemax = 10;

  run_emscripten_file(path,
                      static_bump, has_table, tablemin, tablemax,
                      0, NULL, NULL);

  return;
}
