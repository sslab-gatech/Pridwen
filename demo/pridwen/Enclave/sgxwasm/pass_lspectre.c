#include <sgxwasm/pass.h>
#include <sgxwasm/pass_cfg.h>

/*
 LSPECTRE scheme:
  bb:
    ...
    jae ...
    lfence
    ...
    jmp next_bb

  --->

  bb:
    ...
    jae ...
    lfence
    ...
    jmp next_bb
 */

#if __LSPECTRE__
static const char* pass_name = "qspectre";

#define JMP_SIZE 12

// Global variables.
static struct ControlFlowGraph* cfg;
static struct FunctionCFG* fun_cfg;
static int is_caslr_enabled;

// Use to track the number of br_tables.
static int br_table_num;

static uint8_t last_branch;

static int qs_bypass;

#define LOW_INSTR_FREQ 30
static size_t low_instr_counter;

#if 1 // For debug.
/*#define ListSize 27
static size_t whitelist[ListSize] = { 21, 24, 27, 29, 32, 33, 36, 38, 41,
                                      42, 43, 52, 53, 54, 55, 56, 59, 61,
                                      63, 64, 65, 66, 76, 77, 80, 81, 82 };*/

#define ListSize 3
static size_t whitelist[ListSize] = { 681, 796, 839 };
// static size_t skip_number = 1174;
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
jump_to_next_bb(struct CompilerContext* ctx, struct CFGNode* node,
                struct CFGTarget* target, size_t type, size_t is_cond,
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
  memref->unit_idx = node->id;
  memref->idx = target->id;
  memref->target_type = target->type;
  // Target depth is relative depth, need covertion here.
  memref->depth = node->depth - target->depth;

#if __DEBUG_LSPECTRE__
  plog("[jump_to_next_bb][add_relo] id: %zu -> %zu (%zu), offset: %lx, depth: "
       "%zu\n",
       memref->unit_idx, memref->idx, memref->target_type - 0xe0,
       memref->code_offset, memref->depth);
#endif
}

__attribute__((unused)) static size_t
insert_jump(struct CompilerContext* ctx, struct CFGNode* node,
            struct CFGTarget* target)
{
  size_t offset = pc_offset(output(ctx));
  size_t start = offset;
  int end_with_br_if = 0;

  // If the last instruction is already jmp, skip.
  if ((uint8_t)output(ctx)->data[offset - 5] == 0xe9 &&
      last_branch != OPCODE_BR_IF) {
#if __DEBUG_LSPECTRE__
    plog("[insert_jump] skip id: %zu, ends with 32-bit jmp\n", node->id);
#endif
    end_with_br_if = 1;
    return 0;
  } else if ((uint8_t)output(ctx)->data[offset - 2] == 0xeb &&
             last_branch != OPCODE_BR_IF) {
#if __DEBUG_LSPECTRE__
    plog("[insert_jump] skip id: %zu, ends with 8-bit jmp\n", node->id);
#endif
    end_with_br_if = 1;
    return 0;
  }

  jump_to_next_bb(ctx, node, target, MEMREF_JMP_NEXT, UcondBranch, COND_NONE);

  return pc_offset(output(ctx)) - start;
}

__attribute__((unused)) static void
update_branch(struct CompilerContext* ctx, struct CFGNode* node,
              struct CFGTarget* target)
{

  size_t offset = pc_offset(output(ctx));

  // Case of unconditional 32-bit jump.
  if ((uint8_t)output(ctx)->data[offset - 5] == 0xe9) {
    output_buf_shrink(output(ctx), 5);
    jump_to_next_bb(ctx, node, target, MEMREF_JMP_NEXT, UcondBranch, COND_NONE);
  } else if ((uint8_t)output(ctx)->data[offset - 2] == 0xeb) {
    // Case of unconditional 8-bit jump.
    output_buf_shrink(output(ctx), 2);
    jump_to_next_bb(ctx, node, target, MEMREF_JMP_NEXT, UcondBranch, COND_NONE);
  } else if ((uint8_t)output(ctx)->data[offset - 5] >= 0x80 &&
             (uint8_t)output(ctx)->data[offset - 5] <= 0x8f) {
    condition_t cond = (uint8_t)output(ctx)->data[offset - 5] & 0xf;
    output_buf_shrink(output(ctx), 6);
    jump_to_next_bb(ctx, node, target, MEMREF_JMP_NEXT, CondBranch, cond);
    emit_lfence(output(ctx));
  } else {
#if __DEBUG_LSPECTRE__
    plog("[update_branch] unhandled - id: %zu, depth %zu\n", node->id,
         node->depth);
    assert(0);
#endif
  }
}


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
    if (memrefs->data[i].type != MEMREF_JMP_NEXT) {
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
#if __DEBUG_LSPECTRE__
          plog("[patch target] else/br branch id: %zu -> %zu\n",
               memrefs->data[i].unit_idx, memrefs->data[i].idx);
#endif
        }
        break;
      }
      case IfUnknown: {
        if ((control_state == ControlStart) && (control_type == CONTROL_ELSE) &&
            (memrefs->data[i].depth == node->depth)) {
          memrefs->data[i].idx = node->id;
          memrefs->data[i].target_type = TargetKnown;
#if __DEBUG_LSPECTRE__
          plog("[patch target] if-else branch id: %zu -> %zu\n",
               memrefs->data[i].unit_idx, memrefs->data[i].idx);
#endif
        } else if ((control_state == ControlEnd) &&
                   (control_type == CONTROL_IF) &&
                   (memrefs->data[i].depth == node->depth + 1)) {
          memrefs->data[i].idx = node->id;
          memrefs->data[i].target_type = TargetKnown;
#if __DEBUG_LSPECTRE__
          plog("[patch target] if-end branch id: %zu -> %zu\n",
               memrefs->data[i].unit_idx, memrefs->data[i].idx);
#endif
        }
        break;
      }
      default: {
        assert(target_type == TargetKnown || target_type == TargetLoopKnown ||
               target_type == TargetNone);
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

#if __DEBUG_LSPECTRE__
  plog("[onFunctionStart] fun #%zu, offset: %zu\n", func->fun_index, offset);
#else
  (void)ctx;
  (void)func;
  (void)offset;
#endif

  if (qs_bypass == 1) {
    return;
  }


  // Reset the br_table_num.
  br_table_num = 0;

  // Reset the low_instr_counter.
  low_instr_counter = 0;

  fun_cfg = NULL;
  for (i = 0; i < cfg->size; i++) {
    struct FunctionCFG* f = &cfg->data[i];
    if (f->id == func->fun_index) {
      fun_cfg = f;
      break;
    }
  }
  assert(fun_cfg != NULL);
}

__attribute__((unused)) static void
onFunctionEnd(struct CompilerContext* ctx, const struct Function* func)
{

  (void)ctx;
  (void)func;
}

__attribute__((unused)) static void
onControlStart(struct CompilerContext* ctx, const struct Function* func,
               control_type_t control_type, size_t depth)
{
  if (qs_bypass == 1) {
    return;
  }

  assert(fun_cfg->size > 1);
  struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 2];
  struct CFGNode* next_node = &fun_cfg->data[fun_cfg->size - 1];
  struct CFGTarget* target = get_target(node);
  size_t offset = pc_offset(output(ctx));
  size_t size = node->size;
  size_t extra_bytes = 0;
  size_t fun_index = func->fun_index;

  (void)control_type;
  (void)depth;

  assert(node != NULL && next_node != NULL);

  extra_bytes = insert_jump(ctx, node, target);
  node->size = size + extra_bytes;
  next_node->offset = offset + extra_bytes;

  // Patch conditional branch.
  try_patch_target(ctx, next_node);
}

__attribute__((unused)) static void
onControlEnd(struct CompilerContext* ctx, const struct Function* func,
             control_type_t control_type, size_t depth)
{
  if (qs_bypass == 1) {
    return;
  }

  assert(fun_cfg->size > 1);
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

  extra_bytes = insert_jump(ctx, node, target);
  node->size = size + extra_bytes;
  next_node->offset = offset + extra_bytes;

  // Patch branch target.
  try_patch_target(ctx, next_node);
}

__attribute__((unused)) static void
onInstructionStart(struct CompilerContext* ctx, const struct Function* func,
                   const struct Instr* instr)
{
  (void)ctx;
  (void)func;
  (void)instr;
}

__attribute__((unused)) static void
onInstructionEnd(struct CompilerContext* ctx, const struct Function* func,
                 const struct Instr* instr)
{
  (void)ctx;
  (void)func;
  (void)instr;

  size_t opcode = instr->opcode;

  if (qs_bypass == 1) {
    return;
  }

  // Hook at the end of a function (after ret).
  if (control_depth(control(ctx)) == 0) {
#if __DEBUG_CASLR__
    plog("[onFunctionEnd] fun #%zu, size: %zu\n", func->fun_index,
         pc_offset(output(ctx)));
#endif
    return;
  }

  // Increase the br_table_num by one.
  if (opcode == OPCODE_BR_TABLE) {
    br_table_num++;
  }
}

__attribute__((unused)) static void
onBranchEnd(struct CompilerContext* ctx, const struct Instr* instr,
            uint32_t depth)
{
  assert(fun_cfg != NULL);
  (void)depth;
  uint8_t opcode = instr->opcode;
  switch (opcode) {
    case OPCODE_END: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = get_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_LSPECTRE__
      plog("[onBranchEnd] end\n");
#endif
      update_branch(ctx, node, target);
      last_branch = OPCODE_END;
      break;
    }
    case OPCODE_IF: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = get_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_LSPECTRE__
      plog("[onBranchEnd] if\n");
#endif
      update_branch(ctx, node, target);
      last_branch = OPCODE_IF;
      break;
    }
    case OPCODE_ELSE: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = get_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_LSPECTRE__
      plog("[onBranchEnd] else\n");
#endif
      update_branch(ctx, node, target);
      last_branch = OPCODE_ELSE;
      break;
    }
    case OPCODE_BR: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = get_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_LSPECTRE__
      plog("[onBranchEnd] br %zu\n", depth);
#endif
      update_branch(ctx, node, target);
      last_branch = OPCODE_BR;
      break;
    }
    case OPCODE_BR_IF: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = get_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_LSPECTRE__
      plog("[onBranchEnd] br_if %zu\n", depth);
#endif
      update_branch(ctx, node, target);
      last_branch = OPCODE_BR_IF;
      break;
    }
    case OPCODE_BR_TABLE: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = get_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_LSPECTRE__
      plog("[onBranchEnd] br_table %zu\n", depth);
#endif
      update_branch(ctx, node, target);
      last_branch = OPCODE_BR_TABLE;
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
onMachineInstrStart(struct CompilerContext* ctx, const struct Function* func,
                    const struct MachineInstr* minstr)
{
  (void)ctx;
  (void)func;
  (void)minstr;

  if (qs_bypass == 1) {
    return;
  }

  size_t fun_index = func->fun_index;
  const struct Module* module = sgxwasm_get_module(func);
  size_t num_imports = module->n_imported_funcs;
  if (fun_index < num_imports) {
    return;
  }

  assert(minstr != NULL);
  switch (minstr->type) {
    case BindLabel: {
      // Set the label to bypass bind.
      minstr->label->pos = -1;
      break;
    }

    default:
      break;
  }
}

__attribute__((unused)) static void
onMachineInstrEnd(struct CompilerContext* ctx, const struct Function* func,
                  const struct MachineInstr* minstr)
{
  if (qs_bypass == 1) {
    return;
  }

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
    default:
      break;
  }
}

static void
Validation(size_t fun_id, size_t unit_id, uint64_t addr, size_t size)
{
  (void*)fun_id;
  (void*)unit_id;
  (void*)addr;
  (void*)size;

  size_t i;
  uint8_t* code = (uint8_t*)addr;
  for (i = 0; i < size; i++) {
    uint8_t byte = code[i];
    if (byte < 0x80 | byte > 0x8f) {
      continue;
    }
    if (i + 8 >= size) {
      continue;
    }
    if (code[i + 5] == 0x0f && code[i + 6] == 0xae && code[i + 7] == 0xe8) {
      break;
    }
  }
}

#endif // End of __LSPECTRE__

__attribute__((unused)) static void
PassInit(struct PassManager* pm)
{
  (void)pm;
#if __LSPECTRE__
  size_t i;

  if (pass_is_enabled(pm, "tsgx") || pass_is_enabled(pm, "varys")) {
    qs_bypass = 1;
  }

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
add_pass_lspectre(struct PassManager* pm)
{
  (void)pm;
#if __LSPECTRE__
  struct Pass* pass = get_new_pass(pm);
  assert(pass != NULL);
  pass_init(pass, pass_name, PassInit, NULL, onFunctionStart, onFunctionEnd,
            onControlStart, onControlEnd, onInstructionStart, onInstructionEnd,
            onMachineInstrStart, onMachineInstrEnd, Validation);
  pass_add_dep(pass, "cfg");
#endif
}
