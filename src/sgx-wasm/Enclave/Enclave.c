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
#include <stdio.h> /* vsnprintf */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "ocall_stub.h"

#include "sgx_trts.h"

#include "Enclave.h"
#include "Enclave_t.h" /* print_string */

#include <sgxwasm/ast.h>
#include <sgxwasm/ast_dump.h>
#include <sgxwasm/compile.h>
#include <sgxwasm/config.h>
#include <sgxwasm/dynamic_emscripten_runtime.h>
#include <sgxwasm/emscripten.h>
#include <sgxwasm/high_level.h>
#include <sgxwasm/instantiate.h>
#include <sgxwasm/parse.h>
#include <sgxwasm/runtime.h>
#include <sgxwasm/sense.h>
#include <sgxwasm/util.h>

#if 0
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#endif

int
run_wasm_test(const char*, uint32_t, int, size_t, size_t,
              const char*, uint64_t*, uint8_t*, size_t,
	      uint64_t, uint8_t);

static void*
get_stack_top(void)
{
  return NULL;
}

static int
parse_module(const char* filename, struct WASMModule* module)
{
  char* buf = NULL;
  int ret, result;
  size_t size;
  struct ParseState pstate;

  buf = sgxwasm_load_file(filename, &size);
  if (!buf)
    goto error;

  ret = init_pstate(&pstate, buf, size);
  if (!ret)
    goto error;

  ret = read_wasm_module(&pstate, module, NULL, 0);
  if (!ret)
    goto error;

  result = 0;

  if (0) {
  error:
    result = -1;
  }

  if (buf)
    sgxwasm_unload_file(buf, size);
  return result;
}

static int
dump_wasm_module(const char* filename)
{
  uint32_t i;
  struct WASMModule module;
  int res = -1;

  sgxwasm_init_wasm_module(&module);

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
    struct ImportSectionImport* import = &module.import_section.imports[i];
    printf("Import \"%s\" \"%s\"\n", import->module, import->name);
  }

  for (i = 0; i < module.export_section.n_exports; i++) {
    struct ExportSectionExport* export = &module.export_section.exports[i];
    printf("Export \"%s\"\n", export->name);
  }

  for (i = 0; i < module.code_section.n_codes; ++i) {
    uint32_t j;
    struct TypeSectionType* type;
    struct CodeSectionCode* code = &module.code_section.codes[i];

    type = &module.type_section.types[module.function_section.typeidxs[i]];

    printf("Code #%" PRIu32 "\n", i);

    printf("Locals (%" PRIu32 "):\n", code->n_locals);
    for (j = 0; j < code->n_locals; ++j) {
      printf("  %s (%" PRIu32 ")\n",
             sgxwasm_valtype_repr(code->locals[j].valtype),
             code->locals[j].count);
    }

    printf("Signature: [");
    for (j = 0; j < type->n_inputs; ++j) {
      printf("%s,", sgxwasm_valtype_repr(type->input_types[j]));
    }
    printf("] -> [");
    for (j = 0; j < FUNC_TYPE_N_OUTPUTS(type); ++j) {
      printf("%s,", sgxwasm_valtype_repr(FUNC_TYPE_OUTPUT_IDX(type, j)));
    }
    printf("]\n");

    printf("Instructions:\n");
    dump_instructions(module.code_section.codes[i].instructions,
                      module.code_section.codes[i].n_instructions,
                      1);
    printf("\n");
  }

  res = 0;

error:
  sgxwasm_free_wasm_module(&module);

  return res;
}

static int
run_emscripten_file(const char* filename,
                    uint32_t static_bump,
                    int has_table,
                    size_t tablemin,
                    size_t tablemax,
                    int argc,
                    char** argv,
                    char** envp)
{
  struct WasmJITHigh high;
  int ret;
  void* stack_top;
  int high_init = 0;
  const char* msg;
  uint32_t flags = 0;
  struct EmscriptenContext ctx;
  struct PassManager pm;
  struct SystemConfig sys_config;

#if SGXWASM_LOADTIME_BENCH
  uint64_t t1, t2;
  ocall_sgx_rdtsc(&t1);
#endif

  // Initialize emscripten context.
  emscripten_context_init(&ctx);

  // Initialize pass manager.
  pass_manager_init(&pm);
  // Call the init function of each pass.
  passes_init(&pm);

#if __DEMO__
  printf("[PRIDWEN] PassManager initializing..... done\n");
  dump_passes(&pm);
#endif

  init_system_sensing(&sys_config);
  system_sensing(&sys_config);

#if DEBUG_SYS_CONFIG
  dump_system_config(&sys_config);
#endif

#if __DEMO__
  printf("[PRIDWEN] Hardware probing............. done\n");
  dump_system_config(&sys_config);
#endif

#if __DEBUG_PASS__
  dump_passes(&pm);
#endif
  stack_top = get_stack_top();
  if (!stack_top) {
    // fprintf(stderr, "warning: running without a stack limit\n");
  }

  if (sgxwasm_high_init(&high)) {
    msg = "failed to initialize";
    printf("%s\n", msg);
    goto error;
  }
  high_init = 1;

  if (!has_table)
    flags |= SGXWASM_HIGH_INSTANTIATE_EMSCRIPTEN_RUNTIME_FLAGS_NO_TABLE;

  if (sgxwasm_high_instantiate_emscripten_runtime(
        &high, &ctx, static_bump, tablemin, tablemax, flags)) {
    msg = "failed to instantiate emscripten runtime";
    printf("%s\n", msg);
    goto error;
  }
#if SGXWASM_LOADTIME_BENCH
  ocall_sgx_rdtsc(&t2);
  printf("%lu\t", t2 - t1);
#endif

#if SGXWASM_LOADTIME_MEMORY
  //uint64_t counter = sgxwasm_get_alloc_counter();
  //printf("init-time memory: %lu\n", counter);
  //sgxwasm_reset_alloc_counter();
#endif

  if (sgxwasm_high_instantiate(&high, &pm, &sys_config, filename, "asm", 0)) {
    msg = "failed to instantiate module";
    printf("%s\n", msg);
    goto error;
  }

  ret = sgxwasm_high_emscripten_invoke_main(
    &high, &ctx, "asm", argc, argv, envp, 0);

  if (SGXWASM_IS_TRAP_ERROR(ret)) {
    /*fprintf(stderr,
            "TRAP: %s\n",
            sgxwasm_trap_reason_to_string(SGXWASM_DECODE_TRAP_ERROR(ret)));*/
  } else if (ret < 0) {
    msg = "failed to invoke main";
    printf("%s\n", msg);
    goto error;
  }

  if (0) {
    char error_buffer[256];

  error:
    ret = sgxwasm_high_error_message(&high, error_buffer, sizeof(error_buffer));
    if (!ret) {
      // fprintf(stderr, "%s: %s\n", msg, error_buffer);
      ret = -1;
    }
  }

  if (high_init)
    sgxwasm_high_close(&high);

  return ret;
}

void
enclave_main(const char* path)
{
  char* buf;
  int has_table;
  size_t tablemin = 0, tablemax = 0;
  uint32_t static_bump = 0;

  //printf("hello enclave: %s\n", path);

  // Hard-coded
  // static_bump = 10000;
  //static_bump = 26320;
  // static_bump = 50864;
  static_bump = 60000; // lighttpd
  //static_bump = 6000;
  has_table = 1;

  run_emscripten_file(
    path, static_bump, has_table, tablemin, tablemax, 0, NULL, NULL);
  return;
}

void
enclave_spec_test(const char* path, const char* fun_name,
                  uint64_t* args, size_t n_args,
		  uint8_t* args_type, size_t n_args_type,
		  uint64_t expected, uint8_t expected_type)
{
  char* buf;
  int has_table;
  size_t tablemin = 0, tablemax = 0;
  uint32_t static_bump = 0;

  // Hard-coded
  static_bump = 6000;
  has_table = 1;

  run_wasm_test(path, static_bump, has_table, tablemin, tablemax,
                fun_name, args, args_type, n_args_type, expected, expected_type);
  return;
}
