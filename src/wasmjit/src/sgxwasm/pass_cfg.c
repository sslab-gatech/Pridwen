#include <sgxwasm/pass.h>
#include <sgxwasm/pass_cfg.h>

#if __CFG__
static const char* pass_name = "cfg";

#define JMP_SIZE 5

static DEFINE_VECTOR_INIT(cfg, struct ControlFlowGraph);
static DEFINE_VECTOR_GROW(cfg, struct ControlFlowGraph);
static DEFINE_VECTOR_INIT(fun_cfg, struct FunctionCFG);
DEFINE_VECTOR_GROW(fun_cfg, struct FunctionCFG);
DEFINE_VECTOR_INIT(cfg_target_list, struct CFGTargetList);
static DEFINE_VECTOR_GROW(cfg_target_list, struct CFGTargetList);

static struct ControlFlowGraph cfg = { 0, 0, NULL };
static struct FunctionCFG* fun_cfg;

__attribute__((unused)) static void
dump_cfg(struct ControlFlowGraph* cfg)
{
  size_t i, j, k;
  for (i = 0; i < cfg->size; i++) {
    struct FunctionCFG* fun_cfg = &cfg->data[i];
    printf("fun #%zu\n", fun_cfg->id);
    for (j = 0; j < fun_cfg->size; j++) {
      struct CFGNode* n = &fun_cfg->data[j];
      printf("node #%zu:", n->id);
      for (k = 0; k < n->target_list.size; k++) {
        struct CFGTarget* target = &n->target_list.data[k];
        printf(" %zu", target->id);
      }
      printf("\n");
    }
  }
}

struct CFGTarget*
get_target(struct CFGNode* node)
{
  if (node->target_list.size == 0) {
    return NULL;
  }
  return &node->target_list.data[node->target_list.size - 1];
}

struct CFGTarget*
new_target(struct CFGNode* node)
{
  if (!cfg_target_list_grow(&node->target_list)) {
    return NULL;
  }
  return &node->target_list.data[node->target_list.size - 1];
}

static void
add_unit(struct CompilerContext* ctx, struct CFGNode* node)
{
  assert(ctx->memrefs != NULL);
  struct MemoryReferences* memrefs = ctx->memrefs;
  struct MemoryRef* memref = new_memref(memrefs);
  memref->type = MEMREF_CODE_UNIT;
  memref->code_offset = node->offset;
  memref->unit_idx = node->id;
  memref->size = node->size;
#if __DEBUG_CFG__
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

static void
flush_units(struct CompilerContext* ctx,
            size_t fun_index,
            struct FunctionCFG* fun_cfg)
{
  assert(ctx->memrefs != NULL && fun_cfg != NULL && fun_cfg->id == fun_index);
  size_t i;

  for (i = 0; i < fun_cfg->size; i++) {
    struct CFGNode* node = &fun_cfg->data[i];
    add_unit(ctx, node);
  }
}

static int
is_ending_with_jmp(struct CompilerContext* ctx)
{
  size_t offset = pc_offset(output(ctx));
  if ((uint8_t)output(ctx)->data[offset - 5] == 0xe9) {
    return 1;
  } else if ((uint8_t)output(ctx)->data[offset - 2] == 0xeb) {
    return 1;
  }
  return 0;
}

__attribute__((unused)) static void
update_branch(size_t fun_index,
              struct FunctionCFG* cfg,
              struct CFGNode* node,
              struct CFGTarget* target)
{
  (void)fun_index;
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
#if __DEBUG_CFG__
      plog("[update_branch] fun: %zu br %zu to loop: %zu\n",
           fun_index,
           target->depth,
           target->id);
#endif
    }
  }
#if __DEBUG_CFG__
  plog("[update_branch] fun: %zu id: %zu -> %zu (%zu)\n",
       fun_index,
       node->id,
       target->id,
       target->type - 0xe0);
#endif
  // Unused.
  target->offset = 0;
}

/* Currently there are two cases of branches require patching.
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
try_patch_target(struct FunctionCFG* cfg, struct CFGNode* node)
{
  size_t i, j;
  uint8_t control_state = node->control_state;
  control_type_t control_type = node->control_type;

  for (i = 0; i < cfg->size - 1; i++) {
    struct CFGNode* n = &cfg->data[i];
    for (j = 0; j < n->target_list.size; j++) {
      struct CFGTarget* target = &n->target_list.data[j];
      uint8_t target_type = target->type;
      // Target depth is relative depth, need covertion here.
      size_t depth = n->depth - target->depth;
      switch (target_type) {
        case ElseUnknown:
        case BrUnknown: {
          if (control_state != ControlEnd) {
            break;
          }
          if (depth == node->depth + 1) {
            target->id = node->id;
            target->type = TargetKnown;
#if __DEBUG_CFG__
            plog("[patch target] else/br branch id: %zu -> %zu\n",
                 n->id,
                 target->id);
#endif
          }
          break;
        }
        case IfUnknown: {
          if ((control_state == ControlStart) &&
              (control_type == CONTROL_ELSE) && (depth == node->depth)) {
            target->id = node->id;
            target->type = TargetKnown;
#if __DEBUG_CFG__
            plog("[patch target] if-else branch id: %zu -> %zu\n",
                 n->id,
                 target->id);
#endif
          } else if ((control_state == ControlEnd) &&
                     (control_type == CONTROL_IF) &&
                     (depth == node->depth + 1)) {
            target->id = node->id;
            target->type = TargetKnown;
#if __DEBUG_CFG__
            plog("[patch target] if-end branch id: %zu -> %zu\n",
                 n->id,
                 target->id);
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
}

__attribute__((unused)) static void
onFunctionStart(struct CompilerContext* ctx, const struct Function* func)
{
#if __DEBUG_CFG__
  plog("[onFunctionStart] fun #%zu, offset: %zu\n",
       func->fun_index,
       pc_offset(output(ctx)));
#else
  (void)ctx;
  (void)func;
#endif

  if (!cfg_grow(&cfg)) {
    assert(0);
  }

  fun_cfg = &cfg.data[cfg.size - 1];
  fun_cfg_init(fun_cfg);
  fun_cfg->id = func->fun_index;

  struct CFGNode* node;
  if (!fun_cfg_grow(fun_cfg)) {
    assert(0);
  }
  node = &fun_cfg->data[fun_cfg->size - 1];
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
  assert(fun_cfg != NULL);
  struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
  struct CFGTarget* target;
  size_t offset = pc_offset(output(ctx));
  size_t diff = offset - node->offset;
  size_t new_id = node->id + 1;
  size_t skip = is_ending_with_jmp(ctx);

  (void)func;

  if (skip == 1) {
    // Do not need to add new target.
    plog("skip id: %zu\n", node->id);
    target = NULL;
  } else {
    target = new_target(node);
    if (node->control_state == FunctionStart) {
      target->id = new_id;
      target->type = TargetKnown;
      target->offset = offset;
      target->depth = 0;
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
      target->depth = 0;
    } else {
      assert(node->control_state == ControlEnd);
      target->id = new_id;
      target->type = TargetKnown;
      target->depth = 0;
    }
  }
  node->size = diff;
  plog("[onControlStart] (node size) - (%zu, %zu)\n", node->id, node->size);

  // Start a new node.
  if (!fun_cfg_grow(fun_cfg)) {
    assert(0);
  }
  node = &fun_cfg->data[fun_cfg->size - 1];
  node->id = new_id;
  node->offset = offset;
  node->depth = depth;
  node->control_type = control_type;
  node->control_state = ControlStart;
  cfg_target_list_init(&node->target_list);

  // Patch conditional branch.
  try_patch_target(fun_cfg, node);
}

__attribute__((unused)) static void
onControlEnd(struct CompilerContext* ctx,
             const struct Function* func,
             control_type_t control_type,
             size_t depth)
{
  assert(fun_cfg != NULL);
  struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
  struct CFGTarget* target;
  size_t offset = pc_offset(output(ctx));
  size_t diff = offset - node->offset;
  size_t new_id = node->id + 1;
  size_t skip = is_ending_with_jmp(ctx);

  (void)func;

  assert(node != NULL);

  if (skip == 1) {
    // Do not need to add new target.
    plog("skip id: %zu\n", node->id);
    target = NULL;
  } else {
    target = new_target(node);
    target->id = new_id;
    target->type = TargetKnown;
    target->offset = offset;
    target->depth = 0;
  }
  node->size = diff;
  plog("[onControlEnd] (node size) - (%zu, %zu)\n", node->id, node->size);

  // Add a new node.
  if (!fun_cfg_grow(fun_cfg)) {
    assert(0);
  }
  node = &fun_cfg->data[fun_cfg->size - 1];
  node->id = new_id;
  node->offset = offset;
  node->depth = depth;
  node->control_type = control_type;
  node->control_state = ControlEnd;
  cfg_target_list_init(&node->target_list);

  // Patch branch target.
  try_patch_target(fun_cfg, node);
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
  (void)instr;
  // Hook at the end of a function (after ret).
  if (control_depth(control(ctx)) != 0) {
    return;
  }
  struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
  size_t offset = pc_offset(output(ctx));
  size_t diff = offset - node->offset;

  node->size = diff;

  flush_units(ctx, func->fun_index, fun_cfg);

#if __DEBUG_CFG__
  plog("[onFunctionEnd] fun #%zu, size: %zu\n",
       func->fun_index,
       pc_offset(output(ctx)));
#endif
}

__attribute__((unused)) static void
onBranchEnd(struct CompilerContext* ctx,
            const struct Function* func,
            const struct Instr* instr,
            uint32_t depth)
{
  assert(fun_cfg != NULL);
  uint8_t opcode = instr->opcode;
  switch (opcode) {
    case OPCODE_END: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = new_target(node);
      struct ControlBlock* c = control_at(control(ctx), 0);
      assert(node != NULL && target != NULL);
#if __DEBUG_CFG__
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
      update_branch(func->fun_index, fun_cfg, node, target);
      break;
    }
    case OPCODE_IF: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = new_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_CFG__
      plog("[onBranchEnd] if\n");
#endif
      target->id = 0;
      target->type = IfUnknown;
      target->offset = pc_offset(output(ctx)) - 5;
      target->depth = depth;
      update_branch(func->fun_index, fun_cfg, node, target);
      break;
    }
    case OPCODE_ELSE: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = new_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_CFG__
      plog("[onBranchEnd] else\n");
#endif
      target->id = 0;
      target->type = ElseUnknown;
      target->offset = pc_offset(output(ctx)) - 5;
      target->depth = depth;
      update_branch(func->fun_index, fun_cfg, node, target);
      break;
    }
    case OPCODE_BR: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = new_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_CFG__
      plog("[onBranchEnd] br %zu\n", depth);
#endif
      target->id = 0;
      target->type = BrUnknown;
      target->offset = pc_offset(output(ctx)) - 5;
      target->depth = depth;
      update_branch(func->fun_index, fun_cfg, node, target);
      break;
    }
    case OPCODE_BR_IF: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = new_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_CFG__
      plog("[onBranchEnd] br_if %zu\n", depth);
#endif
      target->id = 0;
      target->type = BrUnknown;
      target->offset = pc_offset(output(ctx)) - 5;
      target->depth = depth;
      update_branch(func->fun_index, fun_cfg, node, target);
      break;
    }
    case OPCODE_BR_TABLE: {
      struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
      struct CFGTarget* target = new_target(node);
      assert(node != NULL && target != NULL);
#if __DEBUG_CFG__
      plog("[onBranchEnd] br_table %zu\n", depth);
#endif
      target->id = 0;
      target->type = BrUnknown;
      target->offset = pc_offset(output(ctx)) - 5;
      target->depth = depth;
      update_branch(func->fun_index, fun_cfg, node, target);
      break;
    }
    default: {
      // unhanlded cases.
    }
  }
}

__attribute__((unused)) static void
onMachineInstrStart(struct CompilerContext* ctx,
                  const struct Function* func,
                  const struct MachineInstr* minstr)
{
  (void) ctx;
  (void) func;

  assert(minstr != NULL);
  if (minstr->type == UcondBranch || minstr->type == CondBranch) {
  }
}

__attribute__((unused)) static void
onMachineInstrEnd(struct CompilerContext* ctx,
                  const struct Function* func,
                  const struct MachineInstr* minstr)
{
  assert(minstr != NULL);
  if (minstr->type == UcondBranch || minstr->type == CondBranch) {
    onBranchEnd(ctx, func, minstr->instr, minstr->depth);
  }
}
#endif // End of __CFG__

__attribute__((unused)) static void
PassInit(struct PassManager* pm)
{
  (void)pm;
#if __CFG__
  // Initialize cfg.
  cfg_init(&cfg);
  plog("init\n");
#endif
}

__attribute__((unused)) static void
PassEnd(struct PassManager* pm)
{
  (void)pm;
#if __CFG__
  // Initialize cfg.
  plog("end\n");
  // dump_cfg(&cfg);
#endif
}

struct ControlFlowGraph*
get_cfg(struct Pass* pass)
{
#if __CFG__
  if (strcmp(pass->name, "cfg") != 0) {
    return NULL;
  }
  return &cfg;
#else
  (void)pass;
  return NULL;
#endif
}

void
add_pass_cfg(struct PassManager* pm)
{
  (void)pm;
#if __CFG__
  struct Pass* pass = get_new_pass(pm);
  assert(pass != NULL);
  pass_init(pass,
            pass_name,
            PassInit,
            PassEnd,
            onFunctionStart,
            onFunctionEnd,
            onControlStart,
            onControlEnd,
            onInstructionStart,
            onInstructionEnd,
            onMachineInstrStart,
            onMachineInstrEnd);
  pass_add_dep(pass, "test");
#endif
}
