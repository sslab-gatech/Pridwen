#include <sgxwasm/pass.h>
#include <sgxwasm/pass_cfg.h>

/*
 T-SGX scheme:
 main:
   leaq next_address, r15
   jmp sb.start
   ...
   jmp sb.end

bb:
  ...
  leaq next_address, r15
  jmp sb.next

  ...
  xend
  call out
  leaq next_address, r15
  jmp sb.start
*/

#if __TSGX__
static const char* pass_name = "tsgx";

#define JMP_SIZE 12

// Global variables.
static struct ControlFlowGraph* cfg;
static struct FunctionCFG* fun_cfg;
static int is_caslr_enabled;

// Use to track the number of br_tables.
static int br_table_num;

#if FIX_SIZE_UNIT // For fix-sized code units
#define CODE_UNIT_SIZE 64

struct FixSizeCodeUnit
{
  size_t size;
  size_t capacity;
  struct UnitInstr // The UnitInstr may not relect the actual instrucction.
  {
    size_t offset;
    size_t size;
  } * data;
};
static DEFINE_VECTOR_GROW(fix_size_unit, struct FixSizeCodeUnit);
static DEFINE_VECTOR_INIT(fix_size_unit, struct FixSizeCodeUnit);
static DEFINE_VECTOR_RESIZE(fix_size_unit, struct FixSizeCodeUnit);

struct FixSizeCodeUnit fix_size_unit;

static void
add_instr(struct FixSizeCodeUnit* code_unit, size_t offset, size_t size)
{
  assert(code_unit != NULL);
  if (!fix_size_unit_grow(code_unit)) {
    assert(0);
  }
  struct UnitInstr* instr = &code_unit->data[code_unit->size - 1];
  instr->offset = offset;
  instr->size = size;
}

static void
reset_unit(struct FixSizeCodeUnit* code_unit)
{
  if (!fix_size_unit_resize(code_unit, 0)) {
    assert(0);
  }
}
#endif

#if 0 // For debug.
/*#define ListSize 27
static size_t whitelist[ListSize] = { 21, 24, 27, 29, 32, 33, 36, 38, 41,
                                      42, 43, 52, 53, 54, 55, 56, 59, 61,
                                      63, 64, 65, 66, 76, 77, 80, 81, 82 };*/

#define ListSize 1
// 27, 59, 63
static size_t whitelist[ListSize] = { 27 };
static size_t skip_number = 1174;
// static size_t skip_counter = 0;

__attribute__((unused)) static int
is_in_whitelist(size_t id)
{
  int i;
  for (i = 0; i < ListSize; i++) {
    if (id == whitelist[i]) {
      return 1;
    }
  }
  return 0;
}
#endif

// Debug
// static int counter = 0;
#if 0
static void
dump_last_n_memrefs(struct CompilerContext* ctx, size_t n)
{
  assert(ctx->memrefs != NULL);
  struct MemoryReferences* memrefs = ctx->memrefs;
  size_t i;
  size_t p = memrefs->size - 1;

  plog("========DUMP LAST N MEMREFS=======\n");
  for (i = 0; i < n; i++) {
    struct MemoryRef* memref = &memrefs->data[p];
    plog("#%u id: %zu -> %zu, type: %u, offset: %lx, depth: %zu\n",
         p,
         memref->unit_idx,
         memref->idx,
         memref->type,
         memref->code_offset,
         memref->depth);
    if (p == 0) {
      break;
    }
    p--;
  }
  plog("=====DUMP LAST N MEMREFS END====\n");
}
#endif

__attribute__((unused)) static void
insert_lea(struct CompilerContext* ctx,
           struct CFGNode* node,
           struct CFGTarget* target)
{
  assert(ctx->memrefs != NULL);
  struct MemoryReferences* memrefs = ctx->memrefs;
  struct MemoryRef* memref = new_memref(memrefs);
  struct Operand next_addr;

  build_operand(&next_addr, REG_UNKNOWN, REG_UNKNOWN, SCALE_NONE, 5);
  emit_lea_rm(output(ctx), GP_R15, &next_addr, VALTYPE_I64);

  memref->type = MEMREF_LEA_NEXT;
  memref->code_offset = pc_offset(output(ctx)) - 4;
  memref->unit_idx = node->id;
  memref->idx = target->id;
  memref->target_type = target->type;
  memref->depth = node->depth - target->depth;

#if __DEBUG_TSGX__
  plog("[insert_lea][add_relo] id: %zu -> %zu (%zu), offset: %lx, depth: %zu\n",
       memref->unit_idx,
       memref->idx,
       memref->target_type - 0xe0,
       memref->code_offset,
       memref->depth);
#endif
}

__attribute__((unused)) static void
jump_to_springboard(struct CompilerContext* ctx,
                    size_t type,
                    size_t is_cond,
                    condition_t cond)
{
  assert(ctx->memrefs != NULL);
  struct MemoryReferences* memrefs = ctx->memrefs;
  struct MemoryRef* memref = new_memref(memrefs);

  if (is_cond == UcondBranch) {
    emit_jmp_32(output(ctx), 0);
  } else {
    assert(is_cond == CondBranch);
    label_t label = { 0, 0 };
    emit_jcc(output(ctx), cond, &label, Far);
  }

  // unit_id will be resolved based on offset (see convert_to_unit_relo_info()).
  memref->type = type;
  memref->code_offset = pc_offset(output(ctx)) - 4;
  memref->idx = 0;
}

__attribute__((unused)) static void
update_jump_to_springboard(struct CompilerContext* ctx,
                           // struct MemoryRef* memref,
                           size_t id,
                           size_t type,
                           size_t is_cond,
                           condition_t cond)
{
  struct MemoryReferences* memrefs = ctx->memrefs;
  struct MemoryRef* memref = &memrefs->data[id];

  assert(memref != NULL);
  assert(memref->type == MEMREF_JMP_NEXT);
  size_t offset = memref->code_offset;

  if (is_cond == UcondBranch) {
    emit_jmp_32(output(ctx), 0);
  } else {
    assert(is_cond == CondBranch);
    label_t label = { 0, 0 };
    emit_jcc(output(ctx), cond, &label, Far);
  }
  memset(memref, 0, sizeof(struct MemoryRef));
  memref->type = type;
  memref->code_offset = pc_offset(output(ctx)) - 4;
  memref->idx = 0;
#if __DEBUG_TSGX__
  plog("[update_jump_to_springboard] offset %lx -> %lx\n",
       offset,
       memref->code_offset);
#endif
}

__attribute__((unused)) static size_t
insert_jump(struct CompilerContext* ctx,
            struct CFGNode* node,
            struct CFGTarget* target)
{
  size_t offset = pc_offset(output(ctx));
  size_t start = offset;

  // If the last instruction is already jmp, skip.
  if ((uint8_t)output(ctx)->data[offset - 5] == 0xe9) {
#if __DEBUG_TSGX__
    plog("[insert_jump] skip id: %zu, ends with 32-bit jmp\n", node->id);
#endif
    return 0;
  } else if ((uint8_t)output(ctx)->data[offset - 2] == 0xeb) {
#if __DEBUG_TSGX__
    plog("[insert_jump] skip id: %zu, ends with 8-bit jmp\n", node->id);
#endif
    return 0;
  }

  insert_lea(ctx, node, target);
  jump_to_springboard(ctx, MEMREF_SPRINGBOARD_NEXT, UcondBranch, COND_NONE);

  return pc_offset(output(ctx)) - start;
}

__attribute__((unused)) static void
update_branch(struct CompilerContext* ctx,
              struct CFGNode* node,
              struct CFGTarget* target)
{

  size_t offset = pc_offset(output(ctx));

  // Case of unconditional 32-bit jump.
  if ((uint8_t)output(ctx)->data[offset - 5] == 0xe9) {
    output_buf_shrink(output(ctx), 5);
    insert_lea(ctx, node, target);
    jump_to_springboard(ctx, MEMREF_SPRINGBOARD_NEXT, UcondBranch, COND_NONE);
  } else if ((uint8_t)output(ctx)->data[offset - 2] == 0xeb) {
    // Case of unconditional 8-bit jump.
    output_buf_shrink(output(ctx), 2);
    insert_lea(ctx, node, target);
    jump_to_springboard(ctx, MEMREF_SPRINGBOARD_NEXT, UcondBranch, COND_NONE);
  } else if ((uint8_t)output(ctx)->data[offset - 5] >= 0x80 &&
             (uint8_t)output(ctx)->data[offset - 5] <= 0x8f) {
    condition_t cond = (uint8_t)output(ctx)->data[offset - 5] & 0xf;
    output_buf_shrink(output(ctx), 6);
    insert_lea(ctx, node, target);
    jump_to_springboard(ctx, MEMREF_SPRINGBOARD_NEXT, CondBranch, cond);
  } else {
#if __DEBUG_TSGX__
    plog("[update_branch] unhandled - id: %zu, depth %zu\n",
         node->id,
         node->depth);
    assert(0);
#endif
  }
}

__attribute__((unused)) size_t
update_aslr_branch(struct CompilerContext* ctx,
                   struct CFGNode* node,
                   struct CFGTarget* target)
{
  struct MemoryReferences* memrefs = ctx->memrefs;
  size_t memref_index = memrefs->size - 1;
  struct MemoryRef* memref = &memrefs->data[memref_index];
  size_t offset = pc_offset(output(ctx));

  if (memref->type != MEMREF_JMP_NEXT) {
#if __DEBUG_TSGX__
    plog("[update_aslr_branch] skip id: %zu\n", node->id);
#endif
    return 0;
  }

  size_t original_offset = memref->code_offset;

  // Simply update the relocation entry of the jmp/jcc inserted by pass_caslr.
  if ((uint8_t)output(ctx)->data[offset - 5] == 0xe9) {
    output_buf_shrink(output(ctx), 5);
    // spill_register(ctx, GP_RAX);
    insert_lea(ctx, node, target);
    update_jump_to_springboard(
      ctx, memref_index, MEMREF_SPRINGBOARD_NEXT, UcondBranch, COND_NONE);
  } else if ((uint8_t)output(ctx)->data[offset - 5] >= 0x80 &&
             (uint8_t)output(ctx)->data[offset - 5] <= 0x8f) {
    condition_t cond = (uint8_t)output(ctx)->data[offset - 5] & 0xf;
    output_buf_shrink(output(ctx), 6);
    // spill_register(ctx, GP_RAX);
    insert_lea(ctx, node, target);
    update_jump_to_springboard(
      ctx, memref_index, MEMREF_SPRINGBOARD_NEXT, CondBranch, cond);
  } else {
    // Should not have other cases.
    assert(0);
  }
#if __DEBUG_TSGX__
  plog("[update_aslr_branch] update offset: %lx -> %lx\n",
       original_offset,
       memref->code_offset);
#endif
  return pc_offset(output(ctx)) - offset;
}

#if FIX_SIZE_UNIT
__attribute__((unused)) static void
update_memref(struct CompilerContext* ctx,
              size_t start_offset,
              size_t end_offset)
{
  assert(ctx->memrefs != NULL);
  struct MemoryReferences* memrefs = ctx->memrefs;
  struct MemoryRef* memref;
  size_t offset;
  size_t extra_bytes = JMP_SIZE;
  size_t i;

  if ((uint8_t)output(ctx)->data[start_offset - 5] == 0xe9) {
    extra_bytes = 0;
  }

  for (i = 0; i < memrefs->size; i++) {
    memref = &memrefs->data[i];
    offset = memref->code_offset;
    if (offset >= start_offset && offset < end_offset) {
#if __DEBUG_TSGX_SPLIT__
      printf("[update_memref] %zu -> %zu (%u), offset %lx",
             memref->unit_idx,
             memref->idx,
             memref->type,
             offset);
#endif
      memref->code_offset += extra_bytes;
      printf(
        " -> %lx (%lx - %lx)\n", memref->code_offset, start_offset, end_offset);
    }
  }
}

// Split unit design
// Before split:
// unit n (target: m)
// ...
// ...  <----- split point
// ...
// unit m
//
// After split:
// unit n (target: k)
// ...
// unit k (target: m)
// ...
// unit m
__attribute__((unused)) static void
split_unit(struct CompilerContext* ctx, struct FixSizeCodeUnit* code_unit)
{
  struct CFGNode* node;
  struct CFGNode* next_node;
  struct CFGTarget* target;
  struct CFGTarget* next_target;
  size_t offset = pc_offset(output(ctx));
  size_t split_offset = offset;
  uint8_t* buf = NULL;
  size_t buf_size = 0;
  int i;
  int extra_bytes = 0;

#if 0 // For debug.
  const struct Function* func = ctx->func;

  /*if (is_in_whitelist(func->fun_index)) {
    // Reset the fix_size_unit.
    plog("[split_unit] skip fun #%zu\n", func->fun_index);
    reset_unit(code_unit);
    add_instr(code_unit, pc_offset(output(ctx)), 0); // Head node.
    return;
  }*/

  node = &fun_cfg->data[fun_cfg->size - 1];
  if (node->id >= skip_number) {
    plog("[split_unit] skip fun #%zu, unit: %zu\n", func->fun_index, node->id);
    return;
  }
#endif

  // Create a new node for the split.
  if (!fun_cfg_grow(fun_cfg)) {
    assert(0);
  }
  node = &fun_cfg->data[fun_cfg->size - 2];
  target = get_target(node);

  next_node = &fun_cfg->data[fun_cfg->size - 1];
  next_node->id = node->id + 1;
  next_node->depth = node->depth;
  next_node->control_type = node->control_type;
  next_node->control_state = node->control_state;

  cfg_target_list_init(&next_node->target_list);

#if __DEBUG_TSGX_SPLIT__
  plog("[split_unit] node: %zu, next_node: %zu\n", node->id, next_node->id);
#endif
  // Create a target for the next_node and copy
  // the target of node to it.
  if (target != NULL) {
    next_target = new_target(next_node);
    if (target->id == 0) {
      next_target->id = 0;
    } else {
      // Need to shift id by one.
      next_target->id = next_node->id + 1;
    }
    next_target->type = target->type;
    next_target->depth = target->depth;

    // Update the target of node.
    target->id = next_node->id;
    target->type = TargetKnown;
  } else {
    // Case of the very first unit (the start of the function),
    // no target is assigned yet, so we need to create a new one.
    target = new_target(node);
    target->id = next_node->id;
    target->type = TargetKnown;
    target->depth = 0;
  }

  // Search for approriate split point.
  for (i = code_unit->size - 1; i >= 0; i--) {
    struct UnitInstr* instr = &code_unit->data[i];
    split_offset = instr->offset;
#if __DEBUG_TSGX_SPLIT__
    plog("Search for split point - size: %zu, offset %lx\n",
         instr->size,
         instr->offset);
#endif
    if (instr->size <= CODE_UNIT_SIZE - JMP_SIZE) {
      break;
    }
  }

  // If the split_offset is smaller than the current offset, we need
  // to put them into the next unit.
  if (split_offset < offset) {
    buf_size = offset - split_offset;
#if __DEBUG_TSGX_SPLIT__
    plog("split_offset: %lx, offset: %lx, diff: %zu\n",
         split_offset,
         offset,
         buf_size);
#endif
    buf = malloc(buf_size);
    memset(buf, 0, buf_size);
    memcpy(buf, (uint8_t*)&output(ctx)->data[split_offset], buf_size);
    // Extra bytes that we move to the next unit may include
    // memory references. As a result, we need to upade the offset
    // of these memory references.
    update_memref(ctx, split_offset, offset);
    output_buf_shrink(output(ctx), buf_size);
  }

  // Insert jmp.
  extra_bytes = insert_jump(ctx, node, target);

  // Update the offset for the next node.
  next_node->offset = pc_offset(output(ctx));
  // Update the size of node.
  node->size = next_node->offset - node->offset;

  // Append the bytes we previously removed.
  for (i = 0; i < (int)buf_size; i++) {
    emit(output(ctx), buf[i]);
  }
  offset = pc_offset(output(ctx));
  // Keep the current size.
  next_node->size = offset - next_node->offset;
  assert(next_node->size <= CODE_UNIT_SIZE);

  // Reset the fix_size_unit.
  reset_unit(code_unit);
  add_instr(code_unit, next_node->offset, 0); // Head node.
  add_instr(code_unit, offset, next_node->size);

  if (buf != NULL) {
    free(buf);
  }
}
#endif

/* Currently there are three cases of branches require patching.
 * Case 1: Unconditional jmp (at the end of if, br-like instructions)
 *   - Target: the end of an control block.
 *   - Example 1:
 *       if
 *         ...
 *         jmp n
 *       else
 *         ...
 *       end <--- patching target of jmp to this point
 *   - Example 2:
 *       block
 *         ...
 *         if
 *           ...
 *           br 1
 *         else
 *           ...
 *         end
 *       end <---- patching target of br 1 to this point
 *   - Note: If the target of br if loop, which should be
 *           the beginning of the loop (backward jmp). This
 *           case does not require patching (handled by
 *           update_branch)
 * Case 2: Conditional jmp (at the beginning of if)
 *  - Target 1: the end of the if block for if-end case
 *  - Example:
 *      if do jcc
 *        ...
 *      end <----- patching the target of jcc to this point.
 *  - Target 2: the beginning of the else block for if-else-case
 *  - Example:
 *      if do jcc
 *        ...
 *      else <----- patching the target of jcc to this point.
 *        ...
 *      end
 */
__attribute__((unused)) static void
try_patch_target(struct CompilerContext* ctx, struct CFGNode* node)
{
  assert(ctx->memrefs != NULL);
  struct MemoryReferences* memrefs = ctx->memrefs;
  size_t i;
  uint8_t control_state = node->control_state;
  control_type_t control_type = node->control_type;

  (void)control_type;

  for (i = 0; i < memrefs->size; i++) {
    uint8_t target_type = memrefs->data[i].target_type;
    if (memrefs->data[i].type != MEMREF_LEA_NEXT) {
      continue;
    }
    switch (target_type) {
      case ElseUnknown:
      case BrUnknown: {
        if (control_state != ControlEnd) {
          break;
        }
        if (memrefs->data[i].depth == node->depth + 1) {
          memrefs->data[i].idx = node->id;
          memrefs->data[i].target_type = TargetKnown;
#if __DEBUG_TSGX__
          plog("[patch target] else/br branch id: %zu -> %zu\n",
               memrefs->data[i].unit_idx,
               memrefs->data[i].idx);
#endif
        }
        break;
      }
      case IfUnknown: {
        if ((control_state == ControlStart) && (control_type == CONTROL_ELSE) &&
            (memrefs->data[i].depth == node->depth)) {
          memrefs->data[i].idx = node->id;
          memrefs->data[i].target_type = TargetKnown;
#if __DEBUG_TSGX__
          plog("[patch target] if-else branch id: %zu -> %zu\n",
               memrefs->data[i].unit_idx,
               memrefs->data[i].idx);
#endif
        } else if ((control_state == ControlEnd) &&
                   (control_type == CONTROL_IF) &&
                   (memrefs->data[i].depth == node->depth + 1)) {
          memrefs->data[i].idx = node->id;
          memrefs->data[i].target_type = TargetKnown;
#if __DEBUG_TSGX__
          plog("[patch target] if-end branch id: %zu -> %zu\n",
               memrefs->data[i].unit_idx,
               memrefs->data[i].idx);
#endif
        }
        break;
      }
      default: {
        assert(target_type == TargetKnown || target_type == TargetNone);
        break;
      }
    }
  }
}

__attribute__((unused)) static void
onFunctionStart(struct CompilerContext* ctx, const struct Function* func)
{
  size_t i;
  size_t offset = pc_offset(output(ctx));
  const struct Module* module = sgxwasm_get_module(func);
  size_t num_imports = module->n_imported_funcs;
#if __DEBUG_TSGX__
  plog("[onFunctionStart] fun #%zu, offset: %zu\n", func->fun_index, offset);
#else
  (void)ctx;
  (void)func;
#endif

#if FIX_SIZE_UNIT
  fix_size_unit_init(&fix_size_unit);
  // Add the head node.
  add_instr(&fix_size_unit, 0, 0);
  add_instr(&fix_size_unit, offset, offset);

  // For debug.
  // skip_counter = 0;
#endif

  // Reset the br_table_num;
  br_table_num = 0;

  fun_cfg = NULL;
  for (i = 0; i < cfg->size; i++) {
    struct FunctionCFG* f = &cfg->data[i];
    if (f->id == func->fun_index) {
      fun_cfg = f;
      break;
    }
  }
  assert(fun_cfg != NULL);

  if (func->name && !strcmp(func->name, "_main")) {
    struct Operand next_addr;
    build_operand(&next_addr, REG_UNKNOWN, REG_UNKNOWN, SCALE_NONE, 5);
    emit_lea_rm(output(ctx), GP_R15, &next_addr, VALTYPE_I64);
    jump_to_springboard(ctx, MEMREF_SPRINGBOARD_BEGIN, UcondBranch, COND_NONE);
  } else if (func->fun_index >= num_imports) {
    // jump_to_springboard(ctx, MEMREF_SPRINGBOARD_NEXT);
  }
}

__attribute__((unused)) static void
onFunctionEnd(struct CompilerContext* ctx, const struct Function* func)
{

  (void)ctx;
  (void)func;

  if (func->name && !strcmp(func->name, "_main")) {
    struct Operand next_addr;
    build_operand(&next_addr, REG_UNKNOWN, REG_UNKNOWN, SCALE_NONE, 5);
    emit_lea_rm(output(ctx), GP_R15, &next_addr, VALTYPE_I64);
    jump_to_springboard(ctx, MEMREF_SPRINGBOARD_END, UcondBranch, COND_NONE);
  }
}

__attribute__((unused)) static void
onControlStart(struct CompilerContext* ctx,
               const struct Function* func,
               control_type_t control_type,
               size_t depth)
{
  assert(fun_cfg->size > 1);
  size_t fun_index = func->fun_index;
  const struct Module* module = sgxwasm_get_module(func);
  size_t num_imports = module->n_imported_funcs;
  struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 2];
  struct CFGNode* next_node = &fun_cfg->data[fun_cfg->size - 1];
  struct CFGTarget* target = get_target(node);
  size_t offset = pc_offset(output(ctx));
  size_t size = node->size;
  size_t extra_bytes = 0;

  (void)func;
  (void)control_type;
  (void)depth;

  assert(node != NULL && next_node != NULL);

  if (fun_index < num_imports) {
    return;
  }

  // If pass_calsr is enabled already, we only need to
  // update the branch as it should be inserted already.
  if (!is_caslr_enabled) {
    extra_bytes = insert_jump(ctx, node, target);
  } else {
    assert(is_caslr_enabled == 1);
    extra_bytes = update_aslr_branch(ctx, node, target);
  }
  node->size = size + extra_bytes;
  next_node->offset = offset + extra_bytes;

  // Patch conditional branch.
  try_patch_target(ctx, next_node);

#if FIX_SIZE_UNIT
  // Reset the unit.
  reset_unit(&fix_size_unit);
  add_instr(&fix_size_unit, pc_offset(output(ctx)), 0);

  // For debug.
  // skip_counter++;
#endif
}

__attribute__((unused)) static void
onControlEnd(struct CompilerContext* ctx,
             const struct Function* func,
             control_type_t control_type,
             size_t depth)
{
  assert(fun_cfg->size > 1);
  size_t fun_index = func->fun_index;
  const struct Module* module = sgxwasm_get_module(func);
  size_t num_imports = module->n_imported_funcs;
  struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 2];
  struct CFGNode* next_node = &fun_cfg->data[fun_cfg->size - 1];
  struct CFGTarget* target = get_target(node);
  size_t offset = pc_offset(output(ctx));
  size_t size = node->size;
  size_t extra_bytes = 0;

  (void)func;
  (void)control_type;
  (void)depth;

  assert(node != NULL && next_node != NULL);

  if (fun_index < num_imports) {
    return;
  }

  // If pass_calsr is enabled already, we only need to
  // update the branch as it should be inserted already.
  if (!is_caslr_enabled) {
    extra_bytes = insert_jump(ctx, node, target);
  } else {
    assert(is_caslr_enabled == 1);
    extra_bytes = update_aslr_branch(ctx, node, target);
  }
  node->size = size + extra_bytes;
  next_node->offset = offset + extra_bytes;

  // Patch branch target.
  try_patch_target(ctx, next_node);

#if FIX_SIZE_UNIT
  // Reset the unit.
  reset_unit(&fix_size_unit);
  add_instr(&fix_size_unit, pc_offset(output(ctx)), 0);

  // For debug.
  // skip_counter++;
#endif
}

__attribute__((unused)) static void
onInstructionStart(struct CompilerContext* ctx,
                   const struct Function* func,
                   const struct Instr* instr)
{
  (void)ctx;
  (void)func;
  (void)instr;

  size_t fun_index = func->fun_index;
  const struct Module* module = sgxwasm_get_module(func);
  size_t num_imports = module->n_imported_funcs;
  if (fun_index < num_imports) {
    return;
  }

#if FIX_SIZE_UNIT
  // The point is prior to control start & end.
  // Check if requiring spilt based on the current counter.
  // Example:
  //    inst
  //    ...
  //    control_inst
  //                  <---- Split point, if counter > unit_size
  //    - end_unit n
  //    - start_unit n + 1
  uint8_t opcode = instr->opcode;
  if (is_control_instruction(opcode)) {
    size_t num_inst = fix_size_unit.size;
    assert(num_inst > 0);
    struct UnitInstr* i = &fix_size_unit.data[num_inst - 1];
    if (i->size > CODE_UNIT_SIZE - JMP_SIZE) {
#if __DEBUG_TSGX_SPLIT__
      plog("[onInstructionStart] Control Inst - Split point\n");
#endif
      split_unit(ctx, &fix_size_unit);
    }
  }
#endif
}

__attribute__((unused)) static void
onInstructionEnd(struct CompilerContext* ctx,
                 const struct Function* func,
                 const struct Instr* instr)
{
  (void)ctx;
  (void)func;
  (void)instr;

  size_t fun_index = func->fun_index;
  const struct Module* module = sgxwasm_get_module(func);
  size_t num_imports = module->n_imported_funcs;
  if (fun_index < num_imports) {
    return;
  }

  size_t opcode = instr->opcode;

  // Increase the br_table_num by one.
  if (opcode == OPCODE_BR_TABLE) {
    br_table_num++;
  }

#if FIX_SIZE_UNIT
  size_t num_inst = fix_size_unit.size;
  struct UnitInstr* prev_instr = &fix_size_unit.data[num_inst - 1];
  size_t offset = pc_offset(output(ctx));
  size_t diff = offset - prev_instr->offset;
  size_t size = prev_instr->size + diff;

  // Ensure no split point at the end of the function as this will
  // be checked after the low_return hook.
  if (control_depth(control(ctx)) == 0) {
    assert(size <= CODE_UNIT_SIZE);
    return;
  }
#if __DEBUG_TSGX_SPLIT__
  if (diff >= CODE_UNIT_SIZE) {
    plog("Inst %x, size: %zu (large), counter: %zu\n", opcode, diff, size);
  } else {
    plog("Inst %x, size: %zu, counter: %zu\n", opcode, diff, size);
  }
#endif
  if (size > (CODE_UNIT_SIZE - JMP_SIZE)) {
#if __DEBUG_TSGX_SPLIT__
    plog("Find split point: (prev: %zu, curr: %zu)\n", prev_instr->size, size);
#endif
    split_unit(ctx, &fix_size_unit);
  } else {
    add_instr(&fix_size_unit, offset, size);
  }
#endif
}

__attribute__((unused)) static void
onBranchEnd(struct CompilerContext* ctx,
            const struct Instr* instr,
            uint32_t depth)
{
  assert(fun_cfg != NULL);
  uint8_t opcode = instr->opcode;
  switch (opcode) {
    case OPCODE_END: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = get_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_TSGX__
      plog("[onBranchEnd] end\n");
#endif
      if (!is_caslr_enabled) {
        update_branch(ctx, node, target);
      } else {
        update_aslr_branch(ctx, node, target);
      }
      break;
    }
    case OPCODE_IF: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = get_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_TSGX__
      plog("[onBranchEnd] if\n");
#endif
      if (!is_caslr_enabled) {
        update_branch(ctx, node, target);
      } else {
        update_aslr_branch(ctx, node, target);
      }
      break;
    }
    case OPCODE_ELSE: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = get_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_TSGX__
      plog("[onBranchEnd] else\n");
#endif
      if (!is_caslr_enabled) {
        update_branch(ctx, node, target);
      } else {
        update_aslr_branch(ctx, node, target);
      }
      break;
    }
    case OPCODE_BR: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = get_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_TSGX__
      plog("[onBranchEnd] br %zu\n", depth);
#endif
      if (!is_caslr_enabled) {
        update_branch(ctx, node, target);
      } else {
        update_aslr_branch(ctx, node, target);
      }
      break;
    }
    case OPCODE_BR_IF: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = get_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_TSGX__
      plog("[onBranchEnd] br_if %zu\n", depth);
#endif
      if (!is_caslr_enabled) {
        update_branch(ctx, node, target);
      } else {
        update_aslr_branch(ctx, node, target);
      }
      break;
    }
    case OPCODE_BR_TABLE: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = get_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_TSGX__
      plog("[onBranchEnd] br_table %zu\n", depth);
#endif
      if (!is_caslr_enabled) {
        update_branch(ctx, node, target);
      } else {
        update_aslr_branch(ctx, node, target);
      }
      break;
    }
    default: {
      // unhanlded cases.
    }
  }
}

// Deal with the machine instructions generated from BrTable.
__attribute__((unused)) static void
onLowBrTable(struct CompilerContext* ctx, const struct MachineInstr* minstr)
{
  assert(fun_cfg != NULL && ctx->memrefs != NULL);
  struct MemoryReferences* memrefs = ctx->memrefs;
  struct MemoryRef* memref;
  size_t offset = pc_offset(output(ctx));
  struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
  uint8_t type = minstr->type;

  if (type == BrTableJmp) {
    assert((uint8_t)output(ctx)->data[offset - 5] >= 0x80 &&
           (uint8_t)output(ctx)->data[offset - 5] <= 0x8f);
    memref = new_memref(memrefs);
    memref->type = MEMREF_BR_TABLE_JMP;
    memref->code_offset = offset - 4;
    memref->unit_idx = node->id;
    memref->idx = (br_table_num << 16) | (minstr->max << 8) | minstr->min;
  } else if (type == BrCaseJmp) {
    if ((uint8_t)output(ctx)->data[offset - 2] == 0xeb) {
      output(ctx)->data[offset - 2] = 0xe9;
      emit(output(ctx), 0x00);
      emit(output(ctx), 0x00);
      emit(output(ctx), 0x00);
    }
    offset = pc_offset(output(ctx));
    assert((uint8_t)output(ctx)->data[offset - 5] == 0xe9);
    memref = new_memref(memrefs);
    memref->type = MEMREF_BR_CASE_JMP;
    memref->code_offset = offset - 4;
    memref->unit_idx = node->id;
    memref->idx = (br_table_num << 8) | minstr->depth;
  } else if (type == BrTableTarget) {
    memref = new_memref(memrefs);
    memref->type = MEMREF_BR_TABLE_TARGET;
    memref->code_offset = offset;
    memref->unit_idx = node->id;
    memref->idx = (br_table_num << 16) | (minstr->max << 8) | minstr->min;
  } else {
    assert(type == BrCaseTarget);
    size_t id = (br_table_num << 8) | minstr->depth;
    size_t i;
    int found = 0;
    for (i = 0; i < memrefs->size; i++) {
      struct MemoryRef* ref = &memrefs->data[i];
      if (ref->type != MEMREF_BR_CASE_TARGET) {
        continue;
      }
      if (ref->idx == id) {
        found = 1;
        break;
      }
    }
    if (found == 0) {
      memref = new_memref(memrefs);
      memref->type = MEMREF_BR_CASE_TARGET;
      memref->code_offset = offset;
      memref->unit_idx = node->id;
      memref->idx = id;
    }
  }
}

__attribute__((unused)) static void
onMachineInstrStart(struct CompilerContext* ctx,
                    const struct Function* func,
                    const struct MachineInstr* minstr)
{
  (void)ctx;
  (void)func;
  (void)minstr;

  size_t fun_index = func->fun_index;
  const struct Module* module = sgxwasm_get_module(func);
  size_t num_imports = module->n_imported_funcs;
  if (fun_index < num_imports) {
    return;
  }

  assert(minstr != NULL);
  switch (minstr->type) {
    case BrTableTarget:
    case BrCaseTarget:
    case BindLabel: {
      // Set the label to bypass bind.
      minstr->label->pos = -1;
      break;
    }
    case CallFunction: {
      uint32_t target_index = minstr->fun_index;
      if (target_index < num_imports) {
#if TSX_SUPPORT
        emit_xend(output(ctx));
#endif
      }
      break;
    }
  }
}

__attribute__((unused)) static void
onMachineInstrEnd(struct CompilerContext* ctx,
                  const struct Function* func,
                  const struct MachineInstr* minstr)
{
  size_t fun_index = func->fun_index;
  const struct Module* module = sgxwasm_get_module(func);
  size_t num_imports = module->n_imported_funcs;
  if (fun_index < num_imports) {
    return;
  }

  assert(minstr != NULL);
  switch (minstr->type) {
    case UcondBranch:
    case CondBranch: {
      const struct Instr* instr = minstr->instr;
      onBranchEnd(ctx, instr, minstr->depth);
      break;
    }
    case BrTableJmp:
    case BrTableTarget:
    case BrCaseJmp:
    case BrCaseTarget: {
      onLowBrTable(ctx, minstr);
      break;
    }
    case CallFunction: {
      uint32_t target_index = minstr->fun_index;
      if (target_index < num_imports) {
#if TSX_SUPPORT
        label_t label = { 0, 0 };
        // spill_register(ctx, GP_RAX);
        // emit_mov_rr(output(ctx), GP_R14, GP_RAX, VALTYPE_I64);
        bind_label(output(ctx), &label, pc_offset(output(ctx)));
        emit_xbegin(output(ctx), &label);
        // emit_mov_rr(output(ctx), GP_RAX, GP_R14, VALTYPE_I64);

        /*struct Operand next_addr;
        build_operand(&next_addr, REG_UNKNOWN, REG_UNKNOWN, SCALE_NONE, 5);
        emit_lea_rm(output(ctx), GP_R15, &next_addr, VALTYPE_I64);
        jump_to_springboard(
          ctx, MEMREF_SPRINGBOARD_BEGIN, UcondBranch, COND_NONE);*/
#endif
      }
      break;
    }
  }

#if FIX_SIZE_UNIT
  uint8_t opcode = 0xff;
  size_t num_inst = fix_size_unit.size;
  struct UnitInstr* prev_instr = &fix_size_unit.data[num_inst - 1];
  size_t offset = pc_offset(output(ctx));
  size_t diff = offset - prev_instr->offset;
  size_t size = prev_instr->size + diff;

  // Null instr indicates the prologue of the function (before reaching the
  // first instruction).
  if (minstr->instr != NULL) {
    opcode = minstr->instr->opcode;
  }
#if __DEBUG_TSGX_SPLIT__
  if (diff >= CODE_UNIT_SIZE) {
    plog("Low Inst %x, type: %u, size: %zu (large), counter: %zu\n", opcode, minstr->type, diff, size);
  } else {
    plog("Low Inst %x, type: %u, size: %zu, counter: %zu\n", opcode, minstr->type, diff, size);
  }
#endif
  if (size > (CODE_UNIT_SIZE - JMP_SIZE)) {
#if __DEBUG_TSGX_SPLIT__
    plog(
      "Find low split point: (prev: %zu, curr: %zu)\n", prev_instr->size, size);
#endif
    split_unit(ctx, &fix_size_unit);

  } else {
    add_instr(&fix_size_unit, offset, size);
  }
#endif
}
#endif // End of __TSGX__

__attribute__((unused)) static void
PassInit(struct PassManager* pm)
{
  (void)pm;
#if __TSGX__
  size_t i;

  cfg = NULL;
  is_caslr_enabled = 0;
  for (i = 0; i < pm->size; i++) {
    struct Pass* pass = &pm->data[i];
    // Check dependency (pass_cfg should be done before).
    if (strcmp(pass->name, pass_name) == 0) {
      break;
    }
    if (strcmp(pass->name, "cfg") == 0) {
      cfg = get_cfg(pass);
      continue;
    }
    if (strcmp(pass->name, "caslr") == 0) {
      assert(cfg != NULL);
      is_caslr_enabled = 1;
      break;
    }
  }
  assert(cfg);
#endif
}

void
add_pass_tsgx(struct PassManager* pm)
{
  (void)pm;
#if __TSGX__
  struct Pass* pass = get_new_pass(pm);
  assert(pass != NULL);
  pass_init(pass,
            pass_name,
            PassInit,
            NULL,
            onFunctionStart,
            onFunctionEnd,
            onControlStart,
            onControlEnd,
            onInstructionStart,
            onInstructionEnd,
            onMachineInstrStart,
            onMachineInstrEnd);
  pass_add_dep(pass, "cfg");
#endif
}