#include <sgxwasm/pass.h>

#if __ASLR__
static const char* pass_name = "aslr";

//__attribute__((unused)) static DEFINE_VECTOR_GROW(memrefs,
//                                                  struct MemoryReferences);

#define JMP_SIZE 5

enum ControlState
{
  FunctionStart = 0xf0,
  ControlStart = 0xf1,
  ControlEnd = 0xf2
};

enum TargetType
{
  TargetKnown = 0xe0,
  IfUnknown = 0xe1,
  ElseUnknown = 0xe2,
  BrUnknown = 0xe3,
  TargetNone = 0xe4,
};

struct ProcessState
{
  size_t id;
  size_t target_id;
  size_t offset;
  size_t depth;
  uint8_t control_state;
};

struct ControlFlowGraph
{
  size_t capacity;
  size_t size;
  struct CFGNode
  {
    size_t id;
    size_t offset;
    size_t depth;
    control_type_t control_type;
    size_t control_state;
    // CFG target list
    struct CFGTargetList
    {
      size_t capacity;
      size_t size;
      struct CFGTarget
      {
        size_t offset;
        size_t id;
        size_t type;
        size_t depth; // Relative depth.
      } * data;
    } target_list;
  } * data;
};
static DEFINE_VECTOR_INIT(cfg, struct ControlFlowGraph);
static DEFINE_VECTOR_GROW(cfg, struct ControlFlowGraph);
static DEFINE_VECTOR_INIT(cfg_target_list, struct CFGTargetList);
static DEFINE_VECTOR_GROW(cfg_target_list, struct CFGTargetList);

struct ControlFlowGraph cfg = { 0, 0, NULL };

struct CFGTarget*
new_target(struct CFGNode* node)
{
  if (!cfg_target_list_grow(&node->target_list)) {
    return NULL;
  }
  return &node->target_list.data[node->target_list.size - 1];
}

static void
add_unit(struct CompilerContext* ctx, struct CFGNode* node, size_t size)
{
  assert(ctx->memrefs != NULL);
  struct MemoryReferences* memrefs = ctx->memrefs;
  struct MemoryRef* memref = new_memref(memrefs);
  memref->type = MEMREF_CODE_UNIT;
  memref->code_offset = node->offset;
  memref->unit_idx = node->id;
  memref->size = size;
#if __DEBUG_ASLR__
  plog("[add_unit] id: %zu, size: %zu, offset: %lx, depth: %zu, type: %u, "
       "state: %u\n",
       memref->unit_idx,
       memref->size,
       memref->code_offset,
       node->depth,
       node->control_type - 0xb0,
       node->control_state - 0xf0);
#endif
}

__attribute__((unused)) static size_t
insert_jump(struct CompilerContext* ctx,
            struct CFGNode* node,
            struct CFGTarget* target,
            size_t type)
{
  assert(ctx->memrefs != NULL);
  struct MemoryReferences* memrefs = ctx->memrefs;
  struct MemoryRef* memref;
  size_t offset = pc_offset(output(ctx));

  // If the last instruction is already jmp, skip.
  if ((uint8_t)output(ctx)->data[offset - 5] == 0xe9) {
#if __DEBUG_ASLR__
    plog("[insert_jump] skip id: %zu, ends with 32-bit jmp\n", node->id);
#endif
    return 0;
  } else if ((uint8_t)output(ctx)->data[offset - 2] == 0xeb) {
#if __DEBUG_ASLR__
    plog("[insert_jump] skip id: %zu, ends with 8-bit jmp\n", node->id);
#endif
    return 0;
  }

  // Insert jump.
  emit_jmp_32(output(ctx), 0);
  offset = pc_offset(output(ctx));

  memref = new_memref(memrefs);
  memref->type = type;
  memref->code_offset = offset - 4;
  memref->unit_idx = node->id;
  memref->idx = target->id;
  memref->target_type = target->type;
  // Target depth is relative depth, need covertion here.
  memref->depth = node->depth - target->depth;
#if __DEBUG_ASLR__
  plog("[insert_jump] id: %zu -> %zu (%zu), offset: %lx, depth: %zu\n",
       memref->unit_idx,
       memref->idx,
       memref->target_type - 0xe0,
       memref->code_offset,
       memref->depth);
#endif

  return JMP_SIZE;
}

__attribute__((unused)) void
update_branch(struct CompilerContext* ctx,
              struct ControlFlowGraph* cfg,
              struct CFGNode* node,
              struct CFGTarget* target,
              size_t type)
{
  assert(ctx->memrefs != NULL);
  struct MemoryReferences* memrefs = ctx->memrefs;
  struct MemoryRef* memref = new_memref(memrefs);
  size_t offset = pc_offset(output(ctx));

  if (target->type == BrUnknown) {
    int i = cfg->size - 1;
    struct CFGNode* target_node = NULL;
    size_t target_depth = node->depth - target->depth;
    while (i >= 0) {
      target_node = &cfg->data[i];
      if (target_node->depth == target_depth) {
        i--;
        break;
      }
      i--;
      target_node = NULL;
    }
    assert(target_node != NULL);
    // Special handling for case of br 0. If the current the state of
    // current node is ControlEnd, we need to find the corresponding node
    // with ControlStart and check if its control type is loop. If this is
    // the case, make this node as the branch target.
    // Example:
    //   loop
    //     ...
    //     if
    //     end
    //     ..
    //     br 0 <------- the target should be the loop start
    if (target->depth == 0 && target_node->control_state == ControlEnd) {
      struct CFGNode* start_node = NULL;
      while (i >= 0) {
        start_node = &cfg->data[i];
        if (start_node->depth == target_depth &&
            start_node->control_state == ControlStart) {
          break;
        }
        i--;
        start_node = NULL;
      }
      assert(start_node != NULL);
      if (start_node->control_type == CONTROL_LOOP) {
        target_node = start_node;
      }
    }
    if (target_node->control_type == CONTROL_LOOP &&
        target_node->control_state == ControlStart) {
      target->id = target_node->id;
      target->type = TargetKnown;
#if __DEBUG_ASLR__
      plog("[update_branch] fun: %zu br %zu to loop: %zu\n",
           ctx->func->fun_index,
           target->depth,
           target->id);
#endif
    }
  }

  // Case of unconditional 32-bit jump.
  if ((uint8_t)output(ctx)->data[offset - 5] == 0xe9) {
    memref->type = type;
    memref->code_offset = offset - 4;
    memref->unit_idx = node->id;
    memref->idx = target->id;
    memref->target_type = target->type;
    // Target depth is relative depth, need covertion here.
    memref->depth = node->depth - target->depth;
#if __DEBUG_ASLR__
    plog("[update_branch] 32-bit jmp id: %zu -> %zu (%zu), depth: %zu\n",
         memref->unit_idx,
         memref->idx,
         memref->target_type - 0xe0,
         memref->depth);
#endif
  } else if ((uint8_t)output(ctx)->data[offset - 2] == 0xeb) {
    // Convert 8-bit jmp to 32-bit.
    // 0xeb 0x00 -> 0xe9 0x00 0x00 0x00 0x00
    output(ctx)->data[offset - 2] = 0xe9;
    emit(output(ctx), 0x00);
    emit(output(ctx), 0x00);
    emit(output(ctx), 0x00);

    // Case of unconditional 8-bit jump
    memref->type = type;
    memref->code_offset = pc_offset(output(ctx)) - 4;
    memref->unit_idx = node->id;
    memref->idx = target->id;
    memref->target_type = target->type;
    // Target depth is relative depth, need covertion here.
    memref->depth = node->depth - target->depth;
#if __DEBUG_ASLR__
    plog("[update_branch] 8-bit jmp id: %zu -> %zu (%zu), depth: %zu\n",
         memref->unit_idx,
         memref->idx,
         memref->target_type - 0xe0,
         memref->depth);
#endif
  } else if ((uint8_t)output(ctx)->data[offset - 5] >= 0x80 &&
             (uint8_t)output(ctx)->data[offset - 5] <= 0x8f) {
    // Case of 32-bit jcc
    memref->type = type;
    memref->code_offset = offset - 4;
    memref->unit_idx = node->id;
    memref->idx = target->id;
    memref->target_type = target->type;
    // Target depth is relative depth, need covertion here.
    memref->depth = node->depth - target->depth;
#if __DEBUG_ASLR__
    plog("[update_branch] 32-bit jcc id: %zu -> %zu (%zu), depth: %zu\n",
         memref->unit_idx,
         memref->idx,
         memref->target_type - 0xe0,
         memref->depth);
#endif
  } else {
#if __DEBUG_ASLR__
    plog("[update_branch] unhandled - id: %zu, depth %zu\n",
         node->id,
         node->depth);
#endif
  }
}

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
static void
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
#if __DEBUG_ASLR__
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
#if __DEBUG_ASLR__
          plog("[patch target] if-else branch id: %zu -> %zu\n",
               memrefs->data[i].unit_idx,
               memrefs->data[i].idx);
#endif
        } else if ((control_state == ControlEnd) &&
                   (control_type == CONTROL_IF) &&
                   (memrefs->data[i].depth == node->depth + 1)) {
          memrefs->data[i].idx = node->id;
          memrefs->data[i].target_type = TargetKnown;
#if __DEBUG_ASLR__
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
#if __DEBUG_ASLR__
  plog("[onFunctionStart] fun #%zu, offset: %zu\n",
       func->fun_index,
       pc_offset(output(ctx)));
#else
  (void)ctx;
  (void)func;
#endif

  struct CFGNode* node;
  cfg_init(&cfg);
  if (!cfg_grow(&cfg)) {
    assert(0);
  }
  node = &cfg.data[cfg.size - 1];
  node->id = 0;
  node->offset = 0;
  node->depth = 1;
  node->control_type = CONTROL_BLOCK;
  node->control_state = FunctionStart;
  cfg_target_list_init(&node->target_list);
}

__attribute__((unused)) static void
onFunctionEnd(struct CompilerContext* ctx, const struct Function* func)
{

  (void)ctx;
  (void)func;
}

__attribute__((unused)) static void
onControlStart(struct CompilerContext* ctx,
               const struct Function* func,
               control_type_t control_type,
               size_t depth)
{
  struct CFGNode* node = &cfg.data[cfg.size - 1];
  struct CFGTarget* target = new_target(node);
  size_t offset = pc_offset(output(ctx));
  size_t diff = offset - node->offset;
  size_t new_id = node->id + 1;
  size_t extra_bytes = 0;

  (void)func;

  assert(node != NULL && target != NULL);

  if (node->control_state == FunctionStart) {
    target->id = new_id;
    target->type = TargetKnown;
    target->offset = offset;
    // Only used for unknown target.
    target->depth = 0;
    extra_bytes = insert_jump(ctx, node, target, MEMREF_JMP_NEXT);
    add_unit(ctx, node, diff + extra_bytes);
  } else if (node->control_state == ControlStart) {
    if (control_type == CONTROL_ELSE) {
      // Patch later.
      target->id = 0;
      target->type = ElseUnknown;
    } else {
      target->id = new_id;
      target->type = TargetKnown;
    }
    target->offset = offset;
    // Only used for unknown target.
    target->depth = 0;
    extra_bytes = insert_jump(ctx, node, target, MEMREF_JMP_NEXT);
    add_unit(ctx, node, diff + extra_bytes);
  } else {
    assert(node->control_state == ControlEnd);
    target->id = new_id;
    target->type = TargetKnown;
    // Only used for unknown target.
    target->depth = 0;
    extra_bytes = insert_jump(ctx, node, target, MEMREF_JMP_NEXT);
    add_unit(ctx, node, diff + extra_bytes);
  }

  // Start a new node.
  if (!cfg_grow(&cfg)) {
    assert(0);
  }
  node = &cfg.data[cfg.size - 1];
  node->id = new_id;
  node->offset = offset + extra_bytes;
  node->depth = depth;
  node->control_type = control_type;
  node->control_state = ControlStart;
  cfg_target_list_init(&node->target_list);

  // Patch conditional branch.
  try_patch_target(ctx, node);
}

__attribute__((unused)) static void
onControlEnd(struct CompilerContext* ctx,
             const struct Function* func,
             control_type_t control_type,
             size_t depth)
{
  struct CFGNode* node = &cfg.data[cfg.size - 1];
  struct CFGTarget* target = new_target(node);
  size_t offset = pc_offset(output(ctx));
  size_t diff = offset - node->offset;
  size_t new_id = node->id + 1;
  size_t extra_bytes = 0;

  (void)func;

  assert(node != NULL && target != NULL);

  target->id = new_id;
  target->type = TargetKnown;
  target->offset = offset;
  // Only used for unknown target.
  target->depth = 0;
  extra_bytes = insert_jump(ctx, node, target, MEMREF_JMP_NEXT);
  add_unit(ctx, node, diff + extra_bytes);

  // Add a new node.
  if (!cfg_grow(&cfg)) {
    assert(0);
  }
  node = &cfg.data[cfg.size - 1];
  node->id = new_id;
  node->offset = offset + extra_bytes;
  node->depth = depth;
  node->control_type = control_type;
  node->control_state = ControlEnd;
  cfg_target_list_init(&node->target_list);

  // Patch branch target.
  try_patch_target(ctx, node);
}

__attribute__((unused)) static void
onInstructionStart(struct CompilerContext* ctx,
                   const struct Function* func,
                   const struct Instr* instr)
{
  (void)ctx;
  (void)func;
  (void)instr;
}

__attribute__((unused)) static void
onInstructionEnd(struct CompilerContext* ctx,
                 const struct Function* func,
                 const struct Instr* instr)
{
  (void)func;
  (void)instr;

  // Hook at the end of a function (after ret).
  if (control_depth(control(ctx)) != 0) {
    return;
  }
  struct CFGNode* node = &cfg.data[cfg.size - 1];
  size_t offset = pc_offset(output(ctx));
  size_t diff = offset - node->offset;

  add_unit(ctx, node, diff);
#if __DEBUG_ASLR__
  plog("[onFunctionEnd] fun #%zu, size: %zu\n",
       func->fun_index,
       pc_offset(output(ctx)));
#endif
}

__attribute__((unused)) static void
onMachineInstrEnd(struct CompilerContext* ctx,
                  const struct Function* func,
                  const struct MachineInstr* instr)
{
  (void)func;
  const struct Instr* instr = minstr->instr;
  uint32_t depth = minstr->depth;
  uint8_t instr_type = minstr->type;

  if (instr_type == UcondBranch || instr_type == CondBranch) {
    uint8_t opcode = instr->opcode;
    switch (opcode) {
      case OPCODE_END: {
        struct CFGNode* node = &cfg.data[cfg.size - 1];
        struct CFGTarget* target = new_target(node);
        struct ControlBlock* c = control_at(control(ctx), 0);
        assert(node != NULL && target != NULL);
#if __DEBUG_ASLR__
        plog("[onBranchEnd] end\n");
#endif
        target->id = node->id + 1;
        if (is_onearmed_if(c->type)) {
          // For case of one-armed if, the pattern is the
          // same as else.
          target->type = ElseUnknown;
        } else {
          target->type = TargetKnown;
        }
        target->offset = pc_offset(output(ctx)) - 5;
        target->depth = depth;
        update_branch(ctx, &cfg, node, target, MEMREF_JMP_NEXT);
        break;
      }
      case OPCODE_IF: {
        struct CFGNode* node = &cfg.data[cfg.size - 1];
        struct CFGTarget* target = new_target(node);
        assert(node != NULL && target != NULL);
#if __DEBUG_ASLR__
        plog("[onBranchEnd] if\n");
#endif
        target->id = 0;
        target->type = IfUnknown;
        target->offset = pc_offset(output(ctx)) - 5;
        target->depth = depth;
        update_branch(ctx, &cfg, node, target, MEMREF_JMP_NEXT);
        break;
      }
      case OPCODE_ELSE: {
        struct CFGNode* node = &cfg.data[cfg.size - 1];
        struct CFGTarget* target = new_target(node);
        assert(node != NULL && target != NULL);
#if __DEBUG_ASLR__
        plog("[onBranchEnd] else\n");
#endif
        target->id = 0;
        target->type = ElseUnknown;
        target->offset = pc_offset(output(ctx)) - 5;
        target->depth = depth;
        update_branch(ctx, &cfg, node, target, MEMREF_JMP_NEXT);
        break;
      }
      case OPCODE_BR: {
        struct CFGNode* node = &cfg.data[cfg.size - 1];
        struct CFGTarget* target = new_target(node);
        assert(node != NULL && target != NULL);
#if __DEBUG_ASLR__
        plog("[onBranchEnd] br %zu\n", depth);
#endif
        target->id = 0;
        target->type = BrUnknown;
        target->offset = pc_offset(output(ctx)) - 5;
        target->depth = depth;
        update_branch(ctx, &cfg, node, target, MEMREF_JMP_NEXT);
        break;
      }
      case OPCODE_BR_IF: {
        struct CFGNode* node = &cfg.data[cfg.size - 1];
        struct CFGTarget* target = new_target(node);
        assert(node != NULL && target != NULL);
#if __DEBUG_ASLR__
        plog("[onBranchEnd] br_if %zu\n", depth);
#endif
        target->id = 0;
        target->type = BrUnknown;
        target->offset = pc_offset(output(ctx)) - 5;
        target->depth = depth;
        update_branch(ctx, &cfg, node, target, MEMREF_JMP_NEXT);
        break;
      }
      case OPCODE_BR_TABLE: {
        struct CFGNode* node = &cfg.data[cfg.size - 1];
        struct CFGTarget* target = new_target(node);
        assert(node != NULL && target != NULL);
#if __DEBUG_ASLR__
        plog("[onBranchEnd] br_table %zu\n", depth);
#endif
        target->id = 0;
        target->type = BrUnknown;
        target->offset = pc_offset(output(ctx)) - 5;
        target->depth = depth;
        update_branch(ctx, &cfg, node, target, MEMREF_JMP_NEXT);
        break;
      }
      default: {
        // unhanlded cases.
      }
    }
  }
}
#endif // End of __ASLR__

__attribute__((unused)) static void
PassInit(struct PassManager* pm)
{
  (void)pm;
}

void
add_pass_aslr(struct PassManager* pm)
{
  (void)pm;
#if __ASLR__
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
            NULL,
            onMachineInstrEnd,
            NULL);
  pass_add_dep(pass, "test");
#endif
}
