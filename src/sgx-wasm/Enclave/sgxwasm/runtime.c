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

#include <sgxwasm/runtime.h>

#include <sgxwasm/ast.h>
#include <sgxwasm/util.h>

#include <sgxwasm/sys.h>

DEFINE_VECTOR_GROW(func_types, struct FuncTypeVector);

/* end platform specific */

union ExportPtr
sgxwasm_get_export(const struct Module* module,
                   const char* name,
                   sgxwasm_desc_t type)
{
  size_t i;
  union ExportPtr ret;

  for (i = 0; i < module->exports.size; ++i) {
    if (strcmp(module->exports.data[i].name, name))
      continue;

    if (module->exports.data[i].type != type)
      break;

    return module->exports.data[i].value;
  }

  switch (type) {
    case IMPORT_DESC_TYPE_FUNC:
      ret.func = NULL;
      break;
    case IMPORT_DESC_TYPE_TABLE:
      ret.table = NULL;
      break;
    case IMPORT_DESC_TYPE_MEM:
      ret.mem = NULL;
      break;
    case IMPORT_DESC_TYPE_GLOBAL:
      ret.global = NULL;
      break;
  }

  return ret;
}

const struct Module*
sgxwasm_get_module(const struct Function* func)
{
  assert(func->module != NULL);
  return func->module;
}

void
sgxwasm_free_function(struct Function* func)
{
  if (func->code)
    sgxwasm_unmap_code_segment(func->code, func->size);
  free(func);
}

void
sgxwasm_free_module(struct Module* module)
{
  size_t i;
  if (module->free_private_data)
    module->free_private_data(module->private_data);
  free(module->types.data);
  for (i = module->n_imported_funcs; i < module->funcs.size; ++i) {
    sgxwasm_free_function(module->funcs.data[i]);
  }
  free(module->funcs.data);
  for (i = module->n_imported_tables; i < module->tables.size; ++i) {
    free(module->tables.data[i]->data);
    free(module->tables.data[i]);
  }
  free(module->tables.data);
  for (i = module->n_imported_mems; i < module->mems.size; ++i) {
    free(module->mems.data[i]->data);
    free(module->mems.data[i]);
  }
  free(module->mems.data);
  for (i = module->n_imported_globals; i < module->globals.size; ++i) {
    free(module->globals.data[i]);
  }
  free(module->globals.data);
  for (i = 0; i < module->exports.size; ++i) {
    if (module->exports.data[i].name)
      free(module->exports.data[i].name);
  }
  free(module->exports.data);
  free(module);
}

int
sgxwasm_typecheck_func(const struct FuncType* type, const struct Function* func)
{
  return sgxwasm_typelist_equal(type->n_inputs,
                                type->input_types,
                                func->type.n_inputs,
                                func->type.input_types) &&
         sgxwasm_typelist_equal(FUNC_TYPE_N_OUTPUTS(type),
                                FUNC_TYPE_OUTPUT_TYPES(type),
                                FUNC_TYPE_N_OUTPUTS(&func->type),
                                FUNC_TYPE_OUTPUT_TYPES(&func->type));
}

int
sgxwasm_typecheck_table(const struct TableType* type, const struct Table* table)
{
  return (table->elemtype == type->elemtype &&
          table->length >= type->limits.min &&
          (!type->limits.max ||
           (type->limits.max && table->max && table->max <= type->limits.max)));
}

int
sgxwasm_typecheck_memory(const struct MemoryType* type,
                         const struct Memory* mem)
{
  size_t msize = mem->size / WASM_PAGE_SIZE;
  size_t mmax = mem->max / WASM_PAGE_SIZE;
  return (msize >= type->limits.min &&
          (!type->limits.max ||
           (type->limits.max && mmax && mmax <= type->limits.max)));
}

int
sgxwasm_typecheck_global(const struct GlobalType* globaltype,
                         const struct Global* global)
{
  return global->value.type == globaltype->valtype &&
         global->mut == globaltype->mut;
}

void
_sgxwasm_create_func_type(struct FuncType* ft,
                          size_t n_inputs,
                          sgxwasm_valtype_t* input_types,
                          size_t n_outputs,
                          sgxwasm_valtype_t* output_types)
{
  assert(n_outputs <= 1);
  assert(n_inputs <= sizeof(ft->input_types) / sizeof(ft->input_types[0]));
  memset(ft, 0, sizeof(*ft));

  ft->n_inputs = n_inputs;
  memcpy(ft->input_types, input_types, sizeof(input_types[0]) * n_inputs);

  if (n_outputs) {
    ft->output_type = output_types[0];
  } else {
    ft->output_type = VALTYPE_NULL;
  }
}

union ValueUnion
sgxwasm_invoke_function_raw(struct Function* func, union ValueUnion* values)
{
#ifndef __x86_64__
#error Only works on x86_64
#endif
  // return Function->invoker(values);
  (void)func;
  return *values;
}
