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

#include <sgxwasm/compile.h>
#include <sgxwasm/runtime.h>
#include <sgxwasm/util.h>

#include <sgxwasm/sys.h>

#include <sgxwasm/emscripten.h>

static struct Function*
alloc_func(struct Module* module,
           void* _fptr,
           sgxwasm_valtype_t _output,
           size_t n_inputs,
           sgxwasm_valtype_t* inputs)
{
  struct Function* tmp_func = NULL;

  tmp_func = calloc(1, sizeof(struct Function));
  if (!tmp_func)
    goto error;
  tmp_func->module = module;
  tmp_func->type.n_inputs = n_inputs;
  memcpy(tmp_func->type.input_types, inputs, n_inputs);
  tmp_func->type.output_type = _output;
  tmp_func->code = _fptr;
  tmp_func->name = NULL;
  tmp_func->fun_index = 0;

  if (0) {
  error:
    if (tmp_func)
      sgxwasm_free_function(tmp_func);
    tmp_func = NULL;
  }

  return tmp_func;
}

struct NamedModule*
sgxwasm_instantiate_emscripten_runtime(struct EmscriptenContext* ctx,
                                       uint32_t static_bump,
                                       int has_table,
                                       size_t tablemin,
                                       size_t tablemax,
                                       size_t* amt)
{
  struct
  {
    size_t capacity;
    size_t size;
    struct NamedModule* data;
  } modules;
  struct Function* tmp_func = NULL;
  struct Function** tmp_table_buf = NULL;
  struct Table* tmp_table = NULL;
  char* tmp_mem_buf = NULL;
  struct Memory* tmp_mem = NULL;
  struct Global* tmp_global = NULL;
  struct Module* module = NULL;
  struct NamedModule* ret;
  struct EmscriptenGlobals* globals;

  globals = emscripten_context_derive_memory_globals(ctx, static_bump);

  /* TODO: add exports */

#define LVECTOR_INIT(sstack)                                                   \
  do {                                                                         \
    VECTOR_INIT((sstack));                                                     \
  } while (0)

LVECTOR_INIT(&modules);

#define LVECTOR_GROW(sstack)                                                   \
  do {                                                                         \
    if (!VECTOR_GROW((sstack)))                                                \
      goto error;                                                              \
  } while (0)

#define START_MODULE()                                                         \
  {                                                                            \
    module = calloc(1, sizeof(struct Module));                             \
    if (!module)                                                               \
      goto error;                                                              \
  }

#define STR(x) #x
#define XSTR(x) STR(x)

#define END_MODULE()                                                           \
  {                                                                            \
    if (!strcmp(XSTR(CURRENT_MODULE), "env")) {                                \
      module->private_data = calloc(1, sizeof(struct EmscriptenContext));      \
      if (!module->private_data)                                               \
        goto error;                                                            \
      module->free_private_data = &free;                                       \
    }                                                                          \
    LVECTOR_GROW(&modules);                                                    \
    modules.data[modules.size - 1].name = strdup(XSTR(CURRENT_MODULE));        \
    modules.data[modules.size - 1].module = module;                            \
    module = NULL;                                                             \
  }

#define START_TABLE_DEFS()
#define END_TABLE_DEFS()
#define START_MEMORY_DEFS()
#define END_MEMORY_DEFS()
#define START_GLOBAL_DEFS()
#define END_GLOBAL_DEFS()
#define START_FUNCTION_DEFS()
#define END_FUNCTION_DEFS()

#define DEFINE_WASM_FUNCTION(_name, _fptr, _output, n, ...)                    \
  {                                                                            \
    sgxwasm_valtype_t inputs[] = { __VA_ARGS__ };                              \
    tmp_func = alloc_func(module, _fptr, _output, n, inputs);                  \
    if (!tmp_func)                                                             \
      goto error;                                                              \
    LVECTOR_GROW(&module->funcs);                                              \
    module->funcs.data[module->funcs.size - 1] = tmp_func;                     \
    tmp_func = NULL;                                                           \
                                                                               \
    LVECTOR_GROW(&module->exports);                                            \
    module->exports.data[module->exports.size - 1].name = strdup(#_name);      \
    module->exports.data[module->exports.size - 1].type =                      \
      IMPORT_DESC_TYPE_FUNC;                                                   \
    module->exports.data[module->exports.size - 1].value.func =                \
      module->funcs.data[module->funcs.size - 1];                              \
  }

#define DEFINE_EXTERNAL_WASM_TABLE(name)                                       \
  if (has_##name) {                                                            \
    DEFINE_WASM_TABLE(name, VALTYPE_ANYFUNC, name##min, name##max)             \
  }

#define DEFINE_WASM_TABLE(_name, _elemtype, _min, _max)                        \
  {                                                                            \
    tmp_table_buf = calloc(_min, sizeof(tmp_table_buf[0]));                    \
    if ((_min) && !tmp_table_buf)                                              \
      goto error;                                                              \
    tmp_table = calloc(1, sizeof(struct Table));                           \
    if (!tmp_table)                                                            \
      goto error;                                                              \
    tmp_table->data = tmp_table_buf;                                           \
    tmp_table_buf = NULL;                                                      \
    tmp_table->elemtype = (_elemtype);                                         \
    tmp_table->length = (_min);                                                \
    tmp_table->max = (_max);                                                   \
    LVECTOR_GROW(&module->tables);                                             \
    module->tables.data[module->tables.size - 1] = tmp_table;                  \
    tmp_table = NULL;                                                          \
                                                                               \
    LVECTOR_GROW(&module->exports);                                            \
    module->exports.data[module->exports.size - 1].name = strdup(#_name);      \
    module->exports.data[module->exports.size - 1].type =                      \
      IMPORT_DESC_TYPE_TABLE;                                                  \
    module->exports.data[module->exports.size - 1].value.table =               \
      module->tables.data[module->tables.size - 1];                            \
  }

#define DEFINE_WASM_MEMORY(_name, _min, _max)                                  \
  {                                                                            \
    tmp_mem_buf = calloc((_min)*WASM_PAGE_SIZE, 1);                            \
    if ((_min) && !tmp_mem_buf)                                                \
      goto error;                                                              \
    tmp_mem = calloc(1, sizeof(struct Memory));                               \
    tmp_mem->data = tmp_mem_buf;                                               \
    tmp_mem_buf = NULL;                                                        \
    tmp_mem->size = (_min)*WASM_PAGE_SIZE;                                     \
    tmp_mem->max = (_max)*WASM_PAGE_SIZE;                                      \
    LVECTOR_GROW(&module->mems);                                               \
    module->mems.data[module->mems.size - 1] = tmp_mem;                        \
    tmp_mem = NULL;                                                            \
                                                                               \
    LVECTOR_GROW(&module->exports);                                            \
    module->exports.data[module->exports.size - 1].name = strdup(#_name);      \
    module->exports.data[module->exports.size - 1].type =                      \
      IMPORT_DESC_TYPE_MEM;                                                    \
    module->exports.data[module->exports.size - 1].value.mem =                 \
      module->mems.data[module->mems.size - 1];                                \
  }

#define DEFINE_EXTERNAL_WASM_GLOBAL(_name)                                     \
  DEFINE_WASM_GLOBAL(_name, globals->_name, VALTYPE_I32, i32, 0)

#define DEFINE_WASM_GLOBAL(_name, _init, _type, _member, _mut)                 \
  {                                                                            \
    tmp_global = calloc(1, sizeof(struct Global));                         \
    if (!tmp_global)                                                           \
      goto error;                                                              \
    tmp_global->value.type = (_type);                                          \
    tmp_global->value.data._member = (_init);                                  \
    tmp_global->mut = (_mut);                                                  \
    LVECTOR_GROW(&module->globals);                                            \
    module->globals.data[module->globals.size - 1] = tmp_global;               \
    tmp_global = NULL;                                                         \
                                                                               \
    LVECTOR_GROW(&module->exports);                                            \
    module->exports.data[module->exports.size - 1].name = strdup(#_name);      \
    module->exports.data[module->exports.size - 1].type =                      \
      IMPORT_DESC_TYPE_GLOBAL;                                                 \
    module->exports.data[module->exports.size - 1].value.global =              \
      module->globals.data[module->globals.size - 1];                          \
  }

#include <sgxwasm/emscripten_runtime_def.h>

  if (0) {
  error:
    ret = NULL;

    if (modules.data) {
      size_t i;
      for (i = 0; i < modules.size; i++) {
        struct NamedModule* nm = &modules.data[i];
        free(nm->name);
        sgxwasm_free_module(nm->module);
      }
      free(modules.data);
    }
  } else {
    ret = modules.data;
    if (amt) {
      *amt = modules.size;
    }
  }

  if (module) {
    sgxwasm_free_module(module);
  }
  if (tmp_func) {
    sgxwasm_free_function(tmp_func);
  }
  if (tmp_table_buf)
    free(tmp_table_buf);
  if (tmp_table) {
    free(tmp_table->data);
    free(tmp_table);
  }
  if (tmp_mem_buf)
    free(tmp_mem_buf);
  if (tmp_mem) {
    free(tmp_mem->data);
    free(tmp_mem);
  }
  if (tmp_global)
    free(tmp_global);

  return ret;
}
