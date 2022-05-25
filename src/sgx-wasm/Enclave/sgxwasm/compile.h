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

#ifndef __SGXWASM__COMPILE_H__
#define __SGXWASM__COMPILE_H__

#include <sgxwasm/assembler.h>
#include <sgxwasm/ast.h>
#include <sgxwasm/config.h>
#include <sgxwasm/register.h>
#include <sgxwasm/runtime.h>
#include <sgxwasm/sys.h>
#include <sgxwasm/util.h>
#include <sgxwasm/vector.h>

// Forward declaration.
struct PassManager;

struct ModuleTypes
{
  struct FuncType* functypes;
  struct TableType* tabletypes;
  struct MemoryType* memorytypes;
  struct GlobalType* globaltypes;
};

struct MemoryReferences
{
  size_t capacity;
  size_t size;
  struct MemoryRef
  {
    enum
    {
      MEMREF_FUNC,
      MEMREF_TABLE,
      MEMREF_TABLE_SIGS,
      MEMREF_TABLE_SIZE,
      MEMREF_MEM,
      MEMREF_GLOBAL,
      MEMREF_TRAP,
      MEMREF_SPRINGBOARD_BEGIN,
      MEMREF_SPRINGBOARD_NEXT,
      MEMREF_SPRINGBOARD_END,
      MEMREF_JMP_NEXT,
      MEMREF_LEA_NEXT,
      MEMREF_CODE_UNIT,
      MEMREF_BR_TABLE_JMP,
      MEMREF_BR_CASE_JMP,
      MEMREF_BR_TABLE_TARGET,
      MEMREF_BR_CASE_TARGET,
      MEMREF_SSA_POLLING,
    } type;
    size_t code_offset;
    size_t idx;
    size_t unit_idx; // The idx is used for target
    size_t target_type;
    size_t size;
    size_t depth;
  } * data;
};
DECLARE_VECTOR_GROW(memrefs, struct MemoryReferences);

struct MemoryRef* new_memref(struct MemoryReferences*);

#define SGXWASM_COMPILE_FLAG_INTEL_RETPOLINE 1
#define SGXWASM_COMPILE_FLAG_AMD_RETPOLINE 2

size_t
dedup_type(const struct FuncTypeVector *, size_t);

char*
sgxwasm_compile_function(struct PassManager*,
                         //const struct TypeSection*,
                         const struct FuncTypeVector*,
                         const struct ModuleTypes*,
                         const struct Function*,
                         const struct Memory*,
                         size_t,
                         const struct CodeSectionCode*,
                         struct MemoryReferences*,
                         size_t*,
                         size_t*,
                         unsigned);

// Definition of data structures.

enum Location
{
  LOC_REGISTER = 0xa0,
  LOC_STACK = 0xa1,
  LOC_CONST = 0xa2,
  LOC_NONE = 0xa3,
};
typedef enum Location location_t;

#define is_reg(loc) (loc == LOC_REGISTER)
#define is_stack(loc) (loc == LOC_STACK)
#define is_const(loc) (loc == LOC_CONST)

struct StackState
{
  size_t capacity;
  size_t size;
  struct StackSlot
  {
    sgxwasm_valtype_t type;
    location_t loc;
    sgxwasm_register_t reg;
    int32_t i32_const;
  } * data;
  size_t stack_base;
};

// DEFINE_VECTOR_GROW(stack, struct StackState);
// DEFINE_VECTOR_RESIZE(stack, struct StackState);

struct CacheState
{
  struct StackState* stack_state;
  reglist_t* used_registers;
  uint32_t* register_use_count;
  reglist_t* last_spilled_regs;
  size_t stack_base;
};

enum CONTROL_TYPE
{
  CONTROL_BLOCK = 0xb0,
  CONTROL_IF = 0xb1,
  CONTROL_ELSE = 0xb2,
  CONTROL_LOOP = 0xb3,
};
typedef enum CONTROL_TYPE control_type_t;

enum REACHABILITY_TYPE
{
  // reachable code.
  Reachable = 0xc0,
  // reachable code in unreachable block (implies normal validation).
  Unreachable = 0xc1,
  // code unreachable in its own block (implies polymorphic validation).
  SpecOnlyReachable = 0xc2,
};
typedef enum REACHABILITY_TYPE reachability_type_t;

struct Control
{
  size_t capacity;
  size_t size;
  struct ControlBlock
  {
    control_type_t type;
    uint32_t in_arity;
    uint32_t out_arity;
    uint8_t start_reached;
    uint8_t end_reached;
    reachability_type_t reachability;
    label_t label;
    struct CacheState label_state;
    // Backward compatiable.
    size_t cont;
    const struct Instr* instructions;
    size_t n_instructions;
    struct ElseState
    {
      size_t cont;
      const struct Instr* instructions;
      size_t n_instructions;
      label_t label;
      struct CacheState state;
    } else_state;
  } * data;
};

#define control_depth(control_stack) ((int)control_stack->size)

#define is_if(type) (type == CONTROL_IF || type == CONTROL_ELSE)
#define is_onearmed_if(type) (type == CONTROL_IF)
#define is_else(type) (type == CONTROL_ELSE)
#define is_loop(type) (type == CONTROL_LOOP)

// Ref: OutOfLineCode
enum RuntimeStubId
{
  TrapDivByZero = 0xd0,
  TrapRemByZero = 0xd1,
  TrapDivUnrepresentable = 0xd2,
  TrapFloatUnrepresentable = 0xd3,
  TrapFuncInvalid = 0xd4,
  TrapFuncSigMismatch = 0xd5,
  TrapMemOutOfBounds = 0xd6,
};

struct OutOfLineCodeList
{
  size_t capacity;
  size_t size;
  struct OutOfLineCode
  {
    label_t label;
    label_t continuation;
    uint32_t stub;
    size_t position;
    reglist_t regs_to_save;
    size_t pc;
  } * data;
};
// DEFINE_VECTOR_GROW(ool, struct OutOfLineCodeList);

// Ref: LinkageLocation class.
struct LocationSignature
{
  size_t capacity;
  size_t size;
  struct LinkageLocation
  {
    location_t loc;
    union
    {
      uint32_t fp;
      uint32_t gp;
      uint32_t stack_offset;
    };
  } * data;
};
// DEFINE_VECTOR_GROW(signature, struct LocationSignature);

struct ParameterAllocator
{
  uint32_t gp_count;
  uint32_t gp_offset;
  uint32_t fp_count;
  uint32_t fp_offset;
  uint32_t stack_offset;
};

// Ref: StackTransferRecipe

struct MovesStorage
{
  sgxwasm_register_t dst;
  sgxwasm_valtype_t type;
  location_t src_loc;
  sgxwasm_register_t src;
};

struct LoadsStorage
{
  sgxwasm_register_t dst;
  sgxwasm_valtype_t type;
  location_t src_loc;
  size_t stack_index;
  uint32_t i32_const;
};

struct StackTransferRecipe
{
  struct MovesStorage register_moves[RegisterNum];
  struct LoadsStorage register_loads[RegisterNum];
  int src_reg_use_count[RegisterNum];
  reglist_t move_dst_regs;
  reglist_t load_dst_regs;
};

#if MEMORY_TRACE // Memory access analysis
enum MemoryAccessType
{
  MEM_ACCESS_READ = 0xf0,
  MEM_ACCESS_WRITE = 0xf1,
  MEM_ACCESS_RW = 0xf2,
};
typedef enum MemoryAccessType mem_access_type_t;

enum MemoryBaseType
{
  MEM_STACK = 0xe0,
  MEM_GLOBAL = 0xe1,
  MEM_INDIRECT = 0xe2,
};
typedef enum MemoryBaseType mem_base_type_t;

struct MemoryTracer
{
  size_t capacity;
  size_t size;
  struct MemoryAccess
  {
    size_t index;
    control_type_t control_type;
    size_t control_depth;
    mem_access_type_t access_type;
    mem_base_type_t base_type;
    size_t offset;
  } * data;
};
#define mem_tracer(ctx) (&(ctx)->mem_tracer)

#endif

// MachineInstr Type.
enum
{
  UcondBranch = 0x10,
  CondBranch = 0x11,
  StackStore = 0x12,
  StackLoad = 0x13,
  RegLoadConst = 0x14,
  BrTableJmp = 0x15,
  BrCaseJmp = 0x16,
  BrTableTarget = 0x17,
  BrCaseTarget = 0x18,
  BindLabel = 0x19,
  CallFunction = 0x1a,
  Return = 0x1b,
};

// Machine instruction
struct MachineInstr
{
  // Reference to the IR-level instruction.
  const struct Instr *instr;
  uint8_t type;
  size_t size;
  // For branch instructions.
  uint32_t depth;
  // For br_table.
  uint32_t min;
  uint32_t max;
  // For bind_label.
  label_t *label;
  // For call_direct.
  size_t fun_index;
};

// Ref: WasmFullDecoder

struct CompileState
{
  const struct Instr* instr;
  size_t instr_id;
  const struct Instr* instr_list;
  size_t n_instrs;
};

struct CompilerContext
{
  const struct Function* func;
  struct SizedBuffer output;
  struct Control control_stack;
  struct OutOfLineCodeList ool_list;
  struct CacheState cache_state;
  struct CompileState compile_state;
  uint32_t num_used_spill_slots;
  struct FuncType sig;
  uint32_t num_locals;
  uint32_t min_memory_size;
  uint32_t max_memory_size;
  struct MemoryReferences* memrefs;
  // Allow referecing pass manager.
  struct PassManager* pm;
  size_t num_low_instrs;
#if MEMORY_TRACE
  struct MemoryTracer mem_tracer;
#endif
};

#define output(ctx) (&(ctx)->output)
#define control(ctx) (&(ctx)->control_stack)
#define out_of_line_code(ctx) (&(ctx)->ool_list)
#define cache_state(ctx) (&(ctx)->cache_state)
#define num_used_spill_slots(ctx) (&(ctx)->num_used_spill_slots)
#define num_locals(ctx) ((ctx)->num_locals)
#define compile_state(ctx) (&(ctx)->compile_state)
#define instr_id(ctx) (ctx->compile_state.instr_id)
#define instr_list(ctx) ((ctx)->compile_state.instr_list)
#define num_instrs(ctx) (ctx->compile_state.n_instrs)
#define num_low_instrs(ctx) (ctx->num_low_instrs)

#define stack_height(cache_state) (cache_state)->stack_state->size

struct ControlBlock*
control_at(struct Control*, int);

// End of definition of data structures.

// High-level APIs
void
spill_register(struct CompilerContext*, sgxwasm_register_t);

void
get_local(struct CompilerContext*, size_t);
int
push_const(struct CompilerContext*, sgxwasm_valtype_t, int32_t);

#endif
