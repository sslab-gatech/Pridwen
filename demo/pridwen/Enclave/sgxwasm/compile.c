#include <sgxwasm/compile.h>
#include <sgxwasm/config.h>
#include <sgxwasm/pass.h>
#include <sgxwasm/sys.h>

#ifndef SGXWASM_DEBUG_STACK
#define SGXWASM_DEBUG_STACK 0
#endif

#ifndef SGXWASM_DEBUG_COMPILE
#define SGXWASM_DEBUG_COMPILE 0
#endif

#ifndef SGXWASM_DEBUG_MEMORY
#define SGXWASM_DEBUG_MEMORY 0
#endif

#ifdef SGXWASM_DEBUG_COMPILE
#include <sgxwasm/ast_dump.h>
#endif

#define FUNC_EXIT_CONT SIZE_MAX

DEFINE_VECTOR_GROW(memrefs, struct MemoryReferences);
DEFINE_VECTOR_FREE(memrefs, struct MemoryReferences);

static DEFINE_VECTOR_INIT(stack, struct StackState);
static DEFINE_VECTOR_GROW(stack, struct StackState);
static DEFINE_VECTOR_SHRINK(stack, struct StackState);
static DEFINE_VECTOR_RESIZE(stack, struct StackState);
static DEFINE_VECTOR_FREE(stack, struct StackState);
static DEFINE_VECTOR_GROW(control, struct Control);
static DEFINE_VECTOR_SHRINK(control, struct Control);
static DEFINE_VECTOR_FREE(control, struct Control);
static DEFINE_VECTOR_GROW(ool, struct OutOfLineCodeList);
static DEFINE_VECTOR_FREE(ool, struct OutOfLineCodeList);
static DEFINE_VECTOR_RESIZE(signature, struct LocationSignature);

#if MEMORY_TRACE
static DEFINE_VECTOR_INIT(mem_tracer, struct MemoryTracer);
static DEFINE_VECTOR_GROW(mem_tracer, struct MemoryTracer);
#endif

//#if SGXWASM_DEBUG_COMPILE
size_t
dump_output(struct SizedBuffer* output, size_t start)
{
  size_t i;
  for (i = start; i < output->size; i++) {
    uint8_t byte = (uint8_t)output->data[i];
    if (byte < 16) {
      printf("0%x ", byte);
      continue;
    }
    printf("%x ", byte);
  }
  printf("\n");

  return i;
}

__attribute__((unused)) static void
dump_cache_state(struct CacheState* cache_state)
{
  struct StackState* stack_state = cache_state->stack_state;
  printf("------------DUMP STACK------------\n");
  assert(stack_state);

  if (stack_state->size == 0) {
    printf("Stack is empty\n");
    goto end;
  }

  int idx;
  for (idx = stack_state->size - 1; idx >= 0; idx--) {
    struct StackSlot* slot = &stack_state->data[idx];
    printf("#%d: ", idx);
    switch (slot->type) {
      case VALTYPE_I32:
        printf("I32 ");
        break;
      case VALTYPE_I64:
        printf("I64 ");
        break;
      case VALTYPE_F32:
        printf("F32 ");
        break;
      case VALTYPE_F64:
        printf("F64 ");
        break;
      default:
        printf("UNKNOWN_TYPE");
        break;
    }
    switch (slot->loc) {
      case LOC_REGISTER:
        dump_reg_info(cache_state->register_use_count, slot->reg);
        break;
      case LOC_STACK:
        printf("STACK[-%d] ", idx);
        break;
      case LOC_CONST:
        printf("CONST[%d] ", slot->i32_const);
        break;
      default:
        printf("UNKNOWN_LOC");
        break;
    }
    printf("\n");
  }

end:
  printf("----------DUMP STACK END----------\n");
}

__attribute__((unused)) static void
dump_cache_state_info(struct CacheState* cache_state)
{
  struct StackState* stack_state = cache_state->stack_state;
  printf("----------DUMP STACK INFO---------\n");
  assert(stack_state);

  size_t height = stack_height(cache_state);
  printf("Stack height: %zu\n", height);
  printf("Stack base: %zu\n", cache_state->stack_base);
  printf("used_registers: 0x%x\n", *(cache_state->used_registers));

  printf("--------DUMP STACK INFO END--------\n");
}

__attribute__((unused)) static void
dump_stack_top(struct CacheState* cache_state, size_t n)
{
  struct StackState* stack_state = cache_state->stack_state;
  printf("----------DUMP STACK TOP----------\n");
  assert(stack_state);

  if (stack_state->size == 0) {
    printf("Stack is empty\n");
    goto end;
  }

  int idx;
  int top_idx = stack_state->size - 1;
  int end_idx = top_idx - n;
  if (end_idx < 0) {
    end_idx = 0;
  }
  for (idx = top_idx; idx >= end_idx; idx--) {
    struct StackSlot* slot = &stack_state->data[idx];
    printf("#%d: ", idx);
    switch (slot->type) {
      case VALTYPE_I32:
        printf("I32 ");
        break;
      case VALTYPE_I64:
        printf("I64 ");
        break;
      case VALTYPE_F32:
        printf("F32 ");
        break;
      case VALTYPE_F64:
        printf("F64 ");
        break;
      default:
        printf("UNKNOWN_TYPE");
        break;
    }
    switch (slot->loc) {
      case LOC_REGISTER:
        dump_reg_info(cache_state->register_use_count, slot->reg);
        break;
      case LOC_STACK:
        printf("STACK[-%d] ", idx);
        break;
      case LOC_CONST:
        printf("CONST[%d] ", slot->i32_const);
        break;
      default:
        printf("UNKNOWN_LOC");
        break;
    }
    printf("\n");
  }

end:
  printf("--------DUMP STACK TOP END--------\n");
}

__attribute__((unused)) static void
dump_stack_slot(struct StackState* stack_state, size_t index)
{
  printf("----------DUMP STACK SLOT----------\n");
  assert(stack_state);

  if (stack_state->size == 0) {
    printf("Stack is empty\n");
    goto end;
  }

  struct StackSlot* slot = &stack_state->data[index];
  printf("#%zu: ", index);
  switch (slot->type) {
    case VALTYPE_I32:
      printf("I32 ");
      break;
    case VALTYPE_I64:
      printf("I64 ");
      break;
    case VALTYPE_F32:
      printf("F32 ");
      break;
    case VALTYPE_F64:
      printf("F64 ");
      break;
    default:
      printf("UNKNOWN_TYPE");
      break;
  }
  switch (slot->loc) {
    case LOC_REGISTER: {
      dump_reg_info(NULL, slot->reg);
      printf(" const: %d ", slot->i32_const);
      break;
    }
    case LOC_STACK:
      printf("STACK[-%zu] ", index);
      printf(" reg: %u, const: %d", slot->reg, slot->i32_const);
      break;
    case LOC_CONST:
      printf("CONST[%d] ", slot->i32_const);
      printf(" reg: %d ", slot->reg);
      break;
    default:
      printf("UNKNOWN_LOC");
      break;
  }
  printf("\n");

end:
  printf("--------DUMP STACK SLOT END-------\n");
}

__attribute__((unused)) static void
dump_control_stack(struct Control* control_stack)
{
  printf("--------DUMP CONTROL STACK--------\n");
  if (control_stack->size == 0) {
    printf("Stack is empty\n");
    goto end;
  }

  int idx;
  for (idx = control_stack->size - 1; idx >= 0; idx--) {
    struct ControlBlock* c = &control_stack->data[idx];
    switch (c->type) {
      case CONTROL_BLOCK:
        printf("BLOCK ");
        break;
      case CONTROL_IF:
        printf("IF ");
        break;
      case CONTROL_ELSE:
        printf("ELSE ");
        break;
      case CONTROL_LOOP:
        printf("LOOP ");
        break;
      default:
        printf("UNKNOWN CONTROL");
        break;
    }
    printf("in: %u, out: %u, start_reached: %u, end_reached: %u,", c->in_arity,
           c->out_arity, c->start_reached, c->end_reached);
    printf(" label: %d, stack height: %zu, else_label: %d", c->label.pos,
           stack_height(&c->label_state), c->else_state.label.pos);
    printf("\n");
  }

end:
  printf("------DUMP CONTROL STACK END------\n");
}
//#endif

#if __PASS__
static void
passes_function_start(struct CompilerContext* ctx)
{
  assert(ctx->func != NULL && ctx->memrefs != NULL && ctx->pm != NULL);
  const struct Function* func = ctx->func;
  struct PassManager* pm = ctx->pm;
  size_t i;
  for (i = 0; i < pm->size; i++) {
    struct Pass* pass = &pm->data[i];
    if (pass->active == 0) {
      continue;
    }
    if (!pass->function_start) {
      continue;
    }
    (pass->function_start)(ctx, func);
  }
}

static void
passes_function_end(struct CompilerContext* ctx)
{
  assert(ctx->func != NULL && ctx->memrefs != NULL && ctx->pm != NULL);
  const struct Function* func = ctx->func;
  struct PassManager* pm = ctx->pm;
  size_t i;
  for (i = 0; i < pm->size; i++) {
    struct Pass* pass = &pm->data[i];
    if (pass->active == 0) {
      continue;
    }
    if (!pass->function_end) {
      continue;
    }
    (pass->function_end)(ctx, func);
  }
}

static void
passes_control_start(struct CompilerContext* ctx, control_type_t type)
{
  assert(ctx->func != NULL && ctx->memrefs != NULL && ctx->pm != NULL);
  const struct Function* func = ctx->func;
  struct PassManager* pm = ctx->pm;
  size_t depth = control_depth(control(ctx));
  size_t i;
  for (i = 0; i < pm->size; i++) {
    struct Pass* pass = &pm->data[i];
    if (pass->active == 0) {
      continue;
    }
    if (!pass->control_start) {
      continue;
    }
    (pass->control_start)(ctx, func, type, depth);
  }
}

static void
passes_control_end(struct CompilerContext* ctx, control_type_t type)
{
  assert(ctx->func != NULL && ctx->memrefs != NULL && ctx->pm != NULL);
  const struct Function* func = ctx->func;
  struct PassManager* pm = ctx->pm;
  // The actual control stack is shrinked after the hooking point,
  // manually minus the depth by one here.
  size_t depth = control_depth(control(ctx)) - 1;
  size_t i;
  for (i = 0; i < pm->size; i++) {
    struct Pass* pass = &pm->data[i];
    if (pass->active == 0) {
      continue;
    }
    if (!pass->control_end) {
      continue;
    }
    (pass->control_end)(ctx, func, type, depth);
  }
}

static void
passes_instruction_start(struct CompilerContext* ctx)
{
  assert(ctx->func != NULL && ctx->memrefs != NULL && ctx->pm != NULL);
  struct PassManager* pm = ctx->pm;
  const struct Function* func = ctx->func;
  const struct Instr* instr = compile_state(ctx)->instr;
  size_t i;
  for (i = 0; i < pm->size; i++) {
    struct Pass* pass = &pm->data[i];
    if (pass->active == 0) {
      continue;
    }
    if (!pass->instruction_start) {
      continue;
    }
    (pass->instruction_start)(ctx, func, instr);
  }
}

static void
passes_instruction_end(struct CompilerContext* ctx)
{
  assert(ctx->func != NULL && ctx->memrefs != NULL && ctx->pm != NULL);
  struct PassManager* pm = ctx->pm;
  const struct Function* func = ctx->func;
  const struct Instr* instr = compile_state(ctx)->instr;
  size_t i;
  for (i = 0; i < pm->size; i++) {
    struct Pass* pass = &pm->data[i];
    if (pass->active == 0) {
      continue;
    }
    if (!pass->instruction_end) {
      continue;
    }
    (pass->instruction_end)(ctx, func, instr);
  }
}

static void
passes_machine_inst_start(struct CompilerContext* ctx,
                          struct MachineInstr* minstr)
{
  assert(ctx->func != NULL && ctx->memrefs != NULL && ctx->pm != NULL);
  struct PassManager* pm = ctx->pm;
  const struct Function* func = ctx->func;
  size_t i;
  for (i = 0; i < pm->size; i++) {
    struct Pass* pass = &pm->data[i];
    if (pass->active == 0) {
      continue;
    }
    if (!pass->machine_inst_start) {
      continue;
    }
    (pass->machine_inst_start)(ctx, func, minstr);
  }
}

static void
passes_machine_inst_end(struct CompilerContext* ctx,
                        struct MachineInstr* minstr)
{
  assert(ctx->func != NULL && ctx->memrefs != NULL && ctx->pm != NULL);
  struct PassManager* pm = ctx->pm;
  const struct Function* func = ctx->func;
  size_t i;
  for (i = 0; i < pm->size; i++) {
    struct Pass* pass = &pm->data[i];
    if (pass->active == 0) {
      continue;
    }
    if (!pass->machine_inst_end) {
      continue;
    }
    (pass->machine_inst_end)(ctx, func, minstr);
  }
}
#endif

#if MEMORY_TRACE
static void
record_memory_access(struct MemoryTracer* mem_tracer, size_t index,
                     control_type_t control_type, size_t control_depth,
                     mem_access_type_t access_type, mem_base_type_t base_type,
                     size_t offset)
{
  struct MemoryAccess* access = &mem_tracer->data[mem_tracer->size - 1];
  access->index = index;
  access->control_type = control_type;
  access->control_depth = control_depth;
  access->access_type = access_type;
  access->base_type = base_type;
  access->offset = offset;
}
#endif

// Machine-level hooks.

__attribute__((unused)) static void
uncond_jmp(struct CompilerContext* ctx, label_t* label, uint32_t depth,
           distance_t distance)
{
  (void)depth;
#if __PASS__
  struct MachineInstr minstr;
  minstr.instr = compile_state(ctx)->instr;
  minstr.type = UcondBranch;
  minstr.depth = depth;
  passes_machine_inst_start(ctx, &minstr);
#endif
  num_low_instrs(ctx) += emit_jmp_label(output(ctx), label, distance);
#if __PASS__
  passes_machine_inst_end(ctx, &minstr);
#endif
}

__attribute__((unused)) static void
cond_jmp(struct CompilerContext* ctx, condition_t cond, label_t* label,
         sgxwasm_valtype_t type, sgxwasm_register_t lhs, sgxwasm_register_t rhs,
         uint32_t depth)
{
  (void)depth;
#if __PASS__
  struct MachineInstr minstr;
  minstr.instr = compile_state(ctx)->instr;
  minstr.type = CondBranch;
  minstr.depth = depth;
  passes_machine_inst_start(ctx, &minstr);
#endif
  num_low_instrs(ctx) +=
    emit_cond_jump_rr(output(ctx), cond, label, type, lhs, rhs);
#if __PASS__
  passes_machine_inst_end(ctx, &minstr);
#endif
}

__attribute__((unused)) static void
br_table_cond_jmp(struct CompilerContext* ctx, condition_t cond, label_t* label,
                  sgxwasm_valtype_t type, sgxwasm_register_t lhs,
                  sgxwasm_register_t rhs, uint32_t min, uint32_t max)
{
#if __PASS__
  struct MachineInstr minstr;
  minstr.instr = compile_state(ctx)->instr;
  minstr.type = BrTableJmp;
  minstr.min = min;
  minstr.max = max;
  passes_machine_inst_start(ctx, &minstr);
#endif
  num_low_instrs(ctx) +=
    emit_cond_jump_rr(output(ctx), cond, label, type, lhs, rhs);
#if __PASS__
  passes_machine_inst_end(ctx, &minstr);
#endif
}

__attribute__((unused)) static void
br_table_uncond_jmp(struct CompilerContext* ctx, label_t* label, uint32_t depth)
{
#if __PASS__
  struct MachineInstr minstr;
  minstr.instr = compile_state(ctx)->instr;
  minstr.type = BrCaseJmp;
  minstr.depth = depth;
  passes_machine_inst_start(ctx, &minstr);
#endif
  num_low_instrs(ctx) += emit_jmp_label(output(ctx), label, Far);
#if __PASS__
  passes_machine_inst_end(ctx, &minstr);
#endif
}

__attribute__((unused)) static void
br_table_bind(struct CompilerContext* ctx, label_t* label, uint8_t type,
              uint32_t min, uint32_t max, uint32_t depth)
{
#if __PASS__
  struct MachineInstr minstr;
  minstr.instr = compile_state(ctx)->instr;
  minstr.type = type;
  if (minstr.type == BrTableTarget) {
    minstr.min = min;
    minstr.max = max;
  } else {
    assert(minstr.type == BrCaseTarget);
    minstr.depth = depth;
  }
  minstr.label = label;
  passes_machine_inst_start(ctx, &minstr);
#endif
  if (!is_bound(label)) {
    bind_label(output(ctx), label, pc_offset(output(ctx)));
  }
#if __PASS__
  passes_machine_inst_end(ctx, &minstr);
#endif
}

__attribute__((unused)) static void
low_bind_label(struct CompilerContext* ctx, label_t* label)
{
#if __PASS__
  struct MachineInstr minstr;
  minstr.instr = compile_state(ctx)->instr;
  minstr.type = BindLabel;
  minstr.label = label;
  passes_machine_inst_start(ctx, &minstr);
#endif
  // Allow us to set the label to avoid binding.
  if (!is_bound(label)) {
    bind_label(output(ctx), label, pc_offset(output(ctx)));
  }
#if __PASS__
  passes_machine_inst_end(ctx, &minstr);
#endif
}

// Wrapper of Spill.
__attribute__((unused)) static void
spill_to_stack(struct CompilerContext* ctx, uint32_t index,
               sgxwasm_register_t reg, int64_t val, sgxwasm_valtype_t type)
{
#if __PASS__
  struct MachineInstr minstr;
  minstr.instr = compile_state(ctx)->instr;
  minstr.type = StackStore;
  passes_machine_inst_start(ctx, &minstr);
#endif
  num_low_instrs(ctx) +=
    Spill(output(ctx), num_used_spill_slots(ctx), index, reg, val, type);
#if __PASS__
  passes_machine_inst_end(ctx, &minstr);
#endif
}

// Wrapper of Fill.
__attribute__((unused)) static void
fill_from_stack(struct CompilerContext* ctx, sgxwasm_register_t reg,
                uint32_t index, sgxwasm_valtype_t type)
{
#if __PASS__
  struct MachineInstr minstr;
  minstr.instr = compile_state(ctx)->instr;
  minstr.type = StackLoad;
  passes_machine_inst_start(ctx, &minstr);
#endif
  num_low_instrs(ctx) += Fill(output(ctx), reg, index, type);
#if __PASS__
  passes_machine_inst_end(ctx, &minstr);
#endif
}

// Wrapper of LoadConstant.
__attribute__((unused)) static void
load_const_to_reg(struct CompilerContext* ctx, sgxwasm_register_t reg,
                  int64_t val, sgxwasm_valtype_t type)
{
#if __PASS__
  struct MachineInstr minstr;
  minstr.instr = compile_state(ctx)->instr;
  minstr.type = RegLoadConst;
  passes_machine_inst_start(ctx, &minstr);
#endif
  num_low_instrs(ctx) += LoadConstant(output(ctx), reg, val, type);
#if __PASS__
  passes_machine_inst_end(ctx, &minstr);
#endif
}

// Wrapper of LoadConstant.
__attribute__((unused)) static void
call_function(struct CompilerContext* ctx, sgxwasm_register_t addr,
              size_t fun_index)
{
#if __PASS__
  struct MachineInstr minstr;
  minstr.instr = compile_state(ctx)->instr;
  minstr.type = CallFunction;
  minstr.fun_index = fun_index;
  passes_machine_inst_start(ctx, &minstr);
#endif
  num_low_instrs(ctx) += emit_call_r(output(ctx), addr);
#if __PASS__
  passes_machine_inst_end(ctx, &minstr);
#endif
}

__attribute__((unused)) static void
low_return(struct CompilerContext* ctx)
{
#if __PASS__
  struct MachineInstr minstr;
  minstr.instr = compile_state(ctx)->instr;
  minstr.type = Return;
  passes_machine_inst_start(ctx, &minstr);
#endif
  // XXX: We assume the return without popping additional bytes for now.
  num_low_instrs(ctx) += emit_ret(output(ctx), 0);
#if __PASS__
  passes_machine_inst_end(ctx, &minstr);
#endif
}

// End of machine-level hooks.

__attribute__((unused)) static unsigned
peek_stack(struct CacheState* cache_state)
{
  size_t size = stack_height(cache_state);
  assert(size);
  return cache_state->stack_state->data[size - 1].type;
}

static void
pop_stack(struct CacheState* cache_state)
{
  assert(stack_height(cache_state));
  stack_shrink(cache_state->stack_state);
}

// Make dst points to src.
static void
steal_state(struct CacheState* dst, struct CacheState* src)
{
  assert(src->stack_state);
  // Steal stack state.
  if (dst->stack_state == NULL) {
    dst->stack_state = malloc(sizeof(struct StackState));
    if (!dst->stack_state)
      assert(0);
    stack_init(dst->stack_state);
  }
  if (dst->stack_state->data != NULL) {
    stack_free(dst->stack_state);
    // free(dst->stack_state->data);
  }
  // Make the dst points to the src.
  dst->stack_state->capacity = src->stack_state->capacity;
  dst->stack_state->size = src->stack_state->size;
  dst->stack_state->data = src->stack_state->data;
  // Make the src points to NULL, which prevents
  // the src being freed upon pop_control.
  stack_init(src->stack_state);

  // Steal register cache.
  dst->used_registers = src->used_registers;
  dst->register_use_count = src->register_use_count;
  dst->last_spilled_regs = src->last_spilled_regs;
  src->used_registers = NULL;
  src->register_use_count = NULL;
  src->last_spilled_regs = NULL;
}

static void
split_state(struct CacheState* dst, struct CacheState* src)
{
  // Copy stack state.
  assert(src->stack_state);
  size_t src_size = stack_height(src);
  if (dst->stack_state == NULL) {
    dst->stack_state = malloc(sizeof(struct StackState));
    if (!dst->stack_state)
      assert(0);
    stack_init(dst->stack_state);
  }
  // Do nothing if the size are equal.
  size_t dst_size = stack_height(dst);
  if (dst_size != src_size) {
#if SGXWASM_DEBUG_MEMORY
    printf("[split_state] stack_resize - old: %zu, new: %zu\n",
           dst->stack_state->size, src_size);
#endif
    if (!stack_resize(dst->stack_state, src_size))
      assert(0);
  }
  memcpy(dst->stack_state->data, src->stack_state->data,
         src_size * sizeof(struct StackSlot));

  // Copy register caches.
  assert(src->used_registers && src->register_use_count &&
         src->last_spilled_regs);
  if (dst->used_registers == NULL) {
    dst->used_registers = malloc(sizeof(reglist_t));
    if (!dst->used_registers)
      assert(0);
  }
  memcpy(dst->used_registers, src->used_registers, sizeof(reglist_t));
  if (dst->register_use_count == NULL) {
    dst->register_use_count = malloc(RegisterNum * sizeof(uint32_t));
    if (!dst->register_use_count)
      assert(0);
  }
  memcpy(dst->register_use_count, src->register_use_count,
         RegisterNum * sizeof(uint32_t));
  if (dst->last_spilled_regs == NULL) {
    dst->last_spilled_regs = malloc(sizeof(reglist_t));
    if (!dst->last_spilled_regs)
      assert(0);
  }
  memcpy(dst->last_spilled_regs, src->last_spilled_regs, sizeof(reglist_t));
}

static void
make_slot(struct StackSlot* slot, sgxwasm_valtype_t type, location_t loc,
          sgxwasm_register_t reg, int32_t i32_const)
{
  slot->type = type;
  slot->loc = loc;
  slot->reg = reg;
  slot->i32_const = i32_const;
}

static void
make_slot_stack(struct StackSlot* slot, sgxwasm_valtype_t type)
{
  make_slot(slot, type, LOC_STACK, REG_UNKNOWN, 0);
}

static void
make_slot_register(struct StackSlot* slot, sgxwasm_valtype_t type,
                   sgxwasm_register_t reg)
{
  make_slot(slot, type, LOC_REGISTER, reg, 0);
}

static void
make_slot_const(struct StackSlot* slot, sgxwasm_valtype_t type,
                int32_t i32_const)
{
  make_slot(slot, type, LOC_CONST, REG_UNKNOWN, i32_const);
}

static int
push_stack(struct CacheState* cache_state, sgxwasm_valtype_t type,
           location_t loc, sgxwasm_register_t reg, int32_t val)
{
  assert(type == VALTYPE_I32 || type == VALTYPE_I64 || type == VALTYPE_F32 ||
         type == VALTYPE_F64);
  if (!stack_grow(cache_state->stack_state))
    return 0;

  int idx = stack_height(cache_state) - 1;
  struct StackSlot* slot = &cache_state->stack_state->data[idx];
  if (loc == LOC_REGISTER) {
    inc_used(cache_state->used_registers, cache_state->register_use_count, reg);
    make_slot_register(slot, type, reg);
  } else if (loc == LOC_CONST) {
    make_slot_const(slot, type, val);
  } else {
#if __OPT_INIT_STACK__
    assert(loc == LOC_STACK);
    make_slot_stack(slot, type);
#else
    assert(0);
#endif
  }

#if SGXWASM_DEBUG_COMPILE
// dump_cache_state(cache_state);
#endif
  return 1;
}

static int
push_register(struct CompilerContext* ctx, sgxwasm_valtype_t type,
              sgxwasm_register_t reg)
{
  return push_stack(cache_state(ctx), type, LOC_REGISTER, reg, 0);
}

int
push_const(struct CompilerContext* ctx, sgxwasm_valtype_t type, int32_t val)
{
  return push_stack(cache_state(ctx), type, LOC_CONST, REG_UNKNOWN, val);
}

struct ControlBlock*
control_at(struct Control* control_stack, int depth)
{
  assert(control_depth(control_stack) > depth);
  return &control_stack->data[control_stack->size - 1 - depth];
}

static int
reachable(struct ControlBlock* c)
{
  return c->reachability == Reachable;
}

__attribute__((unused)) static int
unreachable(struct ControlBlock* c)
{
  return c->reachability == Unreachable;
}

static reachability_type_t
inner_reachability(struct ControlBlock* c)
{
  if (c->reachability == Reachable)
    return Reachable;
  return SpecOnlyReachable;
}

static label_t*
add_out_of_line_trap(struct OutOfLineCodeList* ool_list, size_t position,
                     uint32_t stub, size_t pc)
{
  if (!ool_grow(ool_list)) {
    assert(0);
  }

  struct OutOfLineCode* ool = &ool_list->data[ool_list->size - 1];
  ool->label.pos = 0;
  ool->label.near_link_pos = 0;
  ool->continuation.pos = 0;
  ool->continuation.near_link_pos = 0;
  ool->stub = stub;
  ool->position = position;
  ool->regs_to_save = get_allocable_reg_list();
  ool->pc = pc;

  return &ool->label;
}

// Reference implementation of LinkageLocation

static void
add_param_at(struct LocationSignature* sig, size_t index,
             sgxwasm_register_class_t rc, location_t loc, uint32_t value)
{
  if (sig->size < (index + 1)) {
    if (!signature_resize(sig, index + 1))
      assert(0);
  }

  struct LinkageLocation* location = &sig->data[index];
  if (loc == LOC_REGISTER) {
    location->loc = LOC_REGISTER;
    if (rc == GP_REG) {
      location->gp = value;
    } else {
      assert(rc == FP_REG);
      location->fp = value;
    }
  } else {
    assert(loc == LOC_STACK);
    location->loc = LOC_STACK;
    location->stack_offset = value;
  }
}

static struct LinkageLocation*
get_param(struct LocationSignature* sig, size_t index)
{
  assert(sig->size > index);
  return &sig->data[index];
}

static void
init_parameter_allocator(struct ParameterAllocator* params)
{
  params->gp_count = 0;
  params->gp_offset = GPRegisterParameterNum;
  params->fp_count = 0;
  params->fp_offset = FPRegisterParameterNum;
  params->stack_offset = 0;
}

static int
can_allocate_gp(struct ParameterAllocator* params)
{
  return (params->gp_count < params->gp_offset);
}

static sgxwasm_register_t
next_gp_reg(struct ParameterAllocator* params)
{
  assert(params->gp_count < params->gp_offset);
  sgxwasm_register_t gp = GPParameterList[params->gp_count];
  params->gp_count += 1;
  return gp;
}

static int
can_allocate_fp(struct ParameterAllocator* params)
{
  return (params->fp_count < params->fp_offset);
}

static sgxwasm_register_t
next_fp_reg(struct ParameterAllocator* params)
{
  assert(params->fp_count < params->fp_offset);
  sgxwasm_register_t fp = FPParemterList[params->fp_count];
  params->fp_count += 1;
  return fp;
}

static uint32_t
next_stack_slot(struct ParameterAllocator* params)
{
  // Assume all types of value take one stack slot.
  uint32_t offset = params->stack_offset;
  params->stack_offset += 1;
  return offset;
}

// End of reference implementation of LinkageLocation

// Reference implementation of StackTransferRecipe

__attribute__((unused)) static void
init_stack_transfer_receipe(struct StackTransferRecipe* stack_transfer)
{
  size_t i;
  memset(stack_transfer, 0, sizeof(struct StackTransferRecipe));
  for (i = 0; i < RegisterNum; i++) {
    stack_transfer->register_loads[i].dst = i;
    stack_transfer->register_loads[i].src_loc = LOC_NONE;
    stack_transfer->register_moves[i].dst = i;
    stack_transfer->register_moves[i].src_loc = LOC_NONE;
  }
}

__attribute__((unused)) static void
move_register(struct StackTransferRecipe* stack_transfer,
              sgxwasm_register_t dst, sgxwasm_register_t src,
              sgxwasm_valtype_t type)
{
  assert(dst != src);
  assert(reg_class(dst) == reg_class(src));
  assert(reg_class_for(type) == reg_class(src));
  if (has(stack_transfer->move_dst_regs, dst)) {
    assert(stack_transfer->register_moves[dst].src == src);
    assert((!is_fp(dst)) == (stack_transfer->register_moves[dst].type == type));
    // It can happen that one fp register holds both the f32 zero and the f64
    // zero, as the initial value for local variables. Move the value as f64
    // in that case.
    if (type == VALTYPE_F64) {
      stack_transfer->register_moves[dst].type = VALTYPE_F64;
    }
  }
  set(&stack_transfer->move_dst_regs, dst);
  stack_transfer->src_reg_use_count[src] += 1;
  stack_transfer->register_moves[dst].src_loc = LOC_REGISTER;
  stack_transfer->register_moves[dst].src = src;
  stack_transfer->register_moves[dst].type = type;
}

__attribute__((unused)) static void
load_const(struct StackTransferRecipe* stack_transfer, sgxwasm_register_t dst,
           uint32_t value, sgxwasm_valtype_t type)
{
  assert(!has(stack_transfer->load_dst_regs, dst));
  set(&stack_transfer->load_dst_regs, dst);
  stack_transfer->register_loads[dst].src_loc = LOC_CONST;
  stack_transfer->register_loads[dst].i32_const = value;
  stack_transfer->register_loads[dst].type = type;
}

__attribute__((unused)) static void
load_stack_slot(struct StackTransferRecipe* stack_transfer,
                sgxwasm_register_t dst, uint32_t stack_index,
                sgxwasm_valtype_t type)
{
  if (has(stack_transfer->load_dst_regs, dst)) {
    // It can happen that we spilled the same register to different stack
    // slots, and then we reload them later into the same dst register.
    // In that case, it is enough to load one of the stack slots.
    return;
  }
  set(&stack_transfer->load_dst_regs, dst);
  stack_transfer->register_loads[dst].src_loc = LOC_STACK;
  stack_transfer->register_loads[dst].stack_index = stack_index;
  stack_transfer->register_loads[dst].type = type;
}

__attribute__((unused)) static void
load_into_register(struct StackTransferRecipe* stack_transfer,
                   sgxwasm_register_t dst, struct StackSlot* src,
                   uint32_t src_idx)
{
  switch (src->loc) {
    case LOC_STACK:
      load_stack_slot(stack_transfer, dst, src_idx, src->type);
      break;
    case LOC_REGISTER:
      assert(reg_class(dst) == reg_class(src->reg));
      if (dst != src->reg)
        move_register(stack_transfer, dst, src->reg, src->type);
      break;
    case LOC_CONST:
      load_const(stack_transfer, dst, src->i32_const, src->type);
      break;
    default:
      assert(0);
      break;
  }
}

// Forward declaration.
static void clear_executed_move(struct CompilerContext*,
                                struct StackTransferRecipe*,
                                sgxwasm_register_t);

__attribute__((unused)) static void
execute_move(struct CompilerContext* ctx, struct StackTransferRecipe* transfers,
             sgxwasm_register_t dst)
{
  struct MovesStorage* move = &transfers->register_moves[dst];
  assert(transfers->src_reg_use_count[dst] == 0);

  num_low_instrs(ctx) += Move(output(ctx), dst, move->src, move->type);
  clear_executed_move(ctx, transfers, dst);
}

__attribute__((unused)) static void
clear_executed_move(struct CompilerContext* ctx,
                    struct StackTransferRecipe* transfers,
                    sgxwasm_register_t dst)
{
  assert(has(transfers->move_dst_regs, dst));
  clear(&transfers->move_dst_regs, dst);
  struct MovesStorage* move = &transfers->register_moves[dst];
  assert(transfers->src_reg_use_count[move->src] > 0);
  if (--transfers->src_reg_use_count[move->src])
    return;
  // src count dropped to zero. If this is a destination register, execute
  // that move now.
  if (!has(transfers->move_dst_regs, move->src))
    return;
  execute_move(ctx, transfers, move->src);
}

__attribute__((unused)) static void
execute_moves(struct CompilerContext* ctx,
              struct StackTransferRecipe* transfers)
{
  // Execute all moves whose {dst} is not being used as src in another move.
  // If any src count drops to zero, also (transitively) execute the
  // corresponding move to that register.
  sgxwasm_register_t dst;
  for (dst = 0; dst < RegisterNum; dst++) {
    // Check if already handled via transitivity in {clear_executed_move}.
    if (!has(transfers->move_dst_regs, dst))
      continue;
    if (transfers->src_reg_use_count[dst])
      continue;
    execute_move(ctx, transfers, dst);
  }

  // All remaining moves are parts of a cycle. Just spill the first one, then
  // process all remaining moves in that cycle. Repeat for all cycles.
  uint32_t next_spill_spot = stack_height(cache_state(ctx));
  while (!is_empty(transfers->move_dst_regs)) {
    sgxwasm_register_t dst = get_first_reg_set(transfers->move_dst_regs);
    struct MovesStorage* move = &transfers->register_moves[dst];
    sgxwasm_register_t spill_reg = move->src;
    spill_to_stack(ctx, next_spill_spot, spill_reg, 0, move->type);
#if MEMORY_TRACE
    struct ControlBlock* c =
      &control(ctx)->data[control_depth(control(ctx)) - 1];
    int offset = next_spill_spot * StackSlotSize * -1 - StackOffset;
    if (!mem_tracer_grow(mem_tracer(ctx)))
      assert(0);
    record_memory_access(mem_tracer(ctx), c->cont, c->type,
                         control_depth(control(ctx)), MEM_ACCESS_WRITE,
                         MEM_STACK, offset);
#endif
    // Remember to reload into the destination register later.
    load_stack_slot(transfers, dst, next_spill_spot, move->type);
    ++next_spill_spot;
    clear_executed_move(ctx, transfers, dst);
  }
}

__attribute__((unused)) static void
execute_loads(struct CompilerContext* ctx,
              struct StackTransferRecipe* transfers)
{
  while (!is_empty(transfers->load_dst_regs)) {
    sgxwasm_register_t dst = get_first_reg_set(transfers->load_dst_regs);
    struct LoadsStorage* load = &transfers->register_loads[dst];
    switch (load->src_loc) {
      case LOC_CONST: {
        assert(load->type == VALTYPE_I32 || load->type == VALTYPE_I64);
        load_const_to_reg(ctx, dst, load->i32_const, load->type);
        break;
      }
      case LOC_STACK: {
        fill_from_stack(ctx, dst, load->stack_index, load->type);

#if 0 // This is fix for register spilling after merge_stack_with (end of br).
      // Because stack transfer does not modify the current
      // stack state, spilling register will result in errors.
      // For example,
      //   stack_transfer content:
      //     src_state [20: rax]    -> dst_state [20: stack]
      //     src_state [370: stack] -> dst_state [370: rax]
      //   emit code:
      //     stack[20] = rax
      //     rax  = stack[370]
      //     spill(rax) <--- Based on src_state instead of dst_state
      //     => rax = stack[20]
      //        This will overwrite the rax.
      // The fix is to modify the src_state accordingly.
        struct StackSlot* local_dst = &cache_state(ctx)->stack_state->data[load->stack_index];
        make_slot_register(local_dst, load->type, dst);
#endif

#if MEMORY_TRACE
        struct ControlBlock* c =
          &control(ctx)->data[control_depth(control(ctx)) - 1];
        int offset = load->stack_index * StackSlotSize * -1 - StackOffset;
        if (!mem_tracer_grow(mem_tracer(ctx)))
          assert(0);
        record_memory_access(mem_tracer(ctx), c->cont, c->type,
                             control_depth(control(ctx)), MEM_ACCESS_READ,
                             MEM_STACK, offset);
#endif
        break;
      }
      default:
        assert(0);
    }
    clear(&transfers->load_dst_regs, dst);
  }
}

static void
execute_transfers(struct CompilerContext* ctx,
                  struct StackTransferRecipe* transfers)
{
  // First, execute register moves. Then load constants and stack values into
  // registers.
  execute_moves(ctx, transfers);
  assert(is_empty(transfers->move_dst_regs));
  execute_loads(ctx, transfers);
  assert(is_empty(transfers->load_dst_regs));
}

__attribute__((unused)) static void
move_stack_value(struct CompilerContext* ctx, int dst_idx, int src_idx,
                 sgxwasm_valtype_t type)
{
  assert(dst_idx != src_idx);
  // XXX: Liftoff checks has_used_register.
  if (type == VALTYPE_I32 || type == VALTYPE_I64) {
    fill_from_stack(ctx, ScratchGP, src_idx, type);
    spill_to_stack(ctx, dst_idx, ScratchGP, 0, type);
  } else {
    assert(type == VALTYPE_F32 || type == VALTYPE_F64);
    fill_from_stack(ctx, ScratchFP, src_idx, type);
    spill_to_stack(ctx, dst_idx, ScratchFP, 0, type);
  }
#if MEMORY_TRACE
  struct ControlBlock* c = &control(ctx)->data[control_depth(control(ctx)) - 1];
  int offset;
  // Record Fill
  if (!mem_tracer_grow(mem_tracer(ctx)))
    assert(0);
  offset = src_idx * StackSlotSize * -1 - StackOffset;
  record_memory_access(mem_tracer(ctx), c->cont, c->type,
                       control_depth(control(ctx)), MEM_ACCESS_READ, MEM_STACK,
                       offset);
  // Record Spill
  if (!mem_tracer_grow(mem_tracer(ctx)))
    assert(0);
  offset = dst_idx * StackSlotSize * -1 - StackOffset;
  record_memory_access(mem_tracer(ctx), c->cont, c->type,
                       control_depth(control(ctx)), MEM_ACCESS_WRITE, MEM_STACK,
                       offset);
#endif
}

__attribute__((unused)) static void
transfer_stack_slot(struct CompilerContext* ctx,
                    struct StackTransferRecipe* transfers,
                    struct StackState* dst_stack, struct StackState* src_stack,
                    int dst_idx, int src_idx)
{
  struct StackSlot* dst = &dst_stack->data[dst_idx];
  struct StackSlot* src = &src_stack->data[src_idx];
  //#if SGXWASM_DEBUG_COMPILE
  if (dst->type != src->type) {
    printf("[transfer_stack_slot] dst\n");
    dump_stack_slot(dst_stack, dst_idx);
    printf("[transfer_stack_slot] src\n");
    dump_stack_slot(src_stack, src_idx);
  }
  //#endif
  assert(dst->type == src->type);
  switch (dst->loc) {
    case LOC_STACK: {
      switch (src->loc) {
        case LOC_STACK:
          if (src_idx == dst_idx)
            break;
          move_stack_value(ctx, dst_idx, src_idx, src->type);
          break;
        case LOC_REGISTER: {
          spill_to_stack(ctx, dst_idx, src->reg, 0, src->type);
#if 0 // Modify the current stack state accordingly.
          make_slot_stack(src, src->type);
#endif

#if MEMORY_TRACE
          struct ControlBlock* c =
            &control(ctx)->data[control_depth(control(ctx)) - 1];
          int offset = dst_idx * StackSlotSize * -1 - StackOffset;
          if (!mem_tracer_grow(mem_tracer(ctx)))
            assert(0);
          record_memory_access(mem_tracer(ctx), c->cont, c->type,
                               control_depth(control(ctx)), MEM_ACCESS_WRITE,
                               MEM_STACK, offset);
#endif
          break;
        }
        case LOC_CONST: {
          spill_to_stack(ctx, dst_idx, REG_UNKNOWN, src->i32_const, src->type);

#if MEMORY_TRACE
          struct ControlBlock* c =
            &control(ctx)->data[control_depth(control(ctx)) - 1];
          int offset = dst_idx * StackSlotSize * -1 - StackOffset;
          if (!mem_tracer_grow(mem_tracer(ctx)))
            assert(0);
          record_memory_access(mem_tracer(ctx), c->cont, c->type,
                               control_depth(control(ctx)), MEM_ACCESS_WRITE,
                               MEM_STACK, offset);
#endif
          break;
        }
        default:
          assert(0);
          break;
      }
      break;
    }
    case LOC_REGISTER:
      load_into_register(transfers, dst->reg, src, src_idx);
      break;
    case LOC_CONST: {
      /*printf("src const: %d, dst const: %d\n",
             src->i32_const, dst->i32_const);*/
      assert(src->i32_const == dst->i32_const);
      break;
    }
    default:
      assert(0);
      break;
  }
}

// End of reference implementation of StackTransferRecipe

#if __OPT_REUSE_REGISTER__
// Currently, we just try to reuse the ScratchGP2 (r11).
// In the future, we should extend to use the all the allocable
// registers, which will maximize the usage of registers.
static void
reset_register_use(struct CompilerContext* ctx)
{
  struct RegisterUse* map = ctx->register_reuse_map;
  for (int i = 0; i < RegisterNum; i++) {
    map[i].mem_type = MEMREF_NONE;
    map[i].offset = 0;
  }
}

static void
set_register_use(struct CompilerContext* ctx, sgxwasm_register_t reg,
                 sgxwasm_memref_t mem_type, size_t offset)
{
  struct RegisterUse* map = ctx->register_reuse_map;
  map[reg].mem_type = mem_type;
  map[reg].offset = offset;
}

static sgxwasm_register_t
find_register_use(struct CompilerContext* ctx, sgxwasm_memref_t mem_type,
                  size_t offset)
{
  struct RegisterUse* map = ctx->register_reuse_map;
  if (map[ScratchGP2].mem_type == mem_type &&
      map[ScratchGP2].offset == offset) {
    return ScratchGP2;
  }
  return REG_UNKNOWN;
#if 0
  for (int i = 0; i < RegisterNum; i++) {
    if (map[i].mem_type == mem_type && map[i].offset == offset) {
      return i;
    }
  }
  return REG_UNKNOWN;
#endif
}
#endif

static void
init_compiler_context(struct CompilerContext* ctx, const struct Function* func,
                      struct MemoryReferences* memrefs, struct PassManager* pm,
                      struct StackState* sstack, reglist_t* used_registers,
                      uint32_t* register_use_count,
                      reglist_t* last_spilled_regs,
                      const struct FuncType* fun_type, uint32_t num_locals,
                      const struct Memory* mem)
{
  ctx->func = func;
  ctx->memrefs = memrefs;
  ctx->pm = pm;
  ctx->output.capacity = 0;
  ctx->output.size = 0;
  ctx->output.data = NULL;
  ctx->control_stack.capacity = 0;
  ctx->control_stack.size = 0;
  ctx->control_stack.data = NULL;
  ctx->ool_list.capacity = 0;
  ctx->ool_list.size = 0;
  ctx->ool_list.data = NULL;
  ctx->cache_state.stack_state = sstack;
  ctx->cache_state.stack_base = 0;
  ctx->cache_state.used_registers = used_registers;
  ctx->cache_state.register_use_count = register_use_count;
  ctx->cache_state.last_spilled_regs = last_spilled_regs;
  ctx->compile_state.instr = NULL;
  ctx->compile_state.instr_id = 0;
  ctx->compile_state.instr_list = NULL;
  ctx->compile_state.n_instrs = 0;
  ctx->num_used_spill_slots = 0;
  memcpy(&ctx->sig, fun_type, sizeof(struct FuncType));
  ctx->num_locals = num_locals;
  if (mem != NULL) {
    ctx->min_memory_size = mem->size;
    ctx->max_memory_size = mem->max;
  } else {
    ctx->min_memory_size = 0;
    ctx->max_memory_size = 0;
  }
  ctx->num_low_instrs = 0;
#if __OPT_REUSE_REGISTER__
  reset_register_use(ctx);
#endif
#if MEMORY_TRACE
  mem_tracer_init(&ctx->mem_tracer);
#endif
}

static void
free_compiler_context(struct CompilerContext* ctx)
{
  int i = 0;
  for (i = 0; i < control(ctx)->size; i++) {
    struct ControlBlock* c = &control(ctx)->data[i];
    stack_free(c->label_state.stack_state);
    stack_free(c->else_state.state.stack_state);
  }
  control_free(control(ctx));
  ool_free(out_of_line_code(ctx));
}

static void
dump_memory_object(char* name, size_t sz, size_t cap, size_t data_sz,
                   size_t* total_sz, size_t* total_cap)
{
  sz *= data_sz;
  cap *= data_sz;
  printf("%s (%zu): %zu (%zu)\n", name, data_sz, sz, cap);
  *total_sz += sz;
  *total_cap += cap;
}

static void
dump_memory_usage(struct CompilerContext* ctx)
{
  size_t sz = 0, cap = 0;
  dump_memory_object("output", ctx->output.size, ctx->output.capacity,
                     sizeof(ctx->output.data[0]), &sz, &cap);
  dump_memory_object("control_stack", ctx->control_stack.size,
                     ctx->control_stack.capacity,
                     sizeof(ctx->control_stack.data[0]), &sz, &cap);
  dump_memory_object("cache_state.sstack", ctx->cache_state.stack_state->size,
                     ctx->cache_state.stack_state->capacity,
                     sizeof(ctx->cache_state.stack_state->data[0]), &sz, &cap);
  dump_memory_object("memrefs", ctx->memrefs->size, ctx->memrefs->capacity,
                     sizeof(ctx->memrefs->data[0]), &sz, &cap);
  dump_memory_object("total", sz, cap, 1, &sz, &cap);
}

// End of definition of CompilerContext

// Implementation of compile state.

static void
update_compile_state(struct CompilerContext* ctx)
{
  struct ControlBlock* c = control_at(control(ctx), 0);
  struct CompileState* state = compile_state(ctx);
  state->instr_list = c->instructions;
  state->n_instrs = c->n_instructions;
}

static const struct Instr*
get_next_instr(struct CompilerContext* ctx)
{
  struct ControlBlock* c = control_at(control(ctx), 0);
  struct CompileState* state = compile_state(ctx);
  size_t* count = &c->cont;
  if (*count >= state->n_instrs) {
    return NULL;
  }
  state->instr = &state->instr_list[*count];
  state->instr_id = *count;
  (*count)++;
  return state->instr;
}

// End of compile state.

__attribute__((unused)) static void
spill(struct CompilerContext* ctx, uint32_t index)
{
  uint32_t height = stack_height(cache_state(ctx));
  assert(height && index < height);
  struct StackSlot* slot = &cache_state(ctx)->stack_state->data[index];
  switch (slot->loc) {
    case LOC_STACK:
      return;
    case LOC_REGISTER: {
      spill_to_stack(ctx, index, slot->reg, 0, slot->type);
      dec_used(cache_state(ctx)->used_registers,
               cache_state(ctx)->register_use_count, slot->reg);
#if MEMORY_TRACE
      struct ControlBlock* c =
        &control(ctx)->data[control_depth(control(ctx)) - 1];
      int offset = index * StackSlotSize * -1 - StackOffset;
      if (!mem_tracer_grow(mem_tracer(ctx)))
        assert(0);
      record_memory_access(mem_tracer(ctx), c->cont, c->type,
                           control_depth(control(ctx)), MEM_ACCESS_WRITE,
                           MEM_STACK, offset);
#endif
      break;
    }
    case LOC_CONST: {
      spill_to_stack(ctx, index, REG_UNKNOWN, (int64_t)slot->i32_const,
                     slot->type);
#if MEMORY_TRACE
      struct ControlBlock* c =
        &control(ctx)->data[control_depth(control(ctx)) - 1];
      int offset = index * StackSlotSize * -1 - StackOffset;
      if (!mem_tracer_grow(mem_tracer(ctx)))
        assert(0);
      record_memory_access(mem_tracer(ctx), c->cont, c->type,
                           control_depth(control(ctx)), MEM_ACCESS_WRITE,
                           MEM_STACK, offset);
#endif
      break;
    }
    default:
      break;
  }
  slot->loc = LOC_STACK;
}

void
spill_register(struct CompilerContext* ctx, sgxwasm_register_t reg)
{
  uint32_t remaining_uses =
    get_use_count(cache_state(ctx)->register_use_count, reg);
  if (remaining_uses == 0) {
    return;
  }
  assert(remaining_uses != 0);
#if MEMORY_TRACE
  size_t cont;
  control_type_t type;
  int offset;
  if (control_depth(control(ctx)) == 0) { // Case of function returning.
    cont = -1;
    type = CONTROL_BLOCK;
  } else {
    struct ControlBlock* c =
      &control(ctx)->data[control_depth(control(ctx)) - 1];
    cont = c->cont;
    type = c->type;
  }
#endif
  int idx;
  for (idx = (int)stack_height(cache_state(ctx)) - 1; idx >= 0; idx--) {
    struct StackSlot* slot = &cache_state(ctx)->stack_state->data[idx];
    if (!is_reg(slot->loc) || slot->reg != reg)
      continue;
    spill_to_stack(ctx, idx, slot->reg, 0, slot->type);
#if MEMORY_TRACE
    offset = idx * StackSlotSize * -1 - StackOffset;
    if (!mem_tracer_grow(mem_tracer(ctx)))
      assert(0);
    record_memory_access(mem_tracer(ctx), cont, type,
                         control_depth(control(ctx)), MEM_ACCESS_WRITE,
                         MEM_STACK, offset);
#endif
    slot->loc = LOC_STACK;
    if (--remaining_uses == 0)
      break;
  }
  clear_used(cache_state(ctx)->used_registers,
             cache_state(ctx)->register_use_count, reg);
}

static sgxwasm_register_t
spill_one_register(struct CompilerContext* ctx, reglist_t candidates,
                   reglist_t pinned)
{
  // Spill one cached value to free a register.
  sgxwasm_register_t spill_reg =
    get_next_spill_reg(*(cache_state(ctx)->used_registers),
                       cache_state(ctx)->last_spilled_regs, candidates, pinned);
  spill_register(ctx, spill_reg);
  return spill_reg;
}

static void
spill_locals(struct CompilerContext* ctx)
{
  int idx;
  for (idx = 0; idx < (int)num_locals(ctx); idx++) {
    spill(ctx, idx);
  }
}

static void
push_stack_slot(struct CompilerContext* ctx, struct StackSlot* slot,
                uint32_t index, sgxwasm_valtype_t type)
{
  struct Operand src;
  int32_t offset;

  offset = index * StackSlotSize * -1 - StackOffset;
  build_operand(&src, GP_RBP, REG_UNKNOWN, SCALE_NONE, offset);

  switch (slot->loc) {
    case LOC_STACK: {
      if (type == VALTYPE_I32) {
        // Load i32 values to a register first to ensure they are zero
        // extended.
        num_low_instrs(ctx) +=
          emit_mov_rm(output(ctx), ScratchGP, &src, VALTYPE_I32);
        num_low_instrs(ctx) += emit_pushq_r(output(ctx), ScratchGP);
      } else {
        // XXX: Check F32
        num_low_instrs(ctx) += emit_pushq_m(output(ctx), &src);
      }
      break;
    }
    case LOC_REGISTER: {
      num_low_instrs(ctx) += Push(output(ctx), slot->reg, slot->type);
      break;
    }
    case LOC_CONST: {
      num_low_instrs(ctx) += emit_pushq_i(output(ctx), slot->i32_const);
      break;
    }
    default:
      assert(0);
  }
}

static sgxwasm_register_t
get_unused_register(struct CompilerContext* ctx, reglist_t candidates,
                    reglist_t pinned)
{
  sgxwasm_register_t reg;
  if (has_unused_register(candidates, *(cache_state(ctx)->used_registers),
                          pinned)) {
    reg =
      unused_register(candidates, *(cache_state(ctx)->used_registers), pinned);
    return reg;
  }

  reg = spill_one_register(ctx, candidates, pinned);
  return reg;
}

static sgxwasm_register_t
get_unused_register_with_class(struct CompilerContext* ctx,
                               sgxwasm_register_class_t rc, reglist_t try_first,
                               reglist_t pinned)
{
  while (!is_empty(try_first)) {
    sgxwasm_register_t reg = get_first_reg_set(try_first);
    if (is_free(*(cache_state(ctx)->used_registers),
                cache_state(ctx)->register_use_count, reg)) {
      return reg;
    }
    clear(&try_first, reg);
  }
  reglist_t candidates = get_cache_reg_list(rc);
  // return get_unused_register(ctx, candidates, pinned);
  sgxwasm_register_t rv = get_unused_register(ctx, candidates, pinned);
#if SGXWASM_DEBUG_COMPILE
  printf("[Register allocation]: %u, used_registers: 0x%x\n", rv,
         *(cache_state(ctx)->used_registers));
#endif
  return rv;
}

__attribute__((unused)) static sgxwasm_register_t
pop_to_register(struct CompilerContext* ctx, reglist_t pinned)
{
  size_t height = stack_height(cache_state(ctx));
  assert(height);
  struct StackSlot slot;
  memcpy(&slot, &cache_state(ctx)->stack_state->data[height - 1],
         sizeof(struct StackSlot));
  stack_shrink(cache_state(ctx)->stack_state);
  switch (slot.loc) {
    case LOC_STACK: {
      sgxwasm_register_t reg = get_unused_register_with_class(
        ctx, reg_class_for(slot.type), EmptyRegList, pinned);
      size_t index = stack_height(cache_state(ctx));
      fill_from_stack(ctx, reg, index, slot.type);
#if MEMORY_TRACE
      struct ControlBlock* c =
        &control(ctx)->data[control_depth(control(ctx)) - 1];
      int offset = index * StackSlotSize * -1 - StackOffset;
      if (!mem_tracer_grow(mem_tracer(ctx)))
        assert(0);
      record_memory_access(mem_tracer(ctx), c->cont, c->type,
                           control_depth(control(ctx)), MEM_ACCESS_READ,
                           MEM_STACK, offset);
#endif
      return reg;
    }
    case LOC_REGISTER:
      dec_used(cache_state(ctx)->used_registers,
               cache_state(ctx)->register_use_count, slot.reg);
      return slot.reg;
    case LOC_CONST: {
      sgxwasm_register_t reg = get_unused_register_with_class(
        ctx, reg_class_for(slot.type), EmptyRegList, pinned);
      // XXX: Check if we need to encode floating point numbers.
      load_const_to_reg(ctx, reg, slot.i32_const, slot.type);
      return reg;
      break;
    }
    default:
      return REG_UNKNOWN;
  }

  return REG_UNKNOWN;
}

struct RegisterReuseMap
{
  size_t capacity;
  size_t size;
  sgxwasm_register_t* data;
};
static DEFINE_VECTOR_RESIZE(reusemap, struct RegisterReuseMap);
static DEFINE_VECTOR_FREE(reusemap, struct RegisterReuseMap);

static sgxwasm_register_t
reuse_map_loop_up(struct RegisterReuseMap* map, sgxwasm_register_t src)
{
  size_t i;
  for (i = 0; i < map->size; i += 2) {
    if (map->data[i] == src) {
      return map->data[i + 1];
    }
  }
  return REG_UNKNOWN;
}

static void
add_pair(struct RegisterReuseMap* map, sgxwasm_register_t src,
         sgxwasm_register_t dst)
{
  assert(src != REG_UNKNOWN && dst != REG_UNKNOWN);
  sgxwasm_register_t previous = reuse_map_loop_up(map, src);
  if (previous == dst) {
    return;
  }

  if (!reusemap_resize(map, map->size + 2)) {
    assert(0);
  }
  map->data[map->size - 2] = src;
  map->data[map->size - 1] = dst;
}

__attribute__((unused)) static void
init_merge_region(struct CacheState* dst_state, uint32_t dst_begin,
                  struct CacheState* src_state, uint32_t src_begin,
                  uint32_t count, uint8_t keep_stack_slots,
                  uint8_t allow_constants, uint8_t reuse_registers,
                  reglist_t used_reg)
{
  struct RegisterReuseMap register_reuse_map = { 0, 0, NULL };
  uint32_t src_idx = src_begin;
  uint32_t src_end = src_begin + count;
  uint32_t dst_idx = dst_begin;
  for (; src_idx < src_end; ++src_idx, ++dst_idx) {
    struct StackSlot* dst = &dst_state->stack_state->data[dst_idx];
    struct StackSlot* src = &src_state->stack_state->data[src_idx];
    if ((is_stack(src->loc) && keep_stack_slots) ||
        (is_const(src->loc) && allow_constants)) {
      memcpy(dst, src, sizeof(struct StackSlot));
      continue;
    }

    sgxwasm_register_t reg = REG_UNKNOWN;
    // First try: Keep the same register, if it's free.
    if (is_reg(src->loc) && is_free(*(dst_state->used_registers),
                                    dst_state->register_use_count, src->reg)) {
#if SGXWASM_DEBUG_COMPILE
      printf("index: %u, reg: %u <- dst_reg: %u\n", src_idx, reg, src->reg);
#endif
      reg = src->reg;
    }
    // Second try: Use the same register we used before (if we reuse registers).
    if (reg == REG_UNKNOWN && reuse_registers) {
      reg = reuse_map_loop_up(&register_reuse_map, src->reg);
    }
    // Third try: Use any free register.
    sgxwasm_register_class_t rc = reg_class_for(src->type);
    if (reg == REG_UNKNOWN && has_unused_register_with_class(
                                *(dst_state->used_registers), rc, used_reg)) {
      reg =
        unused_register_with_class(*(dst_state->used_registers), rc, used_reg);
    }
    if (reg == REG_UNKNOWN) {
      // No free register; make this a stack slot.
      make_slot_stack(dst, src->type);
      continue;
    }
    if (reuse_registers) {
      add_pair(&register_reuse_map, src->reg, reg);
    }
    inc_used(dst_state->used_registers, dst_state->register_use_count, reg);
    make_slot_register(dst, src->type, reg);
  }
  // printf("reusemap: size - %zu (cap: %zu)\n", register_reuse_map.size,
  // register_reuse_map.capacity);
  reusemap_free(&register_reuse_map);
  /*if (register_reuse_map.size > 0 && register_reuse_map.data) {
    printf("reusemap free\n");
    free(register_reuse_map.data);
  }*/
}

__attribute__((unused)) static void
init_merge(struct CacheState* dst_state, struct CacheState* src_state,
           uint32_t num_locals, uint32_t arity)
{
  // |------locals------|---(in between)----|--(discarded)--|----merge----|
  //  <-- num_locals --> <-- stack_depth -->^stack_base      <-- arity -->

  uint32_t target_height = dst_state->stack_base + arity;
  uint32_t discarded = stack_height(src_state) - target_height;
  assert(stack_height(dst_state) == 0);
  assert(stack_height(src_state) >= dst_state->stack_base);
#if SGXWASM_DEBUG_MEMORY
  printf("[init_merge] stack_resize - old: %zu, new: %zu\n",
         dst_state->stack_state->size, target_height);
#endif
  if (!stack_resize(dst_state->stack_state, target_height))
    assert(0); // Should not go here.

#if SGXWASM_DEBUG_COMPILE
  printf("[init_merge] init dst->used_registers: 0x%x\n",
         *(dst_state->used_registers));
#endif

  reglist_t used_regs = get_allocable_reg_list();
  uint32_t i;
  uint32_t src_begin = 0;
  uint32_t dst_begin = 0;
  uint32_t offset;

  // Try to keep locals and the merge region in their registers. Register used
  // multiple times need to be copied to another free register. Compute the list
  // of used registers.
  offset = src_begin;
  for (i = 0; i < num_locals; i++) {
    struct StackSlot* src = &src_state->stack_state->data[offset + i];
    if (is_reg(src->loc)) {
      set(&used_regs, src->reg);
    }
  }
  // NOTE: These two calculation of src_idx are the same.
  // src_idx = stack_height(src_state) - arity;
  offset = src_begin + dst_state->stack_base + discarded;
  for (i = 0; i < arity; i++) {
    struct StackSlot* src = &src_state->stack_state->data[offset + i];
    if (is_reg(src->loc)) {
      set(&used_regs, src->reg);
    }
  }

  // Initialize the merge region. If this region moves, try to turn stack slots
  // into registers since we need to load the value anyways.
  uint8_t keep_merge_stack_slots = discarded == 0 ? 1 : 0;
  init_merge_region(dst_state, dst_begin + dst_state->stack_base, src_state,
                    src_begin + dst_state->stack_base + discarded, arity,
                    keep_merge_stack_slots, 0 /* False */, 0 /* False */,
                    used_regs);

  // Initialize the locals region. Here, stack slots stay stack slots (because
  // they do not move). Try to keep register in registers, but avoid duplicates.
  init_merge_region(dst_state, dst_begin, src_state, src_begin, num_locals,
                    1 /* keep_stack_slots = True */,
                    0 /* allow_constants = False */,
                    0 /* reuse_registers = False */, used_regs);

#if SGXWASM_DEBUG_COMPILE
  printf("used_regs: 0x%x, dst->used_registers: 0x%x\n", used_regs,
         *(dst_state->used_registers));
#endif
  // Sanity check: All the {used_regs} are really in use now.
  assert(used_regs == (*(dst_state->used_registers) & used_regs));

  // Last, initialize the section in between. Here, constants are allowed, but
  // registers which are already used for the merge region or locals must be
  // moved to other registers or spilled. If a register appears twice in the
  // source region, ensure to use the same register twice in the target region.
  init_merge_region(
    dst_state, dst_begin + num_locals, src_state, src_begin + num_locals,
    dst_state->stack_base - num_locals, 1 /* keep_stack_slots = True */,
    1 /* allow_constants = True */, 1 /* reuse_registers = True */, used_regs);
}

__attribute__((unused)) static void
merge_full_stack_with(struct CompilerContext* ctx, struct CacheState* dst_state,
                      struct CacheState* src_state)
{
#if SGXWASM_DEBUG_COMPILE
  printf(
    "[merge_full_stack_with] dst stack height: %lu, src stack height: %lu\n",
    stack_height(dst_state), stack_height(src_state));
#endif
  assert(stack_height(dst_state) == stack_height(src_state));
  int i;
  struct StackTransferRecipe transfers;
  init_stack_transfer_receipe(&transfers);
  for (i = 0; i < (int)stack_height(src_state); i++) {
    transfer_stack_slot(ctx, &transfers, dst_state->stack_state,
                        src_state->stack_state, i, i);
  }
  execute_transfers(ctx, &transfers);
}

__attribute__((unused)) static void
merge_stack_with(struct CompilerContext* ctx, struct CacheState* dst_state,
                 struct CacheState* src_state, uint32_t arity)
{
  uint32_t dst_stack_height = stack_height(dst_state);
  uint32_t src_stack_height = stack_height(src_state);
  assert(dst_stack_height <= src_stack_height);
  assert(arity <= dst_stack_height);
  int dst_stack_base = dst_stack_height - arity;
  int src_stack_base = src_stack_height - arity;
  int i;
  struct StackTransferRecipe transfers;
  init_stack_transfer_receipe(&transfers);
  for (i = 0; i < dst_stack_base; i++) {
    transfer_stack_slot(ctx, &transfers, dst_state->stack_state,
                        src_state->stack_state, i, i);
  }
  for (i = 0; i < (int)arity; i++) {
    transfer_stack_slot(ctx, &transfers, dst_state->stack_state,
                        src_state->stack_state, dst_stack_base + i,
                        src_stack_base + i);
  }
  execute_transfers(ctx, &transfers);
}

struct MemoryRef*
new_memref(struct MemoryReferences* memrefs)
{
  assert(memrefs != NULL);
  if (!memrefs_grow(memrefs))
    assert(0);
  struct MemoryRef* memref = &memrefs->data[memrefs->size - 1];
  memset(memref, 0, sizeof(struct MemoryRef));
  return memref;
}

static void
load_from_memory(struct CompilerContext* ctx, sgxwasm_register_t dst,
                 size_t mem_type, size_t index)
{
  assert(ctx->memrefs != NULL);
  struct MemoryReferences* memrefs = ctx->memrefs;
  struct MemoryRef* memref = new_memref(memrefs);
  // Patch the value later.
  num_low_instrs(ctx) += emit_movq_ri(output(ctx), dst, 0xffffffffffffffff);

  memref->type = mem_type;
  memref->code_offset = output(ctx)->size - 8;
  memref->idx = index;

#if __OPT_REUSE_REGISTER__
  if (dst == ScratchGP2) {
    set_register_use(ctx, dst, mem_type, index);
  }
#endif
}

static void
free_control_block(struct ControlBlock* c)
{
  assert(c != NULL);

  stack_free(c->label_state.stack_state);

  if (c->label_state.stack_state) {
    stack_free(c->label_state.stack_state);
    free(c->label_state.stack_state);
  }
  if (c->label_state.used_registers) {
    free(c->label_state.used_registers);
  }
  if (c->label_state.register_use_count) {
    free(c->label_state.register_use_count);
  }
  if (c->label_state.last_spilled_regs) {
    free(c->label_state.last_spilled_regs);
  }
  if (c->else_state.state.stack_state) {
    stack_free(c->else_state.state.stack_state);
    free(c->else_state.state.stack_state);
  }
  if (c->else_state.state.used_registers) {
    free(c->else_state.state.used_registers);
  }
  if (c->else_state.state.register_use_count) {
    free(c->else_state.state.register_use_count);
  }
  if (c->else_state.state.last_spilled_regs) {
    free(c->else_state.state.last_spilled_regs);
  }
}

static struct ControlBlock*
push_control(struct Control* control_stack, control_type_t type,
             uint32_t in_arity, uint32_t out_arity)
{
  reachability_type_t reachability;
  if (control_depth(control_stack) == 0) {
    reachability = Reachable;
  } else {
    struct ControlBlock* parent = &control_stack->data[control_stack->size - 1];
    reachability = inner_reachability(parent);
  }

  if (!control_grow(control_stack))
    return NULL;

  struct ControlBlock* c = &control_stack->data[control_stack->size - 1];
  c->type = type;
  c->in_arity = in_arity;
  c->out_arity = out_arity;
  // Initialization
  c->reachability = reachability;
  c->start_reached = (reachability == Reachable) ? 1 : 0;
  c->end_reached = 0;
  c->label.pos = 0;
  c->label.near_link_pos = 0;

  c->label_state.stack_state = malloc(sizeof(struct StackState));
  stack_init(c->label_state.stack_state);
  c->label_state.used_registers = malloc(sizeof(reglist_t));
  assert(c->label_state.used_registers);
  *(c->label_state.used_registers) = get_allocable_reg_list();
  c->label_state.register_use_count = malloc(RegisterNum * sizeof(uint32_t));
  assert(c->label_state.register_use_count);
  memset(c->label_state.register_use_count, 0, RegisterNum * sizeof(uint32_t));
  c->label_state.last_spilled_regs = malloc(sizeof(reglist_t));
  assert(c->label_state.last_spilled_regs);
  memset(c->label_state.last_spilled_regs, 0, sizeof(reglist_t));

  c->else_state.label.pos = 0;
  c->else_state.label.near_link_pos = 0;
  c->else_state.state.stack_state = NULL;
  c->else_state.state.used_registers = NULL;
  c->else_state.state.register_use_count = NULL;
  c->else_state.state.last_spilled_regs = NULL;
  return c;
}

static void
finish_one_armed_if(struct CompilerContext* ctx, struct ControlBlock* c)
{
  assert(is_onearmed_if(c->type));
  if (c->end_reached) {
    // Someone already merged to the end of the if. Merge both arms into that.
    if (reachable(c)) {
      // Merge the if state into the end state.
      merge_full_stack_with(ctx, &c->label_state, cache_state(ctx));
      uncond_jmp(ctx, &c->label, 0, Far);
    }
  } else if (reachable(c)) {
    // No merge yet at the end of the if, but we need to create a merge for
    // the both arms of this if. Thus init the merge point from the else
    // state, then merge the if state into that.

    // XXX: Check why the out_arity is 0 in the case.
    assert(c->out_arity == 0);
    init_merge(&c->label_state, &c->else_state.state, num_locals(ctx), 0);
    merge_full_stack_with(ctx, &c->label_state, cache_state(ctx));
    uncond_jmp(ctx, &c->label, 0, Far);
  } else {
    // No merge needed, just continue with the else state.
  }
}

__attribute__((unused)) static void
post_finish_one_armed_if(struct CompilerContext* ctx, struct ControlBlock* c)
{
  assert(is_onearmed_if(c->type));
  if (c->end_reached) {
    // Merge the else state into the end state.
    // Workaround: Allow instrumentation to overwrite label, so no need for
    //             binding as relocation will handle it.
    low_bind_label(ctx, &c->else_state.label);
    merge_full_stack_with(ctx, &c->label_state, &c->else_state.state);
    steal_state(cache_state(ctx), &c->label_state);
  } else if (reachable(c)) {
    // XXX: Check why the out_arity is 0 in the case.
    assert(c->out_arity == 0);
    // Merge the else state into the end state.
    low_bind_label(ctx, &c->else_state.label);
    merge_full_stack_with(ctx, &c->label_state, &c->else_state.state);
    steal_state(cache_state(ctx), &c->label_state);
  } else {
    // No merge needed, just continue with the else state.
    low_bind_label(ctx, &c->else_state.label);
    steal_state(cache_state(ctx), &c->else_state.state);
  }
}

static void
pop_control(struct CompilerContext* ctx, struct ControlBlock* c)
{
  assert(&control(ctx)->data[control_depth(control(ctx)) - 1] == c);

  if (is_loop(c->type)) {
    // A loop just falls through.
    return;
  }

  if (is_onearmed_if(c->type)) {
    // Special handling for one-armed ifs.
    finish_one_armed_if(ctx, c);
  } else if (c->end_reached) {
    // There is a merge already. Merge our state into that, then continue with
    // that state.
    if (reachable(c)) {
      merge_full_stack_with(ctx, &c->label_state, cache_state(ctx));
    }
    steal_state(cache_state(ctx), &c->label_state);
  } else {
    // No merge, just continue with our current state.
  }
}

__attribute__((unused)) static void
fall_thru_to(struct CompilerContext* ctx, struct ControlBlock* c)
{
  assert(&control(ctx)->data[control_depth(control(ctx)) - 1] == c);
  if (!is_loop(c->type)) {
    if (c->end_reached) {
      merge_full_stack_with(ctx, &c->label_state, cache_state(ctx));
    } else {
      split_state(&c->label_state, cache_state(ctx));
    }
  }
  c->end_reached = 1;
}

static void
return_impl(struct CompilerContext* ctx)
{
  assert(stack_height(cache_state(ctx)) >= return_count(&ctx->sig));
  assert(return_count(&ctx->sig) <= 1);
  if (return_count(&ctx->sig) > 0) {
    struct StackTransferRecipe transfers;
    init_stack_transfer_receipe(&transfers);
    sgxwasm_register_t return_reg;
    if (reg_class_for(return_type(&ctx->sig)) == GP_REG) {
      return_reg = GPReturnRegister;
    } else {
      // XXX: Check for the case for returning both XMM0 & XMM1
      return_reg = FPReturnRegister0;
    }
    size_t top_idx = stack_height(cache_state(ctx)) - 1;
    struct StackSlot* top_slot = &cache_state(ctx)->stack_state->data[top_idx];
    load_into_register(&transfers, return_reg, top_slot, top_idx);
    // Execute the transfers.
    execute_transfers(ctx, &transfers);
  }
  // Leave frame.
  num_low_instrs(ctx) += Move(output(ctx), GP_RSP, GP_RBP, VALTYPE_I64);
  num_low_instrs(ctx) += emit_popq_r(output(ctx), GP_RBP);

  // XXX: We assume the return without popping additional bytes for now.
  // emit_ret(output(ctx), 0);
  low_return(ctx);
}

static void
br_impl(struct CompilerContext* ctx, struct ControlBlock* target,
        uint32_t depth)
{
  uint8_t target_reached =
    is_loop(target->type) ? target->start_reached : target->end_reached;
  uint32_t target_arity =
    is_loop(target->type) ? target->in_arity : target->out_arity;
  if (!target_reached) {
    init_merge(&target->label_state, cache_state(ctx), num_locals(ctx),
               target_arity);
  }
  merge_stack_with(ctx, &target->label_state, cache_state(ctx), target_arity);
  uncond_jmp(ctx, &target->label, depth, Far);
}

static void
br_or_ret(struct CompilerContext* ctx, uint32_t depth)
{
  if (depth == (uint32_t)(control_depth(control(ctx)) - 1)) {
    // Do return.
    return_impl(ctx);
  } else {
    struct ControlBlock* target = control_at(control(ctx), depth);
    br_impl(ctx, target, depth);
  }
}

__attribute__((unused)) static void
generate_br_case(struct CompilerContext* ctx, uint32_t br_depth,
                 label_t* br_targets)
{
  label_t* label = &br_targets[br_depth];
  if (is_bound(label)) {
    br_table_uncond_jmp(ctx, label, br_depth);
  } else {
    br_table_bind(ctx, label, BrCaseTarget, 0, 0, br_depth);
    br_or_ret(ctx, br_depth);
  }
}

__attribute__((unused)) static void
generate_br_table(struct CompilerContext* ctx, sgxwasm_register_t tmp,
                  sgxwasm_register_t value, uint32_t min, uint32_t max,
                  uint32_t* table, uint32_t* iterator, uint32_t table_count,
                  label_t* br_targets)
{
  assert(min < max);
  // Check base case.
  if (max == min + 1) {
    assert(min == *iterator);
    generate_br_case(ctx, table[*iterator], br_targets);
    *iterator += 1;
    return;
  }
  uint32_t split = min + (max - min) / 2;
  label_t upper_half = { 0, 0 };
  load_const_to_reg(ctx, tmp, split, VALTYPE_I32);
  br_table_cond_jmp(ctx, COND_GE_U, &upper_half, VALTYPE_I32, value, tmp, min,
                    max);

  // Emit br table for lower half.
  generate_br_table(ctx, tmp, value, min, split, table, iterator, table_count,
                    br_targets);

  // Use min, max pair as the identifier for branch target.
  br_table_bind(ctx, &upper_half, BrTableTarget, min, max, 0);

  // Emit br table for upper half.
  generate_br_table(ctx, tmp, value, split, max, table, iterator, table_count,
                    br_targets);
}

static void
set_local_from_stack_slot(struct CompilerContext* ctx,
                          struct StackSlot* dst_slot)
{
  sgxwasm_valtype_t type = dst_slot->type;
  if (is_reg(dst_slot->loc)) {
    sgxwasm_register_t slot_reg = dst_slot->reg;
    if (get_use_count(cache_state(ctx)->register_use_count, slot_reg) == 1) {
      size_t index = stack_height(cache_state(ctx)) - 1;
      fill_from_stack(ctx, slot_reg, index, type);
#if MEMORY_TRACE
      struct ControlBlock* c =
        &control(ctx)->data[control_depth(control(ctx)) - 1];
      int offset = index * StackSlotSize * -1 - StackOffset;
      if (!mem_tracer_grow(mem_tracer(ctx)))
        assert(0);
      record_memory_access(mem_tracer(ctx), c->cont, c->type,
                           control_depth(control(ctx)), MEM_ACCESS_READ,
                           MEM_STACK, offset);
#endif
      return;
    }
    dec_used(cache_state(ctx)->used_registers,
             cache_state(ctx)->register_use_count, slot_reg);
    make_slot_stack(dst_slot, type);
  }
  // XXX: DCHECK_EQ(type, __ local_type(local_index));
  //(void)local_index;
  sgxwasm_register_class_t rc = reg_class_for(type);
  sgxwasm_register_t dst_reg =
    get_unused_register_with_class(ctx, rc, EmptyRegList, EmptyRegList);
  size_t index = stack_height(cache_state(ctx)) - 1;
  fill_from_stack(ctx, dst_reg, index, type);
  make_slot_register(dst_slot, type, dst_reg);
  inc_used(cache_state(ctx)->used_registers,
           cache_state(ctx)->register_use_count, dst_reg);
#if MEMORY_TRACE
  struct ControlBlock* c = &control(ctx)->data[control_depth(control(ctx)) - 1];
  int offset = index * StackSlotSize * -1 - StackOffset;
  if (!mem_tracer_grow(mem_tracer(ctx)))
    assert(0);
  record_memory_access(mem_tracer(ctx), c->cont, c->type,
                       control_depth(control(ctx)), MEM_ACCESS_READ, MEM_STACK,
                       offset);
#endif
}

static void
set_local_impl(struct CompilerContext* ctx, size_t local_index, uint8_t is_tee)
{
  uint32_t back_index = stack_height(cache_state(ctx)) - 1;
  struct StackSlot* src_slot = &cache_state(ctx)->stack_state->data[back_index];
  struct StackSlot* dst_slot =
    &cache_state(ctx)->stack_state->data[local_index];

  switch (src_slot->loc) {
    case LOC_REGISTER: {
      if (is_reg(dst_slot->loc)) {
        dec_used(cache_state(ctx)->used_registers,
                 cache_state(ctx)->register_use_count, dst_slot->reg);
      }
      memcpy(dst_slot, src_slot, sizeof(struct StackSlot));
      if (is_tee) {
        inc_used(cache_state(ctx)->used_registers,
                 cache_state(ctx)->register_use_count, dst_slot->reg);
      }
      break;
    }
    case LOC_CONST: {
      if (is_reg(dst_slot->loc)) {
        dec_used(cache_state(ctx)->used_registers,
                 cache_state(ctx)->register_use_count, dst_slot->reg);
      }
      memcpy(dst_slot, src_slot, sizeof(struct StackSlot));
      break;
    }
    case LOC_STACK:
      set_local_from_stack_slot(ctx, dst_slot);
      break;
    default:
      assert(0);
      break;
  }
  if (!is_tee) {
    pop_stack(cache_state(ctx));
  }
}

void
get_local(struct CompilerContext* ctx, size_t index)
{
  struct StackSlot* slot = &cache_state(ctx)->stack_state->data[index];
  // assert(type->input_types[index] == slot->type);

  switch (slot->loc) {
    case LOC_REGISTER:
      push_register(ctx, slot->type, slot->reg);
      break;
    case LOC_CONST:
      push_const(ctx, slot->type, slot->i32_const);
      break;
    case LOC_STACK: {
      sgxwasm_register_class_t rc = reg_class_for(slot->type);
      sgxwasm_register_t reg =
        get_unused_register_with_class(ctx, rc, EmptyRegList, EmptyRegList);
      fill_from_stack(ctx, reg, index, slot->type);
      push_register(ctx, slot->type, reg);
#if MEMORY_TRACE
      struct ControlBlock* c =
        &control(ctx)->data[control_depth(control(ctx)) - 1];
      int offset = index * StackSlotSize * -1 - StackOffset;
      if (!mem_tracer_grow(mem_tracer(ctx)))
        assert(0);
      record_memory_access(mem_tracer(ctx), c->cont, c->type,
                           control_depth(control(ctx)), MEM_ACCESS_READ,
                           MEM_STACK, offset);
#endif
      break;
    }
    default:
      assert(0);
      break;
  }
}

__attribute__((unused)) static void
drop(struct CompilerContext* ctx)
{
  uint32_t stack_size = stack_height(cache_state(ctx));
  struct StackSlot* slot =
    &(cache_state(ctx)->stack_state->data[stack_size - 1]);
  // If the dropped slot contains a register, decrement it's use count.
  if (is_reg(slot->loc)) {
    dec_used(cache_state(ctx)->used_registers,
             cache_state(ctx)->register_use_count, slot->reg);
  }
  pop_stack(cache_state(ctx));
}

static sgxwasm_register_t
get_global_addr(struct CompilerContext* ctx, size_t global_index,
                reglist_t* pinned, size_t* offset)
{
#if __OPT_REUSE_REGISTER__
  sgxwasm_register_t addr = find_register_use(ctx, MEMREF_GLOBAL, global_index);
  if (addr == REG_UNKNOWN) {
    load_from_memory(ctx, ScratchGP2, MEMREF_GLOBAL, global_index);
    addr = ScratchGP2;
  }
#else
  sgxwasm_register_t addr =
    get_unused_register_with_class(ctx, GP_REG, EmptyRegList, EmptyRegList);

  // NOTE: Liftoff handles the case of global variables that are both
  // imported and mutable. However, current spec enures all the imported
  // global variables must be immutable.
  load_from_memory(ctx, addr, MEMREF_GLOBAL, global_index);
  set(pinned, addr);
#endif

  // We currently directly patch the value, offset
  // is therefore not needed.
  //*offset = global_index;
  *offset = 0;

  return addr;
}

static void
end_control(struct ControlBlock* current)
{
  current->reachability = Unreachable;
}

__attribute__((unused)) static void
prepare_call(struct CompilerContext* ctx, const struct FuncType* fun_type,
             sgxwasm_register_t* target)
{
  uint8_t num_params = parameter_count(fun_type);
  uint32_t idx, end;
  struct StackTransferRecipe transfers;
  reglist_t param_regs = 0;

  init_stack_transfer_receipe(&transfers);

#if MEMORY_TRACE
  struct ControlBlock* c = &control(ctx)->data[control_depth(control(ctx)) - 1];
  int offset;
#endif

  // Spill all cache slots which are not being used as parameters.
  // Don't update any register use counters, they will be reset later anyway.
  for (idx = 0, end = stack_height(cache_state(ctx)) - num_params; idx < end;
       ++idx) {
    struct StackSlot* slot = &cache_state(ctx)->stack_state->data[idx];
    if (!is_reg(slot->loc)) {
      continue;
    }
    spill_to_stack(ctx, idx, slot->reg, 0, slot->type);
    make_slot_stack(slot, slot->type);
#if MEMORY_TRACE
    offset = idx * StackSlotSize * -1 - StackOffset;
    if (!mem_tracer_grow(mem_tracer(ctx)))
      assert(0);
    record_memory_access(mem_tracer(ctx), c->cont, c->type,
                         control_depth(control(ctx)), MEM_ACCESS_WRITE,
                         MEM_STACK, offset);
#endif
  }

  struct ParameterAllocator params;
  struct LocationSignature fun_sig = { 0, 0, NULL };
  uint32_t param_base = stack_height(cache_state(ctx)) - num_params;
  uint32_t i;

  init_parameter_allocator(&params);
  // Build the function signature.
  for (i = 0; i < num_params; i++) {
    sgxwasm_valtype_t type = parameter_types(fun_type)[i];
    sgxwasm_register_class_t rc = reg_class_for(type);

    if (rc == GP_REG && can_allocate_gp(&params)) {
      sgxwasm_register_t reg = next_gp_reg(&params);
      add_param_at(&fun_sig, i, GP_REG, LOC_REGISTER, reg);
    } else if (rc == FP_REG && can_allocate_fp(&params)) {
      sgxwasm_register_t reg = next_fp_reg(&params);
      add_param_at(&fun_sig, i, FP_REG, LOC_REGISTER, reg);
    } else {
      uint32_t stack_offset = next_stack_slot(&params);
      add_param_at(&fun_sig, i, rc, LOC_STACK, stack_offset);
    }
  }

#if SGXWASM_DEBUG_COMPILE
// dump_cache_state(cache_state(ctx));
#endif

  // Stack alignment bug: If the number of parameters is larger than
  // 6 and odd, this will cause odd number of extra stack pushes and
  // break the stack alignment (16-bit alignment).
  // A simple fix here is to push more to stack.
  if (num_params > 6 && (num_params % 2 == 1)) {
    // Stack padding.
    num_low_instrs(ctx) += emit_pushq_r(output(ctx), GP_RAX);
  }

  // Process parameters backwards, such that pushes of caller frame slots are
  // in the correct order.
  for (i = num_params; i > 0; --i) {
    uint32_t index = i - 1;
    struct LinkageLocation* param = get_param(&fun_sig, index);
    sgxwasm_valtype_t type = parameter_types(fun_type)[index];
    uint32_t stack_idx = param_base + index;
    struct StackSlot* slot = &cache_state(ctx)->stack_state->data[stack_idx];
    sgxwasm_register_class_t rc = reg_class_for(type);
    if (is_reg(param->loc)) {
      sgxwasm_register_t reg = param->gp;
      if (rc == GP_REG) {
        reg = param->gp;
      } else {
        assert(rc == FP_REG);
        reg = param->fp;
      }
      set(&param_regs, reg);
      load_into_register(&transfers, reg, slot, stack_idx);
    } else if (is_stack(param->loc)) {
      push_stack_slot(ctx, slot, stack_idx, type);
    }
  }

  if (target != NULL && has(param_regs, *target)) {
    reglist_t free_regs = mask_out(get_cache_reg_list(GP_REG), param_regs);
    if (!is_empty(free_regs)) {
      sgxwasm_register_t new_target = get_first_reg_set(free_regs);
      move_register(&transfers, new_target, *target, VALTYPE_I64);
      *target = new_target;
    } else {
      assert(0);
      // Handle the case if there is no free regs.
    }
  }

  execute_transfers(ctx, &transfers);

// Pop parameters from the cache stack.
#if SGXWASM_DEBUG_MEMORY
  printf("[prepare_call] stack_resize - old: %zu, new: %zu\n",
         cache_state(ctx)->stack_state->size, param_base);
#endif
  if (!stack_resize(cache_state(ctx)->stack_state, param_base)) {
    assert(0);
  }

  // Reset register use counters.
  reset_used_registers(cache_state(ctx)->used_registers,
                       cache_state(ctx)->register_use_count);
}

__attribute__((unused)) static void
finish_call(struct CompilerContext* ctx, const struct FuncType* fun_type)
{
  if (return_type(fun_type) != VALTYPE_NULL) {
    sgxwasm_register_class_t rc = reg_class_for(return_type(fun_type));
    sgxwasm_register_t return_reg =
      (rc == GP_REG) ? GPReturnRegister : FPReturnRegister0;
    assert(is_free(*(cache_state(ctx)->used_registers),
                   cache_state(ctx)->register_use_count, return_reg));
    push_register(ctx, return_type(fun_type), return_reg);
  }
}

__attribute__((unused)) static void
call_direct(struct CompilerContext* ctx, const struct ModuleTypes* module_types,
            uint32_t fun_index)
{
  // NOTE: The current design put imported and in-module
  // together, we only needs to patch the call target later.

  const struct FuncType* fun_type = &module_types->functypes[fun_index];
  // sgxwasm_register_t addr =
  //  get_unused_register_with_class(ctx, GP_REG, EmptyRegList, EmptyRegList);
  sgxwasm_register_t addr;

  prepare_call(ctx, fun_type, NULL);

  addr =
    get_unused_register_with_class(ctx, GP_REG, EmptyRegList, EmptyRegList);

  load_from_memory(ctx, addr, MEMREF_FUNC, fun_index);

  // emit_call_r(output(ctx), addr);
  call_function(ctx, addr, fun_index);

  finish_call(ctx, fun_type);
}

__attribute__((unused)) static void
EmitTypeConversion(struct CompilerContext* ctx, sgxwasm_valtype_t dst_type,
                   sgxwasm_valtype_t src_type, uint8_t opcode, uint8_t can_trap,
                   size_t trap_position)
{
  sgxwasm_register_class_t src_rc = reg_class_for(src_type);
  sgxwasm_register_class_t dst_rc = reg_class_for(dst_type);
  sgxwasm_register_t src = pop_to_register(ctx, EmptyRegList);
  sgxwasm_register_t dst;
  if (src_rc == dst_rc) {
    reglist_t try_first = 0;
    set(&try_first, src);
    dst = get_unused_register_with_class(ctx, dst_rc, try_first, EmptyRegList);
  } else {
    dst =
      get_unused_register_with_class(ctx, dst_rc, EmptyRegList, EmptyRegList);
  }
  assert((!!can_trap) == (trap_position > 0));
  label_t* trap = can_trap
                    ? add_out_of_line_trap(out_of_line_code(ctx), trap_position,
                                           TrapFloatUnrepresentable, 0)
                    : NULL;

  num_low_instrs(ctx) +=
    emit_type_conversion(output(ctx), opcode, dst, src, trap);
  /*if (!emit_type_conversion(output(ctx), opcode, dst, src, trap)) {
    if (can_trap) {
      // XXX: Implement trap later.
    }
  }*/
  push_register(ctx, dst_type, dst);
}

__attribute__((unused)) condition_t
get_cond(uint8_t opcode)
{
  switch (opcode) {
    case OPCODE_I32_EQ:
    case OPCODE_I64_EQ:
    case OPCODE_F32_EQ:
    case OPCODE_F64_EQ:
      return COND_EQ;
    case OPCODE_I32_NE:
    case OPCODE_I64_NE:
    case OPCODE_F32_NE:
    case OPCODE_F64_NE:
      return COND_NE;
    case OPCODE_I32_LT_U:
    case OPCODE_I64_LT_U:
    case OPCODE_F32_LT:
    case OPCODE_F64_LT:
      return COND_LT_U;
    case OPCODE_I32_LT_S:
    case OPCODE_I64_LT_S:
      return COND_LT_S;
    case OPCODE_I32_LE_U:
    case OPCODE_I64_LE_U:
    case OPCODE_F32_LE:
    case OPCODE_F64_LE:
      return COND_LE_U;
    case OPCODE_I32_LE_S:
    case OPCODE_I64_LE_S:
      return COND_LE_S;
    case OPCODE_I32_GT_U:
    case OPCODE_I64_GT_U:
    case OPCODE_F32_GT:
    case OPCODE_F64_GT:
      return COND_GT_U;
    case OPCODE_I32_GT_S:
    case OPCODE_I64_GT_S:
      return COND_GT_S;
    case OPCODE_I32_GE_U:
    case OPCODE_I64_GE_U:
    case OPCODE_F32_GE:
    case OPCODE_F64_GE:
      return COND_GE_U;
    case OPCODE_I32_GE_S:
    case OPCODE_I64_GE_S:
      return COND_GE_S;
    default:
      assert(0);
  }
}

__attribute__((unused)) static void
EmitShiftOp(struct CompilerContext* ctx, sgxwasm_register_t dst,
            sgxwasm_register_t src, sgxwasm_register_t amount,
            sgxwasm_valtype_t type, uint8_t opcode)
{
  // If dst is rcx, compute into the scratch register first, then move to rcx.
  if (dst == GP_RCX) {
    num_low_instrs(ctx) += Move(output(ctx), ScratchGP, src, type);
    if (amount != GP_RCX) {
      num_low_instrs(ctx) += Move(output(ctx), GP_RCX, amount, type);
    }
    num_low_instrs(ctx) += Shift(output(ctx), ScratchGP, opcode);
    num_low_instrs(ctx) += Move(output(ctx), GP_RCX, ScratchGP, type);
    return;
  }

  // Move amount into rcx. If rcx is in use, move its content into the scratch
  // register. If src is rcx, src is now the scratch register.
  uint8_t use_scratch = 0;
  if (amount != GP_RCX) {
    use_scratch =
      (src == GP_RCX) || is_used(*(cache_state(ctx)->used_registers),
                                 cache_state(ctx)->register_use_count, GP_RCX);
    if (use_scratch) {
      num_low_instrs(ctx) +=
        emit_mov_rr(output(ctx), ScratchGP, GP_RCX, VALTYPE_I64);
    }
    if (src == GP_RCX) {
      src = ScratchGP;
    }
    num_low_instrs(ctx) += Move(output(ctx), GP_RCX, amount, type);
  }

  if (dst != src) {
    num_low_instrs(ctx) += Move(output(ctx), dst, src, type);
  }
  // Do the actual shift.
  num_low_instrs(ctx) += Shift(output(ctx), dst, opcode);

  // Restore rcx if needed.
  if (use_scratch) {
    num_low_instrs(ctx) +=
      emit_mov_rr(output(ctx), GP_RCX, ScratchGP, VALTYPE_I64);
  }
}

__attribute__((unused)) static void
EmitUnOp(struct CompilerContext* ctx, sgxwasm_valtype_t src_type,
         sgxwasm_valtype_t result_type, uint8_t opcode)
{
  sgxwasm_register_class_t src_rc = reg_class_for(src_type);
  sgxwasm_register_class_t result_rc = reg_class_for(result_type);
  sgxwasm_register_t src = pop_to_register(ctx, EmptyRegList);
  sgxwasm_register_t dst;
  if (src_rc == result_rc) {
    reglist_t try_first = 0;
    set(&try_first, src);
    dst =
      get_unused_register_with_class(ctx, result_rc, try_first, EmptyRegList);
  } else {
    dst = get_unused_register_with_class(ctx, result_rc, EmptyRegList,
                                         EmptyRegList);
  }

  switch (opcode) {
    case OPCODE_I32_EQZ: {
      assert(src_type == VALTYPE_I32 && result_type == VALTYPE_I32);
      num_low_instrs(ctx) += emit_test_rr(output(ctx), src, src, VALTYPE_I32);
      num_low_instrs(ctx) += emit_setcc(output(ctx), COND_EQ, dst);
      num_low_instrs(ctx) += emit_movzxb_rr(output(ctx), dst, dst);
      break;
    }
    case OPCODE_I32_CLZ: {
      assert(src_type == VALTYPE_I32 && result_type == VALTYPE_I32);
      label_t nonzero_input = { 0, 0 };
      label_t continuation = { 0, 0 };
      num_low_instrs(ctx) += emit_test_rr(output(ctx), src, src, VALTYPE_I32);
      num_low_instrs(ctx) +=
        emit_jcc(output(ctx), COND_NE, &nonzero_input, Near);
      num_low_instrs(ctx) += emit_movl_ri(output(ctx), dst, 32);
      num_low_instrs(ctx) += emit_jmp_label(output(ctx), &continuation, Near);

      bind_label(output(ctx), &nonzero_input, pc_offset(output(ctx)));
      // Get most significant bit set (MSBS).
      num_low_instrs(ctx) += emit_bsr_rr(output(ctx), dst, src, VALTYPE_I32);
      // CLZ = 31 - MSBS = MSBS ^ 31.
      num_low_instrs(ctx) += emit_xor_ri(output(ctx), dst, 31, VALTYPE_I32);

      bind_label(output(ctx), &continuation, pc_offset(output(ctx)));
      break;
    }
    case OPCODE_I32_CTZ: {
      assert(src_type == VALTYPE_I32 && result_type == VALTYPE_I32);
      label_t nonzero_input = { 0, 0 };
      label_t continuation = { 0, 0 };
      num_low_instrs(ctx) += emit_test_rr(output(ctx), src, src, VALTYPE_I32);
      num_low_instrs(ctx) +=
        emit_jcc(output(ctx), COND_NE, &nonzero_input, Near);
      num_low_instrs(ctx) += emit_movl_ri(output(ctx), dst, 32);
      num_low_instrs(ctx) += emit_jmp_label(output(ctx), &continuation, Near);

      bind_label(output(ctx), &nonzero_input, pc_offset(output(ctx)));
      // Get least significant bit set, which equals number of trailing zeros.
      num_low_instrs(ctx) += emit_bsf_rr(output(ctx), dst, src, VALTYPE_I32);

      bind_label(output(ctx), &continuation, pc_offset(output(ctx)));
      break;
    }
    case OPCODE_I32_POPCNT: {
      num_low_instrs(ctx) += emit_popcnt_rr(output(ctx), dst, src, VALTYPE_I32);
      break;
    }
    case OPCODE_I64_EQZ: {
      assert(src_type == VALTYPE_I64 && result_type == VALTYPE_I32);
      num_low_instrs(ctx) += emit_test_rr(output(ctx), src, src, VALTYPE_I64);
      num_low_instrs(ctx) += emit_setcc(output(ctx), COND_EQ, dst);
      num_low_instrs(ctx) += emit_movzxb_rr(output(ctx), dst, dst);
      break;
    }
    case OPCODE_I64_CLZ: {
      num_low_instrs(ctx) += emit_lzcnt_rr(output(ctx), dst, src, VALTYPE_I64);
      break;
    }
    case OPCODE_I64_CTZ: {
      num_low_instrs(ctx) += emit_tzcnt_rr(output(ctx), dst, src, VALTYPE_I64);
      break;
    }
    case OPCODE_I64_POPCNT: {
      num_low_instrs(ctx) += emit_popcnt_rr(output(ctx), dst, src, VALTYPE_I64);
      break;
    }
    case OPCODE_F32_ABS: {
      assert(src_type == VALTYPE_F32 && result_type == VALTYPE_F32);
      if (dst == src) {
        num_low_instrs(ctx) +=
          emit_movl_ri(output(ctx), ScratchGP, (int32_t)(SIGN_BIT_32 - 1));
        num_low_instrs(ctx) +=
          emit_sse_movd_rr(output(ctx), ScratchFP, ScratchGP);
        num_low_instrs(ctx) += emit_andps_rr(output(ctx), dst, ScratchFP);
      } else {
        num_low_instrs(ctx) +=
          emit_movl_ri(output(ctx), ScratchGP, (int32_t)(SIGN_BIT_32 - 1));
        num_low_instrs(ctx) += emit_sse_movd_rr(output(ctx), dst, ScratchGP);
        num_low_instrs(ctx) += emit_andps_rr(output(ctx), dst, src);
      }
      break;
    }
    case OPCODE_F32_NEG: {
      assert(src_type == VALTYPE_F32 && result_type == VALTYPE_F32);
      if (dst == src) {
        num_low_instrs(ctx) +=
          emit_movl_ri(output(ctx), ScratchGP, (int32_t)SIGN_BIT_32);
        num_low_instrs(ctx) +=
          emit_sse_movd_rr(output(ctx), ScratchFP, ScratchGP);
        num_low_instrs(ctx) += emit_xorps_rr(output(ctx), dst, ScratchFP);
      } else {
        num_low_instrs(ctx) +=
          emit_movl_ri(output(ctx), ScratchGP, (int32_t)SIGN_BIT_32);
        num_low_instrs(ctx) += emit_sse_movd_rr(output(ctx), dst, ScratchGP);
        num_low_instrs(ctx) += emit_xorps_rr(output(ctx), dst, src);
      }
      break;
    }
    case OPCODE_F32_CEIL: {
      assert(src_type == VALTYPE_F32 && result_type == VALTYPE_F32);
      num_low_instrs(ctx) += emit_roundss_rr(output(ctx), dst, src, RoundUp);
      break;
    }
    case OPCODE_F32_FLOOR: {
      assert(src_type == VALTYPE_F32 && result_type == VALTYPE_F32);
      num_low_instrs(ctx) += emit_roundss_rr(output(ctx), dst, src, RoundDown);
      break;
    }
    case OPCODE_F32_TRUNC: {
      assert(src_type == VALTYPE_F32 && result_type == VALTYPE_F32);
      num_low_instrs(ctx) +=
        emit_roundss_rr(output(ctx), dst, src, RoundToZero);
      break;
    }
    case OPCODE_F32_NEAREST: {
      assert(src_type == VALTYPE_F32 && result_type == VALTYPE_F32);
      num_low_instrs(ctx) +=
        emit_roundss_rr(output(ctx), dst, src, RoundToNearest);
      break;
    }
    case OPCODE_F32_SQRT: {
      assert(src_type == VALTYPE_F32 && result_type == VALTYPE_F32);
      num_low_instrs(ctx) += emit_sqrtss_rr(output(ctx), dst, src);
      break;
    }
    case OPCODE_F64_ABS: {
      assert(src_type == VALTYPE_F64 && result_type == VALTYPE_F64);
      if (dst == src) {
        num_low_instrs(ctx) +=
          emit_movq_ri(output(ctx), ScratchGP, (int64_t)(SIGN_BIT_64 - 1));
        num_low_instrs(ctx) +=
          emit_sse_movq_rr(output(ctx), ScratchFP, ScratchGP);
        num_low_instrs(ctx) += emit_andpd_rr(output(ctx), dst, ScratchFP);
      } else {
        num_low_instrs(ctx) +=
          emit_movq_ri(output(ctx), ScratchGP, (int64_t)(SIGN_BIT_64 - 1));
        num_low_instrs(ctx) += emit_sse_movq_rr(output(ctx), dst, ScratchGP);
        num_low_instrs(ctx) += emit_andpd_rr(output(ctx), dst, src);
      }
      break;
    }
    case OPCODE_F64_NEG: {
      assert(src_type == VALTYPE_F64 && result_type == VALTYPE_F64);
      if (dst == src) {
        num_low_instrs(ctx) +=
          emit_movq_ri(output(ctx), ScratchGP, (int64_t)SIGN_BIT_64);
        num_low_instrs(ctx) +=
          emit_sse_movq_rr(output(ctx), ScratchFP, ScratchGP);
        num_low_instrs(ctx) += emit_xorpd_rr(output(ctx), dst, ScratchFP);
      } else {
        num_low_instrs(ctx) +=
          emit_movq_ri(output(ctx), ScratchGP, (int64_t)SIGN_BIT_64);
        num_low_instrs(ctx) += emit_sse_movq_rr(output(ctx), dst, ScratchGP);
        num_low_instrs(ctx) += emit_xorpd_rr(output(ctx), dst, src);
      }
      break;
    }
    case OPCODE_F64_CEIL: {
      assert(src_type == VALTYPE_F64 && result_type == VALTYPE_F64);
      num_low_instrs(ctx) += emit_roundsd_rr(output(ctx), dst, src, RoundUp);
      break;
    }
    case OPCODE_F64_FLOOR: {
      assert(src_type == VALTYPE_F64 && result_type == VALTYPE_F64);
      num_low_instrs(ctx) += emit_roundsd_rr(output(ctx), dst, src, RoundDown);
      break;
    }
    case OPCODE_F64_TRUNC: {
      assert(src_type == VALTYPE_F64 && result_type == VALTYPE_F64);
      num_low_instrs(ctx) +=
        emit_roundsd_rr(output(ctx), dst, src, RoundToZero);
      break;
    }
    case OPCODE_F64_NEAREST: {
      assert(src_type == VALTYPE_F64 && result_type == VALTYPE_F64);
      num_low_instrs(ctx) +=
        emit_roundsd_rr(output(ctx), dst, src, RoundToNearest);
      break;
    }
    case OPCODE_F64_SQRT: {
      assert(src_type == VALTYPE_F64 && result_type == VALTYPE_F64);
      num_low_instrs(ctx) += emit_sqrtsd_rr(output(ctx), dst, src);
      break;
    }
    default:
      break;
  }
  push_register(ctx, result_type, dst);
}

__attribute__((unused)) static void
EmitBinOp(struct CompilerContext* ctx, sgxwasm_valtype_t src_type,
          sgxwasm_valtype_t result_type, uint8_t opcode)
{
  sgxwasm_register_class_t src_rc = reg_class_for(src_type);
  sgxwasm_register_class_t result_rc = reg_class_for(result_type);
  sgxwasm_register_t rhs, lhs, dst;
  reglist_t pinned = 0;

  rhs = pop_to_register(ctx, EmptyRegList);
  set(&pinned, rhs);
  lhs = pop_to_register(ctx, pinned);
  set(&pinned, lhs);
  if (src_rc == result_rc) {
    dst = get_unused_register_with_class(ctx, result_rc, EmptyRegList, pinned);
  } else {
    dst = get_unused_register_with_class(ctx, result_rc, EmptyRegList,
                                         EmptyRegList);
  }

  switch (opcode) {
    // Arthimetic operations
    case OPCODE_I32_ADD: {
      assert(src_type == VALTYPE_I32 && result_type == VALTYPE_I32);
      if (lhs != dst) {
        struct Operand op;
        build_operand(&op, lhs, rhs, SCALE_1, 0);
        num_low_instrs(ctx) += emit_lea_rm(output(ctx), dst, &op, VALTYPE_I32);
      } else {
        num_low_instrs(ctx) += emit_add_rr(output(ctx), dst, rhs, VALTYPE_I32);
      }
      break;
    }
    case OPCODE_I32_SUB: {
      assert(src_type == VALTYPE_I32 && result_type == VALTYPE_I32);
      if (dst == rhs) {
        num_low_instrs(ctx) += emit_neq_r(output(ctx), dst, VALTYPE_I32);
        num_low_instrs(ctx) += emit_add_rr(output(ctx), dst, lhs, VALTYPE_I32);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) +=
            emit_mov_rr(output(ctx), dst, lhs, VALTYPE_I32);
        }
        num_low_instrs(ctx) += emit_sub_rr(output(ctx), dst, rhs, VALTYPE_I32);
      }
      break;
    }
    case OPCODE_I32_MUL: {
      assert(src_type == VALTYPE_I32 && result_type == VALTYPE_I32);
      if (dst == rhs) {
        num_low_instrs(ctx) += emit_imul_rr(output(ctx), dst, lhs, VALTYPE_I32);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) +=
            emit_mov_rr(output(ctx), dst, lhs, VALTYPE_I32);
        }
        num_low_instrs(ctx) += emit_imul_rr(output(ctx), dst, rhs, VALTYPE_I32);
      }
      break;
    }
    case OPCODE_I32_AND: {
      assert(src_type == VALTYPE_I32 && result_type == VALTYPE_I32);
      if (dst == rhs) {
        num_low_instrs(ctx) += emit_and_rr(output(ctx), dst, lhs, VALTYPE_I32);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) +=
            emit_mov_rr(output(ctx), dst, lhs, VALTYPE_I32);
        }
        num_low_instrs(ctx) += emit_and_rr(output(ctx), dst, rhs, VALTYPE_I32);
      }
      break;
    }
    case OPCODE_I32_OR: {
      assert(src_type == VALTYPE_I32 && result_type == VALTYPE_I32);
      if (dst == rhs) {
        num_low_instrs(ctx) += emit_or_rr(output(ctx), dst, lhs, VALTYPE_I32);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) +=
            emit_mov_rr(output(ctx), dst, lhs, VALTYPE_I32);
        }
        num_low_instrs(ctx) += emit_or_rr(output(ctx), dst, rhs, VALTYPE_I32);
      }
      break;
    }
    case OPCODE_I32_XOR: {
      assert(src_type == VALTYPE_I32 && result_type == VALTYPE_I32);
      if (dst == rhs) {
        num_low_instrs(ctx) += emit_xor_rr(output(ctx), dst, lhs, VALTYPE_I32);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) +=
            emit_mov_rr(output(ctx), dst, lhs, VALTYPE_I32);
        }
        num_low_instrs(ctx) += emit_xor_rr(output(ctx), dst, rhs, VALTYPE_I32);
      }
      break;
    }
    case OPCODE_I32_DIV_U: {
      assert(src_type == VALTYPE_I32 && result_type == VALTYPE_I32);
      size_t position = pc_offset(output(ctx));
      label_t* div_by_zero =
        add_out_of_line_trap(out_of_line_code(ctx), position, TrapDivByZero, 0);
      // Make sure {edx:eax} is not used before emiting div or rem.
      spill_register(ctx, GP_RAX);
      spill_register(ctx, GP_RDX);
      num_low_instrs(ctx) +=
        emit_div_or_rem(output(ctx), dst, lhs, rhs, div_by_zero, NULL,
                        VALTYPE_I32, INT_DIV, VAL_UNSIGNED);
      break;
    }
    case OPCODE_I32_DIV_S: {
      assert(src_type == VALTYPE_I32 && result_type == VALTYPE_I32);
      size_t position = pc_offset(output(ctx));
      label_t* div_by_zero =
        add_out_of_line_trap(out_of_line_code(ctx), position, TrapDivByZero, 0);
      label_t* div_unrepresentable = add_out_of_line_trap(
        out_of_line_code(ctx), position, TrapDivUnrepresentable, 0);
      // Make sure {edx:eax} is not used before emiting div or rem.
      spill_register(ctx, GP_RAX);
      spill_register(ctx, GP_RDX);
      num_low_instrs(ctx) +=
        emit_div_or_rem(output(ctx), dst, lhs, rhs, div_by_zero,
                        div_unrepresentable, VALTYPE_I32, INT_DIV, VAL_SIGNED);
      break;
    }
    case OPCODE_I32_REM_U: {
      assert(src_type == VALTYPE_I32 && result_type == VALTYPE_I32);
      size_t position = pc_offset(output(ctx));
      label_t* rem_by_zero =
        add_out_of_line_trap(out_of_line_code(ctx), position, TrapRemByZero, 0);
      // Make sure {edx:eax} is not used before emiting div or rem.
      spill_register(ctx, GP_RAX);
      spill_register(ctx, GP_RDX);
      num_low_instrs(ctx) +=
        emit_div_or_rem(output(ctx), dst, lhs, rhs, rem_by_zero, NULL,
                        VALTYPE_I32, INT_REM, VAL_UNSIGNED);
      break;
    }
    case OPCODE_I32_REM_S: {
      assert(src_type == VALTYPE_I32 && result_type == VALTYPE_I32);
      size_t position = pc_offset(output(ctx));
      label_t* rem_by_zero =
        add_out_of_line_trap(out_of_line_code(ctx), position, TrapRemByZero, 0);
      // Make sure {edx:eax} is not used before emiting div or rem.
      spill_register(ctx, GP_RAX);
      spill_register(ctx, GP_RDX);
      num_low_instrs(ctx) +=
        emit_div_or_rem(output(ctx), dst, lhs, rhs, rem_by_zero, NULL,
                        VALTYPE_I32, INT_REM, VAL_SIGNED);
      break;
    }
    case OPCODE_I64_ADD: {
      assert(src_type == VALTYPE_I64 && result_type == VALTYPE_I64);
      if (lhs != dst) {
        struct Operand op;
        build_operand(&op, lhs, rhs, SCALE_1, 0);
        num_low_instrs(ctx) += emit_lea_rm(output(ctx), dst, &op, VALTYPE_I64);
      } else {
        num_low_instrs(ctx) += emit_add_rr(output(ctx), dst, rhs, VALTYPE_I64);
      }
      break;
    }
    case OPCODE_I64_SUB: {
      assert(src_type == VALTYPE_I64 && result_type == VALTYPE_I64);
      if (dst == rhs) {
        num_low_instrs(ctx) += emit_neq_r(output(ctx), dst, VALTYPE_I64);
        num_low_instrs(ctx) += emit_add_rr(output(ctx), dst, lhs, VALTYPE_I64);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) +=
            emit_mov_rr(output(ctx), dst, lhs, VALTYPE_I64);
        }
        num_low_instrs(ctx) += emit_sub_rr(output(ctx), dst, rhs, VALTYPE_I64);
      }
      break;
    }
    case OPCODE_I64_MUL: {
      assert(src_type == VALTYPE_I64 && result_type == VALTYPE_I64);
      if (dst == rhs) {
        num_low_instrs(ctx) += emit_imul_rr(output(ctx), dst, lhs, VALTYPE_I64);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) +=
            emit_mov_rr(output(ctx), dst, lhs, VALTYPE_I64);
        }
        num_low_instrs(ctx) += emit_imul_rr(output(ctx), dst, rhs, VALTYPE_I64);
      }
      break;
    }
    case OPCODE_I64_AND: {
      assert(src_type == VALTYPE_I64 && result_type == VALTYPE_I64);
      if (dst == rhs) {
        num_low_instrs(ctx) += emit_and_rr(output(ctx), dst, lhs, VALTYPE_I64);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) +=
            emit_mov_rr(output(ctx), dst, lhs, VALTYPE_I64);
        }
        num_low_instrs(ctx) += emit_and_rr(output(ctx), dst, rhs, VALTYPE_I64);
      }
      break;
    }
    case OPCODE_I64_OR: {
      assert(src_type == VALTYPE_I64 && result_type == VALTYPE_I64);
      if (dst == rhs) {
        num_low_instrs(ctx) += emit_or_rr(output(ctx), dst, lhs, VALTYPE_I64);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) +=
            emit_mov_rr(output(ctx), dst, lhs, VALTYPE_I64);
        }
        num_low_instrs(ctx) += emit_or_rr(output(ctx), dst, rhs, VALTYPE_I64);
      }
      break;
    }
    case OPCODE_I64_XOR: {
      assert(src_type == VALTYPE_I64 && result_type == VALTYPE_I64);
      if (dst == rhs) {
        num_low_instrs(ctx) += emit_xor_rr(output(ctx), dst, lhs, VALTYPE_I64);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) +=
            emit_mov_rr(output(ctx), dst, lhs, VALTYPE_I64);
        }
        num_low_instrs(ctx) += emit_xor_rr(output(ctx), dst, rhs, VALTYPE_I64);
      }
      break;
    }
    case OPCODE_I64_DIV_U: {
      assert(src_type == VALTYPE_I64 && result_type == VALTYPE_I64);
      size_t position = pc_offset(output(ctx));
      label_t* div_by_zero =
        add_out_of_line_trap(out_of_line_code(ctx), position, TrapDivByZero, 0);
      // Make sure {edx:eax} is not used before emiting div or rem.
      spill_register(ctx, GP_RAX);
      spill_register(ctx, GP_RDX);
      num_low_instrs(ctx) +=
        emit_div_or_rem(output(ctx), dst, lhs, rhs, div_by_zero, NULL,
                        VALTYPE_I64, INT_DIV, VAL_UNSIGNED);
      break;
    }
    case OPCODE_I64_DIV_S: {
      assert(src_type == VALTYPE_I64 && result_type == VALTYPE_I64);
      size_t position = pc_offset(output(ctx));
      label_t* div_by_zero =
        add_out_of_line_trap(out_of_line_code(ctx), position, TrapDivByZero, 0);
      label_t* div_unrepresentable = add_out_of_line_trap(
        out_of_line_code(ctx), position, TrapDivUnrepresentable, 0);
      // Make sure {edx:eax} is not used before emiting div or rem.
      spill_register(ctx, GP_RAX);
      spill_register(ctx, GP_RDX);
      num_low_instrs(ctx) +=
        emit_div_or_rem(output(ctx), dst, lhs, rhs, div_by_zero,
                        div_unrepresentable, VALTYPE_I64, INT_DIV, VAL_SIGNED);
      break;
    }
    case OPCODE_I64_REM_U: {
      assert(src_type == VALTYPE_I64 && result_type == VALTYPE_I64);
      size_t position = pc_offset(output(ctx));
      label_t* rem_by_zero =
        add_out_of_line_trap(out_of_line_code(ctx), position, TrapRemByZero, 0);
      // Make sure {edx:eax} is not used before emiting div or rem.
      spill_register(ctx, GP_RAX);
      spill_register(ctx, GP_RDX);
      num_low_instrs(ctx) +=
        emit_div_or_rem(output(ctx), dst, lhs, rhs, rem_by_zero, NULL,
                        VALTYPE_I64, INT_REM, VAL_UNSIGNED);
      break;
    }
    case OPCODE_I64_REM_S: {
      assert(src_type == VALTYPE_I64 && result_type == VALTYPE_I64);
      size_t position = pc_offset(output(ctx));
      label_t* rem_by_zero =
        add_out_of_line_trap(out_of_line_code(ctx), position, TrapRemByZero, 0);
      // Make sure {edx:eax} is not used before emiting div or rem.
      spill_register(ctx, GP_RAX);
      spill_register(ctx, GP_RDX);
      num_low_instrs(ctx) +=
        emit_div_or_rem(output(ctx), dst, lhs, rhs, rem_by_zero, NULL,
                        VALTYPE_I64, INT_REM, VAL_SIGNED);
      break;
    }
    // Comparison operations
    case OPCODE_I32_EQ:
    case OPCODE_I32_NE:
    case OPCODE_I32_LT_S:
    case OPCODE_I32_LT_U:
    case OPCODE_I32_GT_S:
    case OPCODE_I32_GT_U:
    case OPCODE_I32_LE_S:
    case OPCODE_I32_LE_U:
    case OPCODE_I32_GE_S:
    case OPCODE_I32_GE_U: {
      assert(src_type == VALTYPE_I32 && result_type == VALTYPE_I32);
      condition_t cond = get_cond(opcode);
      num_low_instrs(ctx) +=
        emit_set_cond(output(ctx), cond, dst, lhs, rhs, VALTYPE_I32);
      break;
    }
    case OPCODE_I64_EQ:
    case OPCODE_I64_NE:
    case OPCODE_I64_LT_S:
    case OPCODE_I64_LT_U:
    case OPCODE_I64_GT_S:
    case OPCODE_I64_GT_U:
    case OPCODE_I64_LE_S:
    case OPCODE_I64_LE_U:
    case OPCODE_I64_GE_S:
    case OPCODE_I64_GE_U: {
      assert(src_type == VALTYPE_I64 && result_type == VALTYPE_I32);
      condition_t cond = get_cond(opcode);
      num_low_instrs(ctx) +=
        emit_set_cond(output(ctx), cond, dst, lhs, rhs, VALTYPE_I64);
      break;
    }
    case OPCODE_F32_EQ:
    case OPCODE_F32_NE:
    case OPCODE_F32_LT:
    case OPCODE_F32_GT:
    case OPCODE_F32_LE:
    case OPCODE_F32_GE: {
      assert(src_type == VALTYPE_F32 && result_type == VALTYPE_I32);
      condition_t cond = get_cond(opcode);
      num_low_instrs(ctx) +=
        emit_float_set_cond(output(ctx), cond, dst, lhs, rhs, VALTYPE_F32);
      break;
    }
    case OPCODE_F64_EQ:
    case OPCODE_F64_NE:
    case OPCODE_F64_LT:
    case OPCODE_F64_GT:
    case OPCODE_F64_LE:
    case OPCODE_F64_GE: {
      assert(src_type == VALTYPE_F64 && result_type == VALTYPE_I32);
      condition_t cond = get_cond(opcode);
      num_low_instrs(ctx) +=
        emit_float_set_cond(output(ctx), cond, dst, lhs, rhs, VALTYPE_F64);
      break;
    }
    case OPCODE_I32_SHL:
    case OPCODE_I32_SHR_U:
    case OPCODE_I32_SHR_S:
    case OPCODE_I32_ROTL:
    case OPCODE_I32_ROTR: {
      assert(src_type == VALTYPE_I32 && result_type == VALTYPE_I32);
      EmitShiftOp(ctx, dst, lhs, rhs, VALTYPE_I32, opcode);
      break;
    }
    case OPCODE_I64_SHL:
    case OPCODE_I64_SHR_U:
    case OPCODE_I64_SHR_S:
    case OPCODE_I64_ROTL:
    case OPCODE_I64_ROTR: {
      assert(src_type == VALTYPE_I64 && result_type == VALTYPE_I64);
      EmitShiftOp(ctx, dst, lhs, rhs, VALTYPE_I64, opcode);
      break;
    }
    case OPCODE_F32_ADD: {
      assert(src_type == VALTYPE_F32 && result_type == VALTYPE_F32);
      if (dst == lhs) {
        num_low_instrs(ctx) += emit_addss_rr(output(ctx), dst, lhs);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) += emit_movss_rr(output(ctx), dst, lhs);
        }
        num_low_instrs(ctx) += emit_addss_rr(output(ctx), dst, rhs);
      }
      break;
    }
    case OPCODE_F32_SUB: {
      assert(src_type == VALTYPE_F32 && result_type == VALTYPE_F32);
      if (dst == lhs) {
        num_low_instrs(ctx) += emit_subss_rr(output(ctx), dst, lhs);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) += emit_movss_rr(output(ctx), dst, lhs);
        }
        num_low_instrs(ctx) += emit_subss_rr(output(ctx), dst, rhs);
      }
      break;
    }
    case OPCODE_F32_MUL: {
      assert(src_type == VALTYPE_F32 && result_type == VALTYPE_F32);
      if (dst == lhs) {
        num_low_instrs(ctx) += emit_mulss_rr(output(ctx), dst, lhs);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) += emit_movss_rr(output(ctx), dst, lhs);
        }
        num_low_instrs(ctx) += emit_mulss_rr(output(ctx), dst, rhs);
      }
      break;
    }
    case OPCODE_F32_DIV: {
      assert(src_type == VALTYPE_F32 && result_type == VALTYPE_F32);
      if (dst == lhs) {
        num_low_instrs(ctx) += emit_divss_rr(output(ctx), dst, lhs);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) += emit_movss_rr(output(ctx), dst, lhs);
        }
        num_low_instrs(ctx) += emit_divss_rr(output(ctx), dst, rhs);
      }
      break;
    }
    case OPCODE_F32_MIN: {
      assert(src_type == VALTYPE_F32 && result_type == VALTYPE_F32);
      num_low_instrs(ctx) += emit_float_min_or_max(output(ctx), dst, lhs, rhs,
                                                   FLOAT_MIN, VALTYPE_F32);
      break;
    }
    case OPCODE_F32_MAX: {
      assert(src_type == VALTYPE_F32 && result_type == VALTYPE_F32);
      num_low_instrs(ctx) += emit_float_min_or_max(output(ctx), dst, lhs, rhs,
                                                   FLOAT_MAX, VALTYPE_F32);
      break;
    }
    case OPCODE_F32_COPYSIGN: {
      assert(src_type == VALTYPE_F32 && result_type == VALTYPE_F32);
      const int32_t sign_bit = 1 << 31;
      num_low_instrs(ctx) += emit_sse_movd_rr(output(ctx), ScratchGP, lhs);
      num_low_instrs(ctx) +=
        emit_and_ri(output(ctx), ScratchGP, ~sign_bit, VALTYPE_I32);
      num_low_instrs(ctx) += emit_sse_movd_rr(output(ctx), ScratchGP2, rhs);
      num_low_instrs(ctx) +=
        emit_and_ri(output(ctx), ScratchGP2, sign_bit, VALTYPE_I32);
      num_low_instrs(ctx) +=
        emit_or_rr(output(ctx), ScratchGP, ScratchGP2, VALTYPE_I32);
      num_low_instrs(ctx) += emit_sse_movd_rr(output(ctx), dst, ScratchGP);
#if __OPT_REUSE_REGISTER__
      set_register_use(ctx, ScratchGP2, MEMREF_NONE, 0);
#endif
      break;
    }
    case OPCODE_F64_ADD: {
      assert(src_type == VALTYPE_F64 && result_type == VALTYPE_F64);
      if (dst == lhs) {
        num_low_instrs(ctx) += emit_addsd_rr(output(ctx), dst, lhs);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) += emit_movsd_rr(output(ctx), dst, lhs);
        }
        num_low_instrs(ctx) += emit_addsd_rr(output(ctx), dst, rhs);
      }
      break;
    }
    case OPCODE_F64_SUB: {
      assert(src_type == VALTYPE_F64 && result_type == VALTYPE_F64);
      if (dst == lhs) {
        num_low_instrs(ctx) += emit_subsd_rr(output(ctx), dst, lhs);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) += emit_movsd_rr(output(ctx), dst, lhs);
        }
        num_low_instrs(ctx) += emit_subsd_rr(output(ctx), dst, rhs);
      }
      break;
    }
    case OPCODE_F64_MUL: {
      assert(src_type == VALTYPE_F64 && result_type == VALTYPE_F64);
      if (dst == lhs) {
        num_low_instrs(ctx) += emit_mulsd_rr(output(ctx), dst, lhs);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) += emit_movsd_rr(output(ctx), dst, lhs);
        }
        num_low_instrs(ctx) += emit_mulsd_rr(output(ctx), dst, rhs);
      }
      break;
    }
    case OPCODE_F64_DIV: {
      assert(src_type == VALTYPE_F64 && result_type == VALTYPE_F64);
      if (dst == lhs) {
        num_low_instrs(ctx) += emit_divsd_rr(output(ctx), dst, lhs);
      } else {
        if (dst != lhs) {
          num_low_instrs(ctx) += emit_movsd_rr(output(ctx), dst, lhs);
        }
        num_low_instrs(ctx) += emit_divsd_rr(output(ctx), dst, rhs);
      }
      break;
    }
    case OPCODE_F64_MIN: {
      assert(src_type == VALTYPE_F64 && result_type == VALTYPE_F64);
      num_low_instrs(ctx) += emit_float_min_or_max(output(ctx), dst, lhs, rhs,
                                                   FLOAT_MIN, VALTYPE_F64);
      break;
    }
    case OPCODE_F64_MAX: {
      assert(src_type == VALTYPE_F64 && result_type == VALTYPE_F64);
      num_low_instrs(ctx) += emit_float_min_or_max(output(ctx), dst, lhs, rhs,
                                                   FLOAT_MAX, VALTYPE_F64);
      break;
    }
    case OPCODE_F64_COPYSIGN: {
      assert(src_type == VALTYPE_F64 && result_type == VALTYPE_F64);
      num_low_instrs(ctx) += emit_sse_movq_rr(output(ctx), ScratchGP2, rhs);
      num_low_instrs(ctx) +=
        emit_shr_ri(output(ctx), ScratchGP2, 63, VALTYPE_I64);
      num_low_instrs(ctx) +=
        emit_shl_ri(output(ctx), ScratchGP2, 63, VALTYPE_I64);
      num_low_instrs(ctx) += emit_sse_movq_rr(output(ctx), ScratchGP, lhs);
      num_low_instrs(ctx) += emit_btr_ri(output(ctx), ScratchGP, 63);
      num_low_instrs(ctx) +=
        emit_or_rr(output(ctx), ScratchGP, ScratchGP2, VALTYPE_I64);
      num_low_instrs(ctx) += emit_sse_movq_rr(output(ctx), dst, ScratchGP);
#if __OPT_REUSE_REGISTER__
      set_register_use(ctx, ScratchGP2, MEMREF_NONE, 0);
#endif
      break;
    }
    default: {
      printf("Unsupported opcode\n");
      assert(0);
    }
  }
  push_register(ctx, result_type, dst);
}

// Try to de-duplicate the type id with the same signature.
size_t
dedup_type(const struct FuncTypeVector* type_table, size_t id)
{
  assert(id < type_table->size);
  size_t i, j;
  int found = 0;
  struct FuncType target_type = type_table->data[id];
  for (i = 0; i < id; i++) {
    struct FuncType type = type_table->data[i];
    if (target_type.n_inputs != type.n_inputs) {
      continue;
    }
    for (j = 0; j < target_type.n_inputs; j++) {
      if (target_type.input_types[j] != type.input_types[j]) {
        found = 0;
        break;
      }
      found = 1;
    }
    if (found == 0) {
      continue;
    }
    if (target_type.output_type != type.output_type) {
      found = 0;
      continue;
    }
    found = 1;
    break;
  }
  if (found == 1) {
    return i;
  }
  // Not found duplicated type, return the original one.
  return id;
}

__attribute__((unused)) static void
call_indirect(struct CompilerContext* ctx,
              // const struct TypeSection* type_table,
              const struct FuncTypeVector* type_table, uint32_t type_index)
{
  const struct FuncType* fun_type;
  type_index = dedup_type(type_table, type_index);
  assert(type_index < type_table->size);
  fun_type = &type_table->data[type_index];

  // Pop the index.
  sgxwasm_register_t index = pop_to_register(ctx, EmptyRegList);
  assert(is_gp(index));
  // If that register is still being used after popping, we move it to another
  // register, because we want to modify that register.
  if (is_used(*(cache_state(ctx)->used_registers),
              cache_state(ctx)->register_use_count, index)) {
    sgxwasm_register_t new_index =
      get_unused_register_with_class(ctx, GP_REG, EmptyRegList, EmptyRegList);
    num_low_instrs(ctx) += Move(output(ctx), new_index, index, VALTYPE_I32);
    index = new_index;
  }

  reglist_t pinned = 0;
  set(&pinned, index);
  // Get three temporary registers.
  sgxwasm_register_t table =
    get_unused_register_with_class(ctx, GP_REG, EmptyRegList, pinned);
  set(&pinned, table);
  sgxwasm_register_t tmp_const =
    get_unused_register_with_class(ctx, GP_REG, EmptyRegList, pinned);
  set(&pinned, tmp_const);
  sgxwasm_register_t scratch =
    get_unused_register_with_class(ctx, GP_REG, EmptyRegList, pinned);
  set(&pinned, scratch);

  // Bounds check against the table size.
  label_t* invalid_func_label = add_out_of_line_trap(
    out_of_line_code(ctx), pc_offset(output(ctx)), TrapFuncInvalid, 0);

  load_from_memory(ctx, tmp_const, MEMREF_TABLE_SIZE, 0);
  num_low_instrs(ctx) +=
    Load(output(ctx), tmp_const, tmp_const, REG_UNKNOWN, 0, I32Load, NULL, 0);
  num_low_instrs(ctx) += emit_cond_jump_rr(
    output(ctx), COND_GE_U, invalid_func_label, VALTYPE_I32, index, tmp_const);

  load_from_memory(ctx, table, MEMREF_TABLE_SIGS, 0);
  load_const_to_reg(ctx, tmp_const, sizeof(uint32_t), VALTYPE_I32);
  num_low_instrs(ctx) +=
    emit_imul_rr(output(ctx), index, tmp_const, VALTYPE_I32);
  num_low_instrs(ctx) +=
    Load(output(ctx), scratch, table, index, 0, I32Load, NULL, 0);

  // Compare against expected signature.
  load_const_to_reg(ctx, tmp_const, type_index, VALTYPE_I32);

  label_t* sig_mismatch_label = add_out_of_line_trap(
    out_of_line_code(ctx), pc_offset(output(ctx)), TrapFuncSigMismatch, 0);
  num_low_instrs(ctx) += emit_cond_jump_rr(
    output(ctx), COND_NE, sig_mismatch_label, VALTYPE_I32, scratch, tmp_const);

  // {index} has already been multiplied by 4. Multiply by another 2.
  load_const_to_reg(ctx, tmp_const, 2, VALTYPE_I32);
  num_low_instrs(ctx) +=
    emit_imul_rr(output(ctx), index, tmp_const, VALTYPE_I32);

  load_from_memory(ctx, table, MEMREF_TABLE, 0);
  num_low_instrs(ctx) +=
    Load(output(ctx), scratch, table, index, 0, I64Load, NULL, 0);

  // Do the indirect call.
  prepare_call(ctx, fun_type, &scratch);

  num_low_instrs(ctx) += emit_call_r(output(ctx), scratch);

  finish_call(ctx, fun_type);
}

enum MEM_OOB_CHECK
{
  STATIC_CHECK,
  DYNAMIC_CHECK,
};
typedef enum MEM_OOB_CHECK mem_oob_check_t;

// NOTE: Emscripten already inserts the runtime check on the beginning
// of the function.
__attribute__((unused)) static int
bounds_check_mem(struct CompilerContext* ctx, uint32_t access_size,
                 uint32_t offset, sgxwasm_register_t index, reglist_t* pinned,
                 mem_oob_check_t check_type)
{
  // Static check.
  int static_oob = !is_in_bounds(offset, access_size, ctx->max_memory_size);

  // The memory access is in bounds.
  if (!static_oob && (check_type == STATIC_CHECK)) {
    return 0;
  }

  size_t position = pc_offset(output(ctx));
  label_t* trap_label = add_out_of_line_trap(out_of_line_code(ctx), position,
                                             TrapMemOutOfBounds, 0);

  if (static_oob) {
    num_low_instrs(ctx) += emit_jmp_label(output(ctx), trap_label, Far);
    struct ControlBlock* current_block = control_at(control(ctx), 0);
    if (reachable(current_block)) {
      current_block->reachability = SpecOnlyReachable;
    }
    return 1;
  }

  assert(check_type == DYNAMIC_CHECK);

  (void)index;
  (void)pinned;

  return 0;
}

__attribute__((unused)) static void
load_mem(struct CompilerContext* ctx, load_type_t type, uint32_t offset)
{
  sgxwasm_valtype_t value_type = get_load_value_type(type);
  reglist_t pinned = 0;
  sgxwasm_register_t index = pop_to_register(ctx, EmptyRegList);
  assert(is_gp(index));
  set(&pinned, index);
// Bounds check.
// TODO: Memory masking.

#if __OPT_REUSE_REGISTER__
  sgxwasm_register_t addr = find_register_use(ctx, MEMREF_MEM, 0);
  if (addr == REG_UNKNOWN) {
    load_from_memory(ctx, ScratchGP2, MEMREF_MEM, 0);
    addr = ScratchGP2;
  }
#else
  sgxwasm_register_t addr =
    get_unused_register_with_class(ctx, GP_REG, EmptyRegList, pinned);
  load_from_memory(ctx, addr, MEMREF_MEM, 0);
  set(&pinned, addr);
#endif

  sgxwasm_register_class_t rc = reg_class_for(value_type);
  sgxwasm_register_t value =
    get_unused_register_with_class(ctx, rc, EmptyRegList, pinned);
  set(&pinned, value);

  uint32_t protected_load_pc = 0;
  num_low_instrs(ctx) +=
    Load(output(ctx), value, addr, index, offset, type, &protected_load_pc, 1);
  // use_trap_handler
  push_register(ctx, value_type, value);
}

__attribute__((unused)) static void
store_mem(struct CompilerContext* ctx, store_type_t type, uint32_t offset)
{
  sgxwasm_valtype_t value_type = get_store_value_type(type);
  // Type check.
  (void)value_type;
  reglist_t pinned = 0;
  sgxwasm_register_t value = pop_to_register(ctx, EmptyRegList);
  set(&pinned, value);
  sgxwasm_register_t index = pop_to_register(ctx, pinned);
  assert(is_gp(index));
  set(&pinned, index);
// Bounds check.

// TODO: Memory masking.
#if __OPT_REUSE_REGISTER__
  sgxwasm_register_t addr = find_register_use(ctx, MEMREF_MEM, 0);
  if (addr == REG_UNKNOWN) {
    load_from_memory(ctx, ScratchGP2, MEMREF_MEM, 0);
    addr = ScratchGP2;
  }
#else
  sgxwasm_register_t addr =
    get_unused_register_with_class(ctx, GP_REG, EmptyRegList, pinned);
  load_from_memory(ctx, addr, MEMREF_MEM, 0);
  set(&pinned, addr);
#endif

  uint32_t protected_load_pc = 0;
  // reglist_t outer_pinned = 0;

  num_low_instrs(ctx) +=
    Store(output(ctx), addr, index, offset, value, type, &protected_load_pc, 1);
  // use_trap_handler
}
__attribute__((unused)) static int
sgxwasm_compile_function_body(struct CompilerContext* ctx,
                              // const struct TypeSection* type_table,
                              const struct FuncTypeVector* type_table,
                              const struct ModuleTypes* module_types,
                              size_t num_funs, size_t n_frame_locals,
                              const struct Instr* instructions,
                              size_t n_instructions, size_t* max_stack,
                              unsigned flags)
{
  assert(ctx->func != NULL);
  const struct Function* func = ctx->func;
  (void)n_frame_locals;
  (void)max_stack;
  (void)flags;

  (void)func;

  // Set up initial control block.
  struct ControlBlock* c =
    push_control(control(ctx), CONTROL_BLOCK, 0, return_count(&ctx->sig));
  if (c == NULL)
    goto error;
  c->label_state.stack_base = num_locals(ctx);
  // Backward compatiable.
  c->instructions = instructions;
  c->n_instructions = n_instructions;
  c->cont = 0;
  update_compile_state(ctx);

  /*#if __PASS__
    passes_function_start(ctx);
  #endif*/

  while (control_depth(control(ctx)) != 0) {
    const struct Instr* instr = get_next_instr(ctx);
    assert(instr != NULL);
    uint8_t opcode = instr->opcode;

#if SGXWASM_DEBUG_COMPILE
// dump_stack_top(cache_state(ctx), 5);
#endif

#if SGXWASM_DEBUG_COMPILE
    dump_instruction(instr, 0);
#endif

#if __PASS__
    passes_instruction_start(ctx);
#endif

    // Skip unreachable instructions.
    if (opcode != OPCODE_END && opcode != OPCODE_ELSE && unreachable(c)) {
      // printf("skip opcode: %x\n", opcode);
      continue;
    }

    switch (opcode) {
      case OPCODE_BLOCK: {
#if SGXWASM_DEBUG_COMPILE
        printf("OPCODE_BLOCK\n");
#endif
        uint32_t in_arity = 0;
        uint32_t out_arity =
          instr->data.block.blocktype != VALTYPE_NULL ? 1 : 0;

        // Push the new control.
        c = push_control(control(ctx), CONTROL_BLOCK, in_arity, out_arity);
        if (c == NULL)
          goto error;

#if __PASS__
        passes_control_start(ctx, CONTROL_BLOCK);
#endif

        c->label_state.stack_base = stack_height(cache_state(ctx));

        c->instructions = instr->data.block.instructions;
        c->n_instructions = instr->data.block.n_instructions;
        c->cont = 0;

        // Start processing the control block.
        update_compile_state(ctx);

#if SGXWASM_DEBUG_COMPILE
        dump_cache_state_info(cache_state(ctx));
// dump_control_stack(control(ctx));
// dump_cache_state_info(cache_state(ctx));
#endif
#if __OPT_REUSE_REGISTER__ // Reset the register use on else branches.
        set_register_use(ctx, ScratchGP2, MEMREF_NONE, 0);
#endif
        break;
      }
      case OPCODE_LOOP: {
#if SGXWASM_DEBUG_COMPILE
        printf("OPCODE_LOOP\n");
#endif
        uint32_t in_arity = 0;
        uint32_t out_arity =
          instr->data.block.blocktype != VALTYPE_NULL ? 1 : 0;

        // Push the new control.
        c = push_control(control(ctx), CONTROL_LOOP, in_arity, out_arity);
        if (c == NULL)
          goto error;

        c->label_state.stack_base = stack_height(cache_state(ctx));
        // Free all the registers before entering the loop.
        spill_locals(ctx);

#if __PASS__ // Hook at the start of loop (after spill_locals).
        passes_control_start(ctx, CONTROL_LOOP);
#endif

        // Loop labels bind at the beginning of the block.
        low_bind_label(ctx, &c->label);
        // Save the current cache state for the merge when jumping to this loop.
        split_state(&c->label_state, cache_state(ctx));

        // TODO: Stack check.

        c->instructions = instr->data.loop.instructions;
        c->n_instructions = instr->data.loop.n_instructions;
        c->cont = 0;

        // Start processing the control block.
        update_compile_state(ctx);

#if SGXWASM_DEBUG_COMPILE
        dump_cache_state_info(cache_state(ctx));
// dump_control_stack(control(ctx));
// dump_cache_state_info(cache_state(ctx));
#endif
#if __OPT_REUSE_REGISTER__ // Reset the register use on else branches.
        set_register_use(ctx, ScratchGP2, MEMREF_NONE, 0);
#endif
        break;
      }
      case OPCODE_IF: {
#if SGXWASM_DEBUG_COMPILE
        printf("OPCODE_IF\n");
#endif
        uint32_t in_arity = 0;
        uint32_t out_arity = instr->data.if_.blocktype != VALTYPE_NULL ? 1 : 0;

        // Push the new control.
        c = push_control(control(ctx), CONTROL_IF, in_arity, out_arity);
        if (c == NULL)
          goto error;

#if __PASS__
        passes_control_start(ctx, CONTROL_IF);
#endif

        sgxwasm_register_t reg = pop_to_register(ctx, EmptyRegList);

        cond_jmp(ctx, COND_EQ, &c->else_state.label, VALTYPE_I32, reg,
                 REG_UNKNOWN, 0);
        c->label_state.stack_base = stack_height(cache_state(ctx));
        // Store the state (after popping the value) for executing the else
        // branch.
        split_state(&c->else_state.state, cache_state(ctx));
/*printf("Split cache_state:\n");
dump_cache_state_info(cache_state(ctx));
printf("-> else_state:\n");
dump_cache_state_info(&c->else_state.state);*/
#if 0
        emit_lfence(output(ctx));
#endif
        c->instructions = instr->data.if_.instructions_then;
        c->n_instructions = instr->data.if_.n_instructions_then;
        c->cont = 0;

        // Start processing the control block.
        update_compile_state(ctx);

#if SGXWASM_DEBUG_COMPILE
        dump_cache_state_info(cache_state(ctx));
// dump_control_stack(control(ctx));
// dump_cache_state_info(cache_state(ctx));
#endif
#if __OPT_REUSE_REGISTER__ // Reset the register use on else branches.
        set_register_use(ctx, ScratchGP2, MEMREF_NONE, 0);
#endif
        break;
      }
      case OPCODE_ELSE: {
#if SGXWASM_DEBUG_COMPILE
        printf("OPCODE_ELSE\n");
#endif
        assert(control_depth(control(ctx)) != 0);
        assert(is_if(c->type) && !is_else(c->type));

        // if block is reachable, emit_jump
        if (reachable(c)) {
          if (!c->end_reached) {
            init_merge(&c->label_state, cache_state(ctx), num_locals(ctx),
                       c->out_arity);
          }
          // Save current cache_state into label_state
          merge_full_stack_with(ctx, &c->label_state, cache_state(ctx));
          uncond_jmp(ctx, &c->label, 0, Far);
          c->end_reached = 1;
        }

        // TODO: Implement TypeCheckFallThru()
        // if (!TypeCheckFallThru(c)) break;
        c->type = CONTROL_ELSE;
#if __PASS__
        passes_control_start(ctx, CONTROL_ELSE);
#endif

        // Workaround: Allow instrumentation to overwrite label, so no need for
        //             binding as relocation will handle it.
        low_bind_label(ctx, &c->else_state.label);

        // Revert the cache_state to else_state, which contains
        // the inital states before the if starts.
        steal_state(cache_state(ctx), &c->else_state.state);

        struct ControlBlock* prev = control_at(control(ctx), 1);
        c->reachability = inner_reachability(prev);

#if SGXWASM_DEBUG_COMPILE
        dump_cache_state_info(cache_state(ctx));
// dump_control_stack(control(ctx));
// dump_cache_state_info(cache_state(ctx));
#endif
#if __OPT_REUSE_REGISTER__ // Reset the register use on else branches.
        set_register_use(ctx, ScratchGP2, MEMREF_NONE, 0);
#endif
        break;
      }
      case OPCODE_END: {
#if SGXWASM_DEBUG_COMPILE
        printf("OPCODE_END\n");
#endif
        assert(control_depth(control(ctx)) != 0);
        if (control_depth(control(ctx)) == 1) {
          // If at the last (implicit) control, check we are at end.
          // assert(c->cont == c->n_instructions);
          assert(instr_id(ctx) == num_instrs(ctx) - 1);
#if __PASS__
          passes_function_end(ctx);
#endif
          // Do return.
          return_impl(ctx);
          control_shrink(control(ctx));
#if SGXWASM_DEBUG_COMPILE
          printf("RETURN (implicit)\n");
#endif
          break;
        }

        pop_control(ctx, c);

        int is_if = is_onearmed_if(c->type);
        int parent_reached = reachable(c) || c->end_reached || is_if;

        // Handling the one-armed if case here to make sure
        // we are hooking the actual end of the control block.
        if (is_if) {
#if __PASS__ // Enfore adding a else as we have state merging
             // code only for else case.
          passes_control_start(ctx, CONTROL_ELSE);
#endif
          post_finish_one_armed_if(ctx, c);
        }
#if __PASS__ // Workaround: Make the hook after the state merging,
             //             ensuring no instructions after the
             //             hooked instructions.
        passes_control_end(ctx, c->type);
#endif
        // bind label.
        low_bind_label(ctx, &c->label);

        // Pop the control block.
        free_control_block(c);
        control_shrink(control(ctx));

        int depth = control_depth(control(ctx));
        assert(depth > 0);
        // Point to the current top of the stack.
        c = control_at(control(ctx), 0);
        // If the parent block was reachable before, but the popped control does
        // not return to here, this block becomes "spec only reachable".
        if (!parent_reached && reachable(c)) {
          c->reachability = SpecOnlyReachable;
        }
        update_compile_state(ctx);

#if SGXWASM_DEBUG_COMPILE
        dump_cache_state_info(cache_state(ctx));
// dump_control_stack(control(ctx));
// dump_cache_state_info(cache_state(ctx));
#endif
#if __OPT_REUSE_REGISTER__ // Reset the register use on else branches.
        set_register_use(ctx, ScratchGP2, MEMREF_NONE, 0);
#endif
        break;
      }
      case OPCODE_RETURN: {
#if SGXWASM_DEBUG_COMPILE
        printf("OPCODE_RETURN\n");
#endif
#if __PASS__
        if ((instr_id(ctx) == num_instrs(ctx) - 2) &&
            (control_depth(control(ctx)) == 1)) {
          assert(instr_list(ctx)[instr_id(ctx) + 1].opcode == OPCODE_END);
          passes_function_end(ctx);
        }
#endif
        return_impl(ctx);
        end_control(c);
        // Work-around: Function ends with return, pop the control.
        // fun:
        //     ...
        // return
        // end     <--- end of encoding
        if ((instr_id(ctx) == num_instrs(ctx) - 2) &&
            (control_depth(control(ctx)) == 1)) {
          // The last instruction should always be the end.
          assert(instr_list(ctx)[instr_id(ctx) + 1].opcode == OPCODE_END);
          // Pop the control block.
          free_control_block(c);
          control_shrink(control(ctx));
        }

#if SGXWASM_DEBUG_COMPILE
        dump_cache_state_info(cache_state(ctx));
// dump_control_stack(control(ctx));
// dump_cache_state_info(cache_state(ctx));
#endif
        break;
      }
      case OPCODE_SELECT: {
        reglist_t pinned = 0;
        sgxwasm_register_t condition = pop_to_register(ctx, EmptyRegList);
        assert(is_gp(condition));
        set(&pinned, condition);
        // Validation: the types of first two values on the stack are the same.
        size_t stack_size = stack_height(cache_state(ctx));
        struct StackSlot* fslot =
          &cache_state(ctx)->stack_state->data[stack_size - 1];
        sgxwasm_valtype_t type = fslot->type;
        struct StackSlot* tslot =
          &cache_state(ctx)->stack_state->data[stack_size - 2];
        assert(type == tslot->type);
        sgxwasm_register_t false_value = pop_to_register(ctx, pinned);
        set(&pinned, false_value);
        sgxwasm_register_t true_value = pop_to_register(ctx, pinned);
        // Get an unused register given a try_first register set.
        reglist_t try_first = 0;
        set(&try_first, false_value);
        set(&try_first, true_value);
        sgxwasm_register_t dst = get_unused_register_with_class(
          ctx, reg_class(true_value), try_first, EmptyRegList);
        push_register(ctx, type, dst);

        // Now emit the actual code to move either {true_value} or {false_value}
        // into {dst}.
        label_t cont = { 0, 0 };
        label_t case_false = { 0, 0 };
        num_low_instrs(ctx) +=
          emit_cond_jump_rr(output(ctx), COND_EQ, &case_false, VALTYPE_I32,
                            condition, REG_UNKNOWN);
        if (dst != true_value) {
          num_low_instrs(ctx) += Move(output(ctx), dst, true_value, type);
        }
        num_low_instrs(ctx) += emit_jmp_label(output(ctx), &cont, Far);

        bind_label(output(ctx), &case_false, pc_offset(output(ctx)));
        if (dst != false_value) {
          num_low_instrs(ctx) += Move(output(ctx), dst, false_value, type);
        }
        bind_label(output(ctx), &cont, pc_offset(output(ctx)));
#if __OPT_REUSE_REGISTER__ // Reset the register use on else branches.
        set_register_use(ctx, ScratchGP2, MEMREF_NONE, 0);
#endif
        break;
      }
      case OPCODE_BR: {
        uint32_t label_index = instr->data.br.labelidx;
        assert(label_index < (uint32_t)control_depth(control(ctx)));
        struct ControlBlock* target = control_at(control(ctx), label_index);
        if (label_index == (uint32_t)(control_depth(control(ctx)) - 1)) {
          // Do return.
          return_impl(ctx);
        } else if (reachable(c)) {
          br_impl(ctx, target, label_index);
          if (is_loop(target->type)) {
            target->start_reached = 1;
          } else {
            target->end_reached = 1;
          }
        }
        end_control(c);
#if __OPT_REUSE_REGISTER__ // Reset the register use on else branches.
        set_register_use(ctx, ScratchGP2, MEMREF_NONE, 0);
#endif
        break;
      }
      case OPCODE_BR_IF: {
        uint32_t label_index = instr->data.br_if.labelidx;
        assert(label_index < (uint32_t)control_depth(control(ctx)));
        if (reachable(c)) {
          label_t cont_false = { 0, 0 };
          sgxwasm_register_t value = pop_to_register(ctx, EmptyRegList);
          num_low_instrs(ctx) += emit_cond_jump_rr(
            output(ctx), COND_EQ, &cont_false, VALTYPE_I32, value, REG_UNKNOWN);
          br_or_ret(ctx, label_index);
          bind_label(output(ctx), &cont_false, pc_offset(output(ctx)));
          struct ControlBlock* target = control_at(control(ctx), label_index);
          if (is_loop(target->type)) {
            target->start_reached = 1;
          } else {
            target->end_reached = 1;
          }
        }
        break;
      }
      case OPCODE_BR_TABLE: {
        uint32_t table_count = instr->data.br_table.n_labelidxs;
        uint32_t* table = instr->data.br_table.labelidxs;
        uint32_t default_index = instr->data.br_table.labelidx;
        uint32_t i;

        // Validation.
        assert(default_index < (uint32_t)control_depth(control(ctx)));
        struct ControlBlock* target = control_at(control(ctx), default_index);
        uint32_t br_arity =
          is_loop(target->type) ? target->in_arity : target->out_arity;
        for (i = 0; i < table_count; i++) {
          uint32_t index = table[i];
          assert(index < (uint32_t)control_depth(control(ctx)));
          target = control_at(control(ctx), index);
          uint32_t arity =
            is_loop(target->type) ? target->in_arity : target->out_arity;
          if (arity != br_arity) {
            assert(0);
          }
        }

        if (reachable(c)) {
          reglist_t pinned = 0;
          sgxwasm_register_t value = pop_to_register(ctx, EmptyRegList);
          set(&pinned, value);
          uint32_t iterator = 0;
          label_t* br_targets =
            malloc(sizeof(label_t) * control_depth(control(ctx)));
          for (i = 0; i < (uint32_t)control_depth(control(ctx)); i++) {
            br_targets[i].pos = 0;
            br_targets[i].near_link_pos = 0;
          }

          if (table_count > 0) {
            sgxwasm_register_t tmp =
              get_unused_register_with_class(ctx, GP_REG, EmptyRegList, pinned);
            load_const_to_reg(ctx, tmp, table_count, VALTYPE_I32);
            label_t case_default = { 0, 0 };

            // emit_cond_jump_rr(
            //  output(ctx), COND_GE_U, &case_default, VALTYPE_I32, value, tmp);

            br_table_cond_jmp(ctx, COND_GE_U, &case_default, VALTYPE_I32, value,
                              tmp, 0, 0);

            generate_br_table(ctx, tmp, value, 0 /* min */,
                              table_count /* max */, table, &iterator,
                              table_count, br_targets);
            br_table_bind(ctx, &case_default, BrTableTarget, 0, 0, 0);
          }

          // Generate the default case.
          generate_br_case(ctx, default_index, br_targets);

          for (i = 0; i < (uint32_t)control_depth(control(ctx)); i++) {
            if (!is_bound(&br_targets[i])) {
              continue;
            }
            struct ControlBlock* target = control_at(control(ctx), i);
            if (is_loop(target->type)) {
              target->start_reached = 1;
            } else {
              target->end_reached = 1;
            }
          }
          free(br_targets);
        }
        end_control(c);
#if __OPT_REUSE_REGISTER__ // Reset the register use on else branches.
        set_register_use(ctx, ScratchGP2, MEMREF_NONE, 0);
#endif
        break;
      }
      case OPCODE_GET_LOCAL: {
        size_t index = instr->data.get_local.localidx;
        get_local(ctx, index);
        break;
      }
      case OPCODE_SET_LOCAL: {
        size_t index = instr->data.set_local.localidx;
        set_local_impl(ctx, index, 0 /* not tee */);
        break;
      }
      case OPCODE_TEE_LOCAL: {
        size_t index = instr->data.tee_local.localidx;
        set_local_impl(ctx, index, 1 /* is tee*/);
        break;
      }
      case OPCODE_GET_GLOBAL: {
        size_t index = instr->data.get_global.globalidx;
        reglist_t pinned = 0;
        size_t offset = 0;
        sgxwasm_valtype_t type = module_types->globaltypes[index].valtype;
        load_type_t load_type = get_load_type(type);
        sgxwasm_register_t addr, value;

        addr = get_global_addr(ctx, index, &pinned, &offset);
        // Temporily pin the register to avoid the same register
        // being allocated.
        assert(addr != REG_UNKNOWN);

        value = get_unused_register_with_class(ctx, reg_class_for(type),
                                               EmptyRegList, pinned);
        num_low_instrs(ctx) += Load(output(ctx), value, addr, REG_UNKNOWN,
                                    offset, load_type, NULL, 1);
        push_register(ctx, type, value);

        break;
      }
      case OPCODE_SET_GLOBAL: {
        size_t index = instr->data.set_global.globalidx;
        reglist_t pinned = 0;
        size_t offset = 0;
        sgxwasm_valtype_t type = module_types->globaltypes[index].valtype;
        store_type_t store_type = get_store_type(type);
        sgxwasm_register_t addr, reg;

        addr = get_global_addr(ctx, index, &pinned, &offset);

        reg = pop_to_register(ctx, pinned);

        num_low_instrs(ctx) += Store(output(ctx), addr, REG_UNKNOWN, offset,
                                     reg, store_type, NULL, 1);
        break;
      }
      case OPCODE_I32_LOAD: {
        size_t offset = instr->data.i32_load.offset;
        load_mem(ctx, I32Load, offset);
        break;
      }
      case OPCODE_I64_LOAD: {
        size_t offset = instr->data.i64_load.offset;
        load_mem(ctx, I64Load, offset);
        break;
      }
      case OPCODE_F32_LOAD: {
        size_t offset = instr->data.f32_load.offset;
        load_mem(ctx, F32Load, offset);
        break;
      }
      case OPCODE_F64_LOAD: {
        size_t offset = instr->data.f64_load.offset;
        load_mem(ctx, F64Load, offset);
        break;
      }
      case OPCODE_I32_LOAD8_S: {
        size_t offset = instr->data.i32_load8_s.offset;
        load_mem(ctx, I32Load8S, offset);
        break;
      }
      case OPCODE_I32_LOAD8_U: {
        size_t offset = instr->data.i32_load8_u.offset;
        load_mem(ctx, I32Load8U, offset);
        break;
      }
      case OPCODE_I32_LOAD16_S: {
        size_t offset = instr->data.i32_load16_s.offset;
        load_mem(ctx, I32Load16S, offset);
        break;
      }
      case OPCODE_I32_LOAD16_U: {
        size_t offset = instr->data.i32_load16_u.offset;
        load_mem(ctx, I32Load16U, offset);
        break;
      }
      case OPCODE_I64_LOAD8_S: {
        size_t offset = instr->data.i64_load8_s.offset;
        load_mem(ctx, I64Load8S, offset);
        break;
      }
      case OPCODE_I64_LOAD8_U: {
        size_t offset = instr->data.i64_load8_u.offset;
        load_mem(ctx, I64Load8U, offset);
        break;
      }
      case OPCODE_I64_LOAD16_S: {
        size_t offset = instr->data.i64_load16_s.offset;
        load_mem(ctx, I64Load16S, offset);
        break;
      }
      case OPCODE_I64_LOAD16_U: {
        size_t offset = instr->data.i64_load16_u.offset;
        load_mem(ctx, I64Load16U, offset);
        break;
      }
      case OPCODE_I64_LOAD32_S: {
        size_t offset = instr->data.i64_load32_s.offset;
        load_mem(ctx, I64Load32S, offset);
        break;
      }
      case OPCODE_I64_LOAD32_U: {
        size_t offset = instr->data.i64_load32_u.offset;
        load_mem(ctx, I64Load32U, offset);
        break;
      }
      case OPCODE_I32_STORE: {
        size_t offset = instr->data.i32_store.offset;
        store_mem(ctx, I32Store, offset);
        break;
      }
      case OPCODE_I64_STORE: {
        size_t offset = instr->data.i64_store.offset;
        store_mem(ctx, I64Store, offset);
        break;
      }
      case OPCODE_F32_STORE: {
        size_t offset = instr->data.f32_store.offset;
        store_mem(ctx, F32Store, offset);
        break;
      }
      case OPCODE_F64_STORE: {
        size_t offset = instr->data.f64_store.offset;
        store_mem(ctx, F64Store, offset);
        break;
      }
      case OPCODE_I32_STORE8: {
        size_t offset = instr->data.i32_store8.offset;
        store_mem(ctx, I32Store8, offset);
        break;
      }
      case OPCODE_I32_STORE16: {
        size_t offset = instr->data.i32_store16.offset;
        store_mem(ctx, I32Store16, offset);
        break;
      }
      case OPCODE_I64_STORE8: {
        size_t offset = instr->data.i64_store8.offset;
        store_mem(ctx, I64Store8, offset);
        break;
      }
      case OPCODE_I64_STORE16: {
        size_t offset = instr->data.i64_store16.offset;
        store_mem(ctx, I64Store16, offset);
        break;
      }
      case OPCODE_I64_STORE32: {
        size_t offset = instr->data.i64_store32.offset;
        store_mem(ctx, I64Store32, offset);
        break;
      }
      case OPCODE_CALL: {
        uint32_t fun_index = instr->data.call.funcidx;
        assert(fun_index < num_funs);
        call_direct(ctx, module_types, fun_index);
#if __OPT_REUSE_REGISTER__ // Reset the register use upon calls.
        set_register_use(ctx, ScratchGP2, MEMREF_NONE, 0);
#endif
        break;
      }
      case OPCODE_CALL_INDIRECT: {
        uint32_t type_index = instr->data.call_indirect.typeidx;
        call_indirect(ctx, type_table, type_index);
#if __OPT_REUSE_REGISTER__ // Reset the register use upon calls.
        set_register_use(ctx, ScratchGP2, MEMREF_NONE, 0);
#endif
        break;
      }
      case OPCODE_I32_CONST: {
        uint32_t val = instr->data.i32_const.value;
        push_const(ctx, VALTYPE_I32, (int32_t)val);
        break;
      }
      case OPCODE_I64_CONST: {
        // We only store value that is type of int32_t into the cache stack.
        uint64_t val = instr->data.i64_const.value;
        int32_t val_i32 = (int32_t)val;
        if (val_i32 == (int64_t)val) {
          push_const(ctx, VALTYPE_I64, (int32_t)val);
        } else {
          sgxwasm_register_t reg = get_unused_register_with_class(
            ctx, reg_class_for(VALTYPE_I64), EmptyRegList, EmptyRegList);
          load_const_to_reg(ctx, reg, val, VALTYPE_I64);
          push_register(ctx, VALTYPE_I64, reg);
        }
        break;
      }
      case OPCODE_F32_CONST: {
        float val = instr->data.f32_const.value;
        sgxwasm_register_t reg = get_unused_register_with_class(
          ctx, reg_class_for(VALTYPE_F32), EmptyRegList, EmptyRegList);
        load_const_to_reg(ctx, reg, f32_encoding(val), VALTYPE_F32);
        push_register(ctx, VALTYPE_F32, reg);
        break;
      }
      case OPCODE_F64_CONST: {
        double val = instr->data.f64_const.value;
        sgxwasm_register_t reg = get_unused_register_with_class(
          ctx, reg_class_for(VALTYPE_F64), EmptyRegList, EmptyRegList);
        load_const_to_reg(ctx, reg, f64_encoding(val), VALTYPE_F64);
        push_register(ctx, VALTYPE_F64, reg);
        break;
      }
      case OPCODE_DROP: {
        drop(ctx);
        break;
      }
      case OPCODE_I32_ADD:
      case OPCODE_I32_SUB:
      case OPCODE_I32_MUL:
      case OPCODE_I32_AND:
      case OPCODE_I32_OR:
      case OPCODE_I32_XOR: {
        EmitBinOp(ctx, VALTYPE_I32, VALTYPE_I32, opcode);
        break;
      }
      case OPCODE_I32_DIV_U:
      case OPCODE_I32_DIV_S:
      case OPCODE_I32_REM_U:
      case OPCODE_I32_REM_S: {
        EmitBinOp(ctx, VALTYPE_I32, VALTYPE_I32, opcode);
        break;
      }
      case OPCODE_I32_EQZ:
      case OPCODE_I32_CLZ:
      case OPCODE_I32_CTZ:
      case OPCODE_I32_POPCNT: {
        EmitUnOp(ctx, VALTYPE_I32, VALTYPE_I32, opcode);
        break;
      }
      case OPCODE_I64_ADD:
      case OPCODE_I64_SUB:
      case OPCODE_I64_MUL:
      case OPCODE_I64_AND:
      case OPCODE_I64_OR:
      case OPCODE_I64_XOR: {
        EmitBinOp(ctx, VALTYPE_I64, VALTYPE_I64, opcode);
        break;
      }
      case OPCODE_I64_DIV_U:
      case OPCODE_I64_DIV_S:
      case OPCODE_I64_REM_U:
      case OPCODE_I64_REM_S: {
        EmitBinOp(ctx, VALTYPE_I64, VALTYPE_I64, opcode);
        break;
      }
      case OPCODE_I64_EQZ: {
        EmitUnOp(ctx, VALTYPE_I64, VALTYPE_I32, opcode);
        break;
      }
      case OPCODE_I64_CLZ:
      case OPCODE_I64_CTZ:
      case OPCODE_I64_POPCNT: {
        EmitUnOp(ctx, VALTYPE_I64, VALTYPE_I64, opcode);
        break;
      }
      case OPCODE_I32_EQ:
      case OPCODE_I32_NE:
      case OPCODE_I32_LT_S:
      case OPCODE_I32_LT_U:
      case OPCODE_I32_GT_S:
      case OPCODE_I32_GT_U:
      case OPCODE_I32_LE_S:
      case OPCODE_I32_LE_U:
      case OPCODE_I32_GE_S:
      case OPCODE_I32_GE_U: {
        EmitBinOp(ctx, VALTYPE_I32, VALTYPE_I32, opcode);
        break;
      }
      case OPCODE_I64_EQ:
      case OPCODE_I64_NE:
      case OPCODE_I64_LT_S:
      case OPCODE_I64_LT_U:
      case OPCODE_I64_GT_S:
      case OPCODE_I64_GT_U:
      case OPCODE_I64_LE_S:
      case OPCODE_I64_LE_U:
      case OPCODE_I64_GE_S:
      case OPCODE_I64_GE_U: {
        EmitBinOp(ctx, VALTYPE_I64, VALTYPE_I32, opcode);
        break;
      }
      case OPCODE_F32_EQ:
      case OPCODE_F32_NE:
      case OPCODE_F32_LT:
      case OPCODE_F32_GT:
      case OPCODE_F32_LE:
      case OPCODE_F32_GE: {
        EmitBinOp(ctx, VALTYPE_F32, VALTYPE_I32, opcode);
        break;
      }
      case OPCODE_F64_EQ:
      case OPCODE_F64_NE:
      case OPCODE_F64_LT:
      case OPCODE_F64_GT:
      case OPCODE_F64_LE:
      case OPCODE_F64_GE: {
        EmitBinOp(ctx, VALTYPE_F64, VALTYPE_I32, opcode);
        break;
      }
      case OPCODE_I32_SHL:
      case OPCODE_I32_SHR_U:
      case OPCODE_I32_SHR_S:
      case OPCODE_I32_ROTL:
      case OPCODE_I32_ROTR: {
        EmitBinOp(ctx, VALTYPE_I32, VALTYPE_I32, opcode);
        break;
      }
      case OPCODE_I64_SHL:
      case OPCODE_I64_SHR_U:
      case OPCODE_I64_SHR_S:
      case OPCODE_I64_ROTL:
      case OPCODE_I64_ROTR: {
        EmitBinOp(ctx, VALTYPE_I64, VALTYPE_I64, opcode);
        break;
      }
      case OPCODE_F32_ADD:
      case OPCODE_F32_SUB:
      case OPCODE_F32_MUL:
      case OPCODE_F32_DIV:
      case OPCODE_F32_MIN:
      case OPCODE_F32_MAX:
      case OPCODE_F32_COPYSIGN: {
        EmitBinOp(ctx, VALTYPE_F32, VALTYPE_F32, opcode);
        break;
      }
      case OPCODE_F32_ABS:
      case OPCODE_F32_NEG:
      case OPCODE_F32_CEIL:
      case OPCODE_F32_FLOOR:
      case OPCODE_F32_TRUNC:
      case OPCODE_F32_NEAREST:
      case OPCODE_F32_SQRT: {
        EmitUnOp(ctx, VALTYPE_F32, VALTYPE_F32, opcode);
        break;
      }
      case OPCODE_F64_ADD:
      case OPCODE_F64_SUB:
      case OPCODE_F64_MUL:
      case OPCODE_F64_DIV:
      case OPCODE_F64_MIN:
      case OPCODE_F64_MAX:
      case OPCODE_F64_COPYSIGN: {
        EmitBinOp(ctx, VALTYPE_F64, VALTYPE_F64, opcode);
        break;
      }
      case OPCODE_F64_ABS:
      case OPCODE_F64_NEG:
      case OPCODE_F64_CEIL:
      case OPCODE_F64_FLOOR:
      case OPCODE_F64_TRUNC:
      case OPCODE_F64_NEAREST:
      case OPCODE_F64_SQRT: {
        EmitUnOp(ctx, VALTYPE_F64, VALTYPE_F64, opcode);
        break;
      }
      case OPCODE_I32_WRAP_I64: {
        EmitTypeConversion(ctx, VALTYPE_I32, VALTYPE_I64, opcode, 0, 0);
        break;
      }
      case OPCODE_I32_TRUNC_S_F32: {
        EmitTypeConversion(ctx, VALTYPE_I32, VALTYPE_F32, opcode, 1,
                           pc_offset(output(ctx)));
        break;
      }
      case OPCODE_I32_TRUNC_U_F32: {
        EmitTypeConversion(ctx, VALTYPE_I32, VALTYPE_F32, opcode, 1,
                           pc_offset(output(ctx)));
        break;
      }
      case OPCODE_I32_TRUNC_S_F64: {
        EmitTypeConversion(ctx, VALTYPE_I32, VALTYPE_F64, opcode, 1,
                           pc_offset(output(ctx)));
        break;
      }
      case OPCODE_I32_TRUNC_U_F64: {
        EmitTypeConversion(ctx, VALTYPE_I32, VALTYPE_F64, opcode, 1,
                           pc_offset(output(ctx)));
        break;
      }
      case OPCODE_I64_EXTEND_S_I32: {
        EmitTypeConversion(ctx, VALTYPE_I64, VALTYPE_I32, opcode, 0, 0);
        break;
      }
      case OPCODE_I64_EXTEND_U_I32: {
        EmitTypeConversion(ctx, VALTYPE_I64, VALTYPE_I32, opcode, 0, 0);
        break;
      }
      case OPCODE_I64_TRUNC_S_F32: {
        EmitTypeConversion(ctx, VALTYPE_I64, VALTYPE_F32, opcode, 1,
                           pc_offset(output(ctx)));
        break;
      }
      case OPCODE_I64_TRUNC_U_F32: {
        EmitTypeConversion(ctx, VALTYPE_I64, VALTYPE_F32, opcode, 1,
                           pc_offset(output(ctx)));
        break;
      }
      case OPCODE_I64_TRUNC_S_F64: {
        EmitTypeConversion(ctx, VALTYPE_I64, VALTYPE_F64, opcode, 1,
                           pc_offset(output(ctx)));
        break;
      }
      case OPCODE_I64_TRUNC_U_F64: {
        EmitTypeConversion(ctx, VALTYPE_I64, VALTYPE_F64, opcode, 1,
                           pc_offset(output(ctx)));
        break;
      }
      case OPCODE_F32_CONVERT_S_I32: {
        EmitTypeConversion(ctx, VALTYPE_F32, VALTYPE_I32, opcode, 0, 0);
        break;
      }
      case OPCODE_F32_CONVERT_U_I32: {
        EmitTypeConversion(ctx, VALTYPE_F32, VALTYPE_I32, opcode, 0, 0);
        break;
      }
      case OPCODE_F32_CONVERT_S_I64: {
        EmitTypeConversion(ctx, VALTYPE_F32, VALTYPE_I64, opcode, 0, 0);
        break;
      }
      case OPCODE_F32_CONVERT_U_I64: {
        EmitTypeConversion(ctx, VALTYPE_F32, VALTYPE_I64, opcode, 0, 0);
        break;
      }
      case OPCODE_F32_DEMOTE_F64: {
        EmitTypeConversion(ctx, VALTYPE_F32, VALTYPE_F64, opcode, 0, 0);
        break;
      }
      case OPCODE_F64_CONVERT_S_I32: {
        EmitTypeConversion(ctx, VALTYPE_F64, VALTYPE_I32, opcode, 0, 0);
        break;
      }
      case OPCODE_F64_CONVERT_U_I32: {
        EmitTypeConversion(ctx, VALTYPE_F64, VALTYPE_I32, opcode, 0, 0);
        break;
      }
      case OPCODE_F64_CONVERT_S_I64: {
        EmitTypeConversion(ctx, VALTYPE_F64, VALTYPE_I64, opcode, 0, 0);
        break;
      }
      case OPCODE_F64_CONVERT_U_I64: {
        EmitTypeConversion(ctx, VALTYPE_F64, VALTYPE_I64, opcode, 0, 0);
        break;
      }
      case OPCODE_F64_PROMOTE_F32: {
        EmitTypeConversion(ctx, VALTYPE_F64, VALTYPE_F32, opcode, 0, 0);
        break;
      }
      case OPCODE_I32_REINTERPRET_F32: {
        EmitTypeConversion(ctx, VALTYPE_I32, VALTYPE_F32, opcode, 0, 0);
        break;
      }
      case OPCODE_I64_REINTERPRET_F64: {
        EmitTypeConversion(ctx, VALTYPE_I64, VALTYPE_F64, opcode, 0, 0);
        break;
      }
      case OPCODE_F32_REINTERPRET_I32: {
        EmitTypeConversion(ctx, VALTYPE_F32, VALTYPE_I32, opcode, 0, 0);
        break;
      }
      case OPCODE_F64_REINTERPRET_I64: {
        EmitTypeConversion(ctx, VALTYPE_F64, VALTYPE_I64, opcode, 0, 0);
        break;
      }
      case OPCODE_NOP: {
        break;
      }
      case OPCODE_UNREACHABLE: {
// Case of deadcode, terminates the compilation.
#if SGXWASM_DEBUG_COMPILE
        printf("unreachable, control: %zu\n", control_depth(control(ctx)));
#endif
#if 0
        if (control_depth(control(ctx)) == 1) {
          // Pop the control block.
          return_impl(ctx);
          free_control_block(c);
          control_shrink(control(ctx));
          break;
        }
        struct ControlBlock* prev = control_at(control(ctx), 1);
        if (prev->type == CONTROL_BLOCK) {
          control_shrink(control(ctx));
          control_shrink(control(ctx));
          break;
        }
#endif
        if (!is_else(c->type)) {
          end_control(c);
        }
        // TODO: trap
        break;
      }
      case OPCODE_MEMORY_SIZE: {
        uint32_t size = ctx->min_memory_size / 65536;
        push_const(ctx, VALTYPE_I32, size);
        break;
      }
      case OPCODE_MEMORY_GROW: { // XXX: Check-needed
        sgxwasm_register_t value = pop_to_register(ctx, EmptyRegList);
        if (ctx->max_memory_size == 0) {
          uint32_t size = ctx->min_memory_size / 65536;
          push_const(ctx, VALTYPE_I32, size);
        } else {
          sgxwasm_register_t page_size = get_unused_register_with_class(
            ctx, GP_REG, EmptyRegList, EmptyRegList);
          size_t max_size = ctx->max_memory_size / 65536;
          label_t els = { 0, 0 };
          label_t end = { 0, 0 };
          emit_cmp_ri(output(ctx), value, max_size, VALTYPE_I32);
          emit_jcc(output(ctx), COND_GT_U, &els, Near);
          push_register(ctx, VALTYPE_I32, value);
          emit_jmp_label(output(ctx), &end, Near);
          bind_label(output(ctx), &els, pc_offset(output(ctx)));
          push_const(ctx, VALTYPE_I32, -1);
          bind_label(output(ctx), &end, pc_offset(output(ctx)));
        }
        break;
      }
      default:
        printf("Unsupported op: 0x%x\n", opcode);
        break;
    } // end of switch.
#if __PASS__
    passes_instruction_end(ctx);
#endif
  }

  if (0) {
  error:
    return 0;
  }

  return 1;
}

static int
process_parameters(struct CompilerContext* ctx,
                   struct LocationSignature* fun_sig, sgxwasm_valtype_t type,
                   size_t index)
{
  sgxwasm_register_class_t rc = reg_class_for(type);
  sgxwasm_register_t reg = get_first_reg_set(get_cache_reg_list(GP_REG));

  struct LinkageLocation* param = get_param(fun_sig, index);

  if (is_reg(param->loc)) {
    if (rc == GP_REG) {
      reg = param->gp;
    } else {
      assert(rc == FP_REG);
      reg = param->fp;
    }
  } else if (is_stack(param->loc)) {
    // TODO: Test stack-based parameter passing.
    reg = get_unused_register_with_class(ctx, rc, EmptyRegList, EmptyRegList);
    num_low_instrs(ctx) +=
      LoadCallerFrameSlot(output(ctx), reg, param->stack_offset, type);
  }
  push_register(ctx, type, reg);

  return 1;
}

static uint64_t
prepare_stack_frame(struct CompilerContext* ctx)
{
  uint64_t offset;
  num_low_instrs(ctx) += emit_pushq_r(output(ctx), GP_RBP);
  num_low_instrs(ctx) += Move(output(ctx), GP_RBP, GP_RSP, VALTYPE_I64);
  num_low_instrs(ctx) += emit_subq_sp_32(output(ctx), 0);
  offset = pc_offset(output(ctx)) - 4;
  return offset;
}

static void
patch_stack_frame(struct CompilerContext* ctx, uint64_t offset, uint32_t size)
{
  size_t i;
  uint32_t bytes = size * 8;

  if ((bytes % StackAlignment) != 0) {
    bytes += StackAlignment - bytes % StackAlignment;
  }
  for (i = 0; i < 4; i++) {
    set_byte_at(output(ctx), offset + i, bytes & 0xff);
    bytes = bytes >> 8;
  }
}

char*
sgxwasm_compile_function(struct PassManager* pm,
                         const struct FuncTypeVector* type_table,
                         const struct ModuleTypes* module_types,
                         const struct Function* func, const struct Memory* mem,
                         size_t num_funs, const struct CodeSectionCode* code,
                         struct MemoryReferences* memrefs, size_t* out_size,
                         size_t* stack_usage, unsigned flags)
{
  struct CompilerContext ctx;
  const struct FuncType* fun_type = &func->type;
  struct StackState sstack = { 0, 0, NULL, 0 };
  size_t n_frame_locals = 0; // XXX: Check later
  uint32_t n_locals = 0;
  char* out;
  uint64_t pc_offset_stack_frame;
  reglist_t used_registers = get_allocable_reg_list();
  uint32_t register_use_count[RegisterNum] = { 0 };
  reglist_t last_spilled_regs = 0;

#if SGXWASM_DEBUG_COMPILE
  printf("Compile function #%zu\n", func->fun_index);
  printf("------COMPILE FUNCTION START------\n");
#endif

  // Initialize allocable list of registers.
  init_allocable_reg_list();
  {
    size_t i;
    n_locals = fun_type->n_inputs;
    for (i = 0; i < code->n_locals; ++i) {
      n_locals += code->locals[i].count;
    }
  }

  init_compiler_context(&ctx, func, memrefs, pm, &sstack, &used_registers,
                        register_use_count, &last_spilled_regs, fun_type,
                        n_locals, mem);

#if __PASS__ // Hooking point for the start of the function.
  passes_function_start(&ctx);
#endif

  // Implement vritual stack from liftoff.

  // Prepare stack frame.
  pc_offset_stack_frame = prepare_stack_frame(&ctx);

  // Process input parameters.
  {
    size_t i, input_index = 0;
    struct ParameterAllocator params;
    size_t parameter_count = fun_type->n_inputs;
    struct LocationSignature fun_sig = { 0, 0, NULL };

    init_parameter_allocator(&params);
    // Build the function signature.
    for (i = 0; i < parameter_count; i++) {
      sgxwasm_valtype_t type = fun_type->input_types[i];
      sgxwasm_register_class_t rc = reg_class_for(type);

      if (rc == GP_REG && can_allocate_gp(&params)) {
        sgxwasm_register_t reg = next_gp_reg(&params);
        add_param_at(&fun_sig, i, GP_REG, LOC_REGISTER, reg);
      } else if (rc == FP_REG && can_allocate_fp(&params)) {
        sgxwasm_register_t reg = next_fp_reg(&params);
        add_param_at(&fun_sig, i, FP_REG, LOC_REGISTER, reg);
      } else {
        uint32_t stack_offset = next_stack_slot(&params);
        add_param_at(&fun_sig, i, rc, LOC_STACK, stack_offset);
      }
    }

    for (i = 0; i < parameter_count; ++i) {
      sgxwasm_valtype_t type = fun_type->input_types[i];
      input_index += process_parameters(&ctx, &fun_sig, type, i);
    }
    assert(input_index == fun_type->n_inputs);
  }

  // Process the rest of local variables.
  {
    size_t i;
    for (i = 0; i < code->n_locals; ++i) {
      sgxwasm_valtype_t valtype = code->locals[i].valtype;
      size_t j;
      for (j = 0; j < code->locals[i].count; j++) {
        switch (valtype) {
          case VALTYPE_I32:
#if __OPT_INIT_STACK__
            push_stack(cache_state(&ctx), VALTYPE_I32, LOC_STACK, REG_UNKNOWN,
                       0);
#else
            push_const(&ctx, VALTYPE_I32, 0);
#endif
            break;
          case VALTYPE_I64:
#if __OPT_INIT_STACK__
            push_stack(cache_state(&ctx), VALTYPE_I64, LOC_STACK, REG_UNKNOWN,
                       0);
#else
            push_const(&ctx, VALTYPE_I64, 0);
#endif
            break;
          case VALTYPE_F32:
          case VALTYPE_F64: {
            sgxwasm_register_t reg = get_unused_register_with_class(
              &ctx, FP_REG, EmptyRegList, EmptyRegList);
            if (reg == REG_UNKNOWN)
              assert(0);
            load_const_to_reg(&ctx, reg, 0, VALTYPE_F64);
            push_register(&ctx, valtype, reg);
            break;
          }
          default:
            // Unsupported type.
            break;
        }
      }
    }
#if __OPT_INIT_STACK__
    spill_register(&ctx, GP_RDI);
    spill_register(&ctx, GP_RCX);
    InitStack(output(&ctx), n_locals - 1, n_locals - fun_type->n_inputs);
#endif
    assert(n_locals == stack_height(cache_state(&ctx)));
  }
  // End of virtual stack implementation.

  // Compile function body.
  if (!sgxwasm_compile_function_body(&ctx, type_table, module_types, num_funs,
                                     n_frame_locals, code->instructions,
                                     code->n_instructions, stack_usage, flags))
    goto error;

  // Patch the stack frame.
  // XXX: liftoff adds n_locals to num_used_spill_slots.
  // Since num_used_spill_slots already takes n_locals into
  // account, we just use it alone.
  patch_stack_frame(&ctx, pc_offset_stack_frame,
                    *num_used_spill_slots(&ctx) + 1);

// XXX: Debugging memory usage.
// dump_memory_usage(&ctx);

#if SGXWASM_DEBUG_COMPILE
  dump_output(output(&ctx), 0);
#endif

  if (0) {
  error:
    free(output(&ctx)->data);
    out = NULL;
  } else {
    out = output(&ctx)->data;
    if (out_size) {
      *out_size = output(&ctx)->size;
    }
  }
  stack_free(&sstack);
  free_compiler_context(&ctx);
  return out;
}
