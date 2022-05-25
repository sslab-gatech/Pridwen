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

#ifndef __SGXWASM__RUNTIME_H__
#define __SGXWASM__RUNTIME_H__

#include <sgxwasm/ast.h>
#include <sgxwasm/sys.h>
#include <sgxwasm/vector.h>

struct Value
{
  sgxwasm_valtype_t type;
  union ValueUnion
  {
    uint32_t i32;
    uint64_t i64;
    float f32;
    double f64;
    struct
    { } null; } data;
  uint64_t val;
};

struct Function
{
  const struct Module* module;
  char* name;
  void* code;
  size_t size;
  size_t fun_index;
  size_t type_index;
  size_t stack_usage;
  struct FuncType type;
};

struct Table
{
  struct Function** data;
  unsigned elemtype;
  size_t length;
  size_t max;
};

struct Memory
{
  char* data;
  size_t size;
  size_t max; /* max of 0 means no max */
};

struct Global
{
  struct Value value;
  unsigned mut;
};

struct Export
{
  char* name;
  sgxwasm_desc_t type;
  union ExportPtr
  {
    struct Function* func;
    struct Table* table;
    struct Memory* mem;
    struct Global* global;
  } value;
};

struct Module
{
  struct FuncTypeVector
  {
    size_t capacity;
    size_t size;
    struct FuncType* data;
  } types;
  DEFINE_ANON_VECTOR(struct Function*) funcs;
  DEFINE_ANON_VECTOR(struct Table*) tables;
  DEFINE_ANON_VECTOR(struct Memory*) mems;
  DEFINE_ANON_VECTOR(struct Global*) globals;
  DEFINE_ANON_VECTOR(struct Export) exports;
  size_t n_imported_funcs, n_imported_tables, n_imported_mems,
    n_imported_globals;
  void* private_data;
  void (*free_private_data)(void*);
};

DECLARE_VECTOR_GROW(func_types, struct FuncTypeVector);

struct NamedModule
{
  char* name;
  struct Module* module;
};

#define IS_HOST(Function) ((Function)->host_function)

#define WASM_PAGE_SIZE ((size_t)(64 * 1024))

const struct Module*
sgxwasm_get_module(const struct Function*);

void sgxwasm_init_code_region(size_t);
int sgxwasm_commit_code_region();
uint64_t sgxwasm_get_code_base();

void
_sgxwasm_create_func_type(struct FuncType* ft,
                          size_t n_inputs,
                          sgxwasm_valtype_t* input_types,
                          size_t n_outputs,
                          sgxwasm_valtype_t* output_types);

int
sgxwasm_typecheck_func(const struct FuncType* expected_type,
                       const struct Function* func);

int
sgxwasm_typecheck_table(const struct TableType* expected_type,
                        const struct Table* table);

int
sgxwasm_typecheck_memory(const struct MemoryType* expected_type,
                         const struct Memory* mem);

int
sgxwasm_typecheck_global(const struct GlobalType* expected_type,
                         const struct Global* mem);

__attribute__((unused)) static int
sgxwasm_typelist_equal(size_t nelts,
                       const sgxwasm_valtype_t* elts,
                       size_t onelts,
                       const sgxwasm_valtype_t* oelts)
{
  size_t i;
  if (nelts != onelts)
    return 0;
  for (i = 0; i < nelts; ++i) {
    if (elts[i] != oelts[i])
      return 0;
  }
  return 1;
}

enum
{
  SGXWASM_TRAP_UNREACHABLE = 1,
  SGXWASM_TRAP_TABLE_OVERFLOW,
  SGXWASM_TRAP_UNINITIALIZED_TABLE_ENTRY,
  SGXWASM_TRAP_MISMATCHED_TYPE,
  SGXWASM_TRAP_MEMORY_OVERFLOW,
  SGXWASM_TRAP_ABORT,
  SGXWASM_TRAP_STACK_OVERFLOW,
  SGXWASM_TRAP_INTEGER_OVERFLOW,
};

__attribute__((unused)) static const char*
sgxwasm_trap_reason_to_string(int reason)
{
  const char* msg;
  switch (reason) {
    case SGXWASM_TRAP_UNREACHABLE:
      msg = "unreachable instruction hit";
      break;
    case SGXWASM_TRAP_TABLE_OVERFLOW:
      msg = "table overflow";
      break;
    case SGXWASM_TRAP_UNINITIALIZED_TABLE_ENTRY:
      msg = "uninitialized table entry";
      break;
    case SGXWASM_TRAP_MISMATCHED_TYPE:
      msg = "mismatched type";
      break;
    case SGXWASM_TRAP_MEMORY_OVERFLOW:
      msg = "memory overflow";
      break;
    case SGXWASM_TRAP_STACK_OVERFLOW:
      msg = "stack overflow";
      break;
    default:
      assert(0);
      __builtin_unreachable();
  }
  return msg;
}

void
sgxwasm_trap(int reason);

void
sgxwasm_free_function(struct Function*);
void
sgxwasm_free_module(struct Module*);

void*
sgxwasm_allocate_code(size_t, int, int);
int
sgxwasm_mark_code_segment_executable(void*, size_t);
int
sgxwasm_unmap_code_segment(void*, size_t);

union ExportPtr
sgxwasm_get_export(const struct Module*, const char* name, sgxwasm_desc_t type);

#define SGXWASM_CHECK_RANGE_SANITIZE(toret, cond, index)                       \
  do {                                                                         \
    *(toret) = (cond);                                                         \
    /* NB: don't assume toret's value,                                         \
       forces actual computation of mask */                                    \
    __asm__("" : "=r"(*(toret)) : "0"(*(toret)));                              \
    *(index) &= ((*(index) - *(index)) - *(toret));                            \
    /* prevent moving masking of index after conditional on toret */           \
    __asm__("" : "=r"(*(toret)) : "0"(*(toret)));                              \
  } while (0)

#endif
