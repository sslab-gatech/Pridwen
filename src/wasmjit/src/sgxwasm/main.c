/* -*-mode:c; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
  Copyright (c) 2018 Rian Hunter et. al, see AUTHORS file.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
 */

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
#include <sgxwasm/util.h>

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <regex.h>
#include <unistd.h>

#include <sys/types.h>

int
run_wasm_test(const char*, uint32_t, int, size_t, size_t);


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
    fprintf(stderr,
            "# Functions != # Codes %" PRIu32 " != %" PRIu32 "\n",
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
lpow(long base, long exp, long* out)
{
  *out = 1;

  while (1) {
    if (exp & 1)
      if (__builtin_mul_overflow(*out, base, out))
        return -1;
    exp >>= 1;
    if (!exp)
      break;
    if (__builtin_mul_overflow(base, base, &base))
      return -1;
  }

  return 0;
}

static int
get_static_bump(const char* filename, uint32_t* static_bump)
{
  char *js_path = NULL, *filebuf = NULL, *filebuf2 = NULL;
  size_t fnlen, filesize;
  regex_t re;
  regmatch_t pmatch[5];
  int ret, compiled = 0;
  long result;

  fnlen = strlen(filename);
  if (fnlen < 4)
    goto error;

  js_path = malloc(fnlen + 1);
  memcpy(js_path, filename, fnlen - 4);
  strcpy(&js_path[fnlen - 4], "js");

  filebuf = sgxwasm_load_file(js_path, &filesize);
  if (!filebuf)
    goto error;

  /* we need to null terminal the file... */
  filebuf2 = malloc(filesize + 1);
  if (!filebuf2)
    goto error;

  memcpy(filebuf2, filebuf, filesize);
  filebuf2[filesize] = '\0';

  ret =
    regcomp(&re,
            "(^|;|\n) *var +STATIC_BUMP *= *([0-9]+)([eE][0-9]+)? *(;|$|\n)",
            REG_EXTENDED);
  if (ret)
    goto error;

  compiled = 1;

  ret = regexec(&re, filebuf2, 4, pmatch, 0);
  if (ret)
    goto error;

  result = strtol(filebuf2 + pmatch[2].rm_so, NULL, 10);
  if ((result == LONG_MIN || result == LONG_MAX) && errno == ERANGE)
    goto error;

  if (pmatch[3].rm_so >= 0) {
    long exponent, coefficient;
    exponent = strtol(filebuf2 + pmatch[3].rm_so + 1, NULL, 10);
    if ((exponent == LONG_MIN || exponent == LONG_MAX) && errno == ERANGE)
      goto error;

    assert(exponent >= 0);

    if (lpow(10, exponent, &coefficient))
      goto error;

    if (__builtin_mul_overflow(coefficient, result, &result))
      goto error;
  }

  assert(result >= 0);
  if (result > UINT32_MAX)
    goto error;

  *static_bump = result;

  ret = 0;

  if (0) {
  error:
    ret = -1;
  }

  if (compiled)
    regfree(&re);
  free(filebuf2);
  if (filebuf)
    sgxwasm_unload_file(filebuf, filesize);
  free(js_path);

  return ret;
}

__attribute__((unused)) static int
get_emscripten_runtime_parameters(const char* filename,
                                  uint32_t* static_bump,
                                  int* has_table,
                                  size_t* tablemin,
                                  size_t* tablemax)
{
  size_t i;
  int ret;
  struct WASMModule module;

  sgxwasm_init_wasm_module(&module);

  if (parse_module(filename, &module))
    goto error;

  /* find correct tablemin and tablemax */
  for (i = 0; i < module.import_section.n_imports; ++i) {
    struct ImportSectionImport* import;
    import = &module.import_section.imports[i];
    if (strcmp(import->module, "env") || strcmp(import->name, "table") ||
        import->desc_type != IMPORT_DESC_TYPE_TABLE)
      continue;

    *tablemin = import->desc.tabletype.limits.min;
    *tablemax = import->desc.tabletype.limits.max;
    break;
  }

  *has_table = i != module.import_section.n_imports;

  ret = get_static_bump(filename, static_bump);
  if (ret) {
    fprintf(stderr, "Couldn't get static bump!\n");
    goto error;
  }

  if (0) {
  error:
    ret = -1;
  }

  sgxwasm_free_wasm_module(&module);

  return ret;
}

__attribute__((unused)) static int
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
  // Initialize emscripten context.
  emscripten_context_init(&ctx);

  // Initialize pass manager.
  pass_manager_init(&pm);
  // Call the init function of each pass.
  passes_init(&pm);

#if __DEBUG_PASS__
  dump_passes(&pm);
#endif
  stack_top = get_stack_top();
  if (!stack_top) {
    // fprintf(stderr, "warning: running without a stack limit\n");
  }

  if (sgxwasm_high_init(&high)) {
    msg = "failed to initialize";
    goto error;
  }
  high_init = 1;

  if (!has_table)
    flags |= SGXWASM_HIGH_INSTANTIATE_EMSCRIPTEN_RUNTIME_FLAGS_NO_TABLE;

  if (sgxwasm_high_instantiate_emscripten_runtime(
        &high, &ctx, static_bump, tablemin, tablemax, flags)) {
    msg = "failed to instantiate emscripten runtime";
    goto error;
  }

  if (sgxwasm_high_instantiate(&high, &pm, filename, "asm", 0)) {
    msg = "failed to instantiate module";
    goto error;
  }

  ret = sgxwasm_high_emscripten_invoke_main(
    &high, &ctx, "asm", argc, argv, envp, 0);

  if (SGXWASM_IS_TRAP_ERROR(ret)) {
    fprintf(stderr,
            "TRAP: %s\n",
            sgxwasm_trap_reason_to_string(SGXWASM_DECODE_TRAP_ERROR(ret)));
  } else if (ret < 0) {
    msg = "failed to invoke main";
    goto error;
  }

  if (0) {
    char error_buffer[256];

  error:
    ret = sgxwasm_high_error_message(&high, error_buffer, sizeof(error_buffer));
    if (!ret) {
      fprintf(stderr, "%s: %s\n", msg, error_buffer);
      ret = -1;
    }
  }

  if (high_init)
    sgxwasm_high_close(&high);

  return ret;
}

extern char** environ;
int
main(int argc, char* argv[])
{
  int ret = 0;
  char* filename;
  int dump_module, opt;
  int has_table;
  size_t tablemin = 0, tablemax = 0;
  uint32_t static_bump = 0;

  dump_module = 0;
  while ((opt = getopt(argc, argv, "dop")) != -1) {
    switch (opt) {
      case 'd':
        dump_module = 1;
        break;
      default:
        return -1;
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "Need an input file\n");
    return -1;
  }

  filename = argv[optind];

  if (dump_module)
    return dump_wasm_module(filename);

  // Hard-coded
  // static_bump = 10000;
  static_bump = 26320;
  has_table = 1;
  tablemin = 10;
  tablemax = 10;

#if SGXWASM_SPEC_TEST
  ret = run_wasm_test(filename, static_bump, has_table, tablemin, tablemax);
#else
  ret = run_emscripten_file(filename,
                            static_bump,
                            has_table,
                            tablemin,
                            tablemax,
                            argc - optind,
                            &argv[optind],
                            environ);
#endif
  return ret;
}
