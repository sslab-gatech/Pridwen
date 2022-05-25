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

// Workaround: Enable LSPECTRE directly.
#ifndef LS
#define LS 0
#endif

// Global variables.
static struct ControlFlowGraph* cfg;
static struct FunctionCFG* fun_cfg;
static int is_caslr_enabled;

// Use to track the number of br_tables.
static int br_table_num;

static uint8_t last_branch;

static int qs_enabled = 0;

// nbench
//#define SkipNum 6
// static size_t skiplist[SkipNum] = { 108, 109, 138, 144, 197, 198 };

// fib
//#define SkipNum 1
// static size_t skiplist[SkipNum] = { 21 };

//#define SkipNum 9
// lighttpd:
// 607:_malloc
// 608:_free
// 609:_calloc
// 610:_realloc
// 611:try_realloc_chunk
// 848:_memcpy
// 849:_memmove
// 850:_memset
// 851:_sbrk
//static size_t skiplist[SkipNum] = { 607, 608, 609, 610, 611, 848, 849, 850,
//851 };

// 18: init_array
// 23: malloc
// 24: free
// 60: memcpy
// 61: memset
// 62: sbrk
//#define SkipNum 5
//static size_t skiplist[SkipNum] = { 28, 29, 78, 79,
//                                    80 }; // poly-c: cholesky, lu, ludcmp
//#define SkipNum 6
//static size_t skiplist[SkipNum] = { 18, 23, 24, 60, 61, 62 }; // poly-c

#define SkipNum 0
static size_t skiplist[SkipNum];

__attribute__((unused)) static int
is_in_skiplist(size_t id)
{
  int i;
  for (i = 0; i < SkipNum; i++) {
    if (id == skiplist[i]) {
      return 1;
    }
  }
  return 0;
}

// static size_t optlist[OptNum] = { 24 }; // fannkuchredux

#define OptNum 0
static size_t optlist[OptNum];
//#define OptNum 1
//static size_t optlist[OptNum] = { 85 }; // lighttpd
//#define OptNum 1
//static size_t optlist[OptNum] = { 10 }; // stackAlloc, 2mm
//#define OptNum 7
//static size_t optlist[OptNum] = { 241, 293, 339, 343, 476, 616, 690 };

__attribute__((unused)) static int
is_in_optlist(size_t id)
{
  int i;
  for (i = 0; i < OptNum; i++) {
    if (id == optlist[i]) {
      return 1;
    }
  }
  return 0;
}

#define LOOP_OPT 1
/*#define LoopOptNum 20
static size_t loop_optlist[OptNum] = { 168, 231, 232, 236, 239, 241, 293, 339,
341, 337,
                                       338, 343, 426, 476, 616, 618, 690,
                                       702, 719, 801};*/

__attribute__((unused)) static int
is_in_loop_optlist(size_t id)
{
  return 1;
#if 0
  if (id >= 700 && id <= 865) {
    return 1;
  }
  if (id >= 600 && id < 700) {
    return 1;
  }
  if (id >= 500 && id < 600) {
    return 1;
  }
  if (id >= 400 && id < 500) {
    return 1;
  }
  if (id >= 300 && id < 400) {
    return 1;
  }
  /*if (id >= 200 && id < 300) {
    return 1;
  }*/
  return 0;
#endif
#if 0
  int i;
  for (i = 0; i < LoopOptNum; i++) {
    if (id == loop_optlist[i]) {
      return 1;
    }
  }
  return 0;
#endif
}

#if FIX_SIZE_UNIT // For fix-sized code units
#define CODE_UNIT_SIZE 256

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

#if 1 // For debug.
/*#define ListSize 27
static size_t whitelist[ListSize] = { 21, 24, 27, 29, 32, 33, 36, 38, 41,
                                      42, 43, 52, 53, 54, 55, 56, 59, 61,
                                      63, 64, 65, 66, 76, 77, 80, 81, 82 };*/

#define ListSize 3
// 27, 59, 63
static size_t whitelist[ListSize] = { 681, 796, 839 };
// static size_t skip_number = 900;
static size_t skip_number = 0;
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

static void
dump_last_five_bytes(uint64_t addr) {
  uint8_t *code = (uint8_t *) addr;
  plog("[Dump last five bytes]");
  for (int i = 0; i < 5; i++) {
    printf(" 0x%x", code[i]);
  }
  printf("\n");
}

__attribute__((unused)) static void
insert_lea(struct CompilerContext* ctx, struct CFGNode* node,
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
       memref->unit_idx, memref->idx, memref->target_type - 0xe0,
       memref->code_offset, memref->depth);
#endif
}

// Used for the skip tsx cases.
__attribute__((unused)) static void
jump_to_next(struct CompilerContext* ctx, struct CFGNode* node,
             struct CFGTarget* target)
{
  assert(ctx->memrefs != NULL);
  struct MemoryReferences* memrefs = ctx->memrefs;
  struct MemoryRef* memref = new_memref(memrefs);

  emit_jmp_32(output(ctx), 0);

  memref->type = MEMREF_LEA_NEXT;
  memref->code_offset = pc_offset(output(ctx)) - 4;
  memref->unit_idx = node->id;
  memref->idx = target->id;
  memref->target_type = target->type;
  // Target depth is relative depth, need covertion here.
  memref->depth = node->depth - target->depth;
#if __DEBUG_TSGX__
  plog(
    "[jump_to_next][add_relo] id: %zu -> %zu (%zu), offset: %lx, depth: %zu\n",
    memref->unit_idx, memref->idx, memref->target_type - 0xe0,
    memref->code_offset, memref->depth);
#endif
}

__attribute__((unused)) static void
jump_to_springboard(struct CompilerContext* ctx, size_t type, size_t is_cond,
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
                           size_t id, size_t type, size_t is_cond,
                           condition_t cond)
{
  struct MemoryReferences* memrefs = ctx->memrefs;
  struct MemoryRef* memref = &memrefs->data[id];

  assert(memref != NULL);
  assert(memref->type == MEMREF_JMP_NEXT);
#if __DEBUG_TSGX__
  size_t offset = memref->code_offset;
#endif

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
  plog("[update_jump_to_springboard] offset %lx -> %lx\n", offset,
       memref->code_offset);
#endif
}

__attribute__((unused)) static size_t
insert_jump(struct CompilerContext* ctx, struct CFGNode* node,
            struct CFGTarget* target, int split, size_t size)
{
  size_t offset = pc_offset(output(ctx));
  size_t start = offset;

  // If the last instruction is already jmp, skip.
  if (size >= 5 &&
      (uint8_t)output(ctx)->data[offset - 5] == 0xe9 &&
      last_branch != OPCODE_BR_IF) {
    // Workaround: to avoid false detection of jumps, adding further check.
    // Type 1: Non-relocated jmps - e9 00 00 00 00
    // Type 2: Pathed near jumps  - e9 xx xx ff ff
    // Other than these two types, do not skip.
    if (target &&
        target->control_type == CONTROL_BLOCK &&
        target->control_state == ControlStart) {
      uint64_t addr = &output(ctx)->data[output(ctx)->size - 5];
      uint8_t* code = (uint8_t *)addr;
      if ((code[1] == 0x00 && code[2] == 0x00 && code[3] == 0x00 && code[4] == 0x00) ||
          (code[3] == 0xff && code[4] == 0xff)) {
#if __DEBUG_TSGX__
    plog("[insert_jump] skip id: %zu, ends with 32-bit jmp, last branch: %x\n",
         node->id, last_branch);
#endif
        return 0;
      }
#if __DEBUG_TSGX__
      plog("[insert_jump] non-skipping case\n");
      dump_last_five_bytes(addr);
#endif
    } else {
#if __DEBUG_TSGX__
    plog("[insert_jump] skip id: %zu, ends with 32-bit jmp, last branch: %x\n",
         node->id, last_branch);
#endif
      return 0;
    }
  } else if (size >= 2 &&
             (uint8_t)output(ctx)->data[offset - 2] == 0xeb &&
             last_branch != OPCODE_BR_IF) {
#if __DEBUG_TSGX__
    plog("[insert_jump] skip id: %zu, ends with 8-bit jmp, last branch: %x\n",
         node->id, last_branch);
#endif
    return 0;
  }

  if (split == 1) {
    insert_lea(ctx, node, target);
    jump_to_springboard(ctx, MEMREF_SPRINGBOARD_NEXT, UcondBranch, COND_NONE);
  } else {
    assert(split == 0);
    // Skip springboard.
    jump_to_next(ctx, node, target);
  }

  return pc_offset(output(ctx)) - start;
}

__attribute__((unused)) static void
update_branch(struct CompilerContext* ctx, struct CFGNode* node,
              struct CFGTarget* target, int split)
{

  size_t offset = pc_offset(output(ctx));

  if (split == 1 && target != NULL && target->type == TargetLoopKnown) {
#if __DEBUG_TSX__
    plog("[update_branch] Target is loop, split = 0\n");
#endif
    split = 0;
  }

  if (split == 1) {
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
//#if LS
      if (qs_enabled == 1) {
        emit_lfence(output(ctx));
      }
//#endif
    } else {
#if __DEBUG_TSGX__
      plog("[update_branch] unhandled - id: %zu, depth %zu\n", node->id,
           node->depth);
      assert(0);
#endif
    }
  } else {
    assert(split == 0);
    assert(ctx->memrefs != NULL);
    struct MemoryReferences* memrefs = ctx->memrefs;
    struct MemoryRef* memref = new_memref(memrefs);
    // Case of unconditional 32-bit jump.
    if ((uint8_t)output(ctx)->data[offset - 5] == 0xe9) {
      memref->type = MEMREF_LEA_NEXT;
      memref->code_offset = offset - 4;
      memref->unit_idx = node->id;
      memref->idx = target->id;
      memref->target_type = target->type;
      // Target depth is relative depth, need covertion here.
      memref->depth = node->depth - target->depth;
    } else if ((uint8_t)output(ctx)->data[offset - 2] == 0xeb) {
      // Convert 8-bit jmp to 32-bit.
      // 0xeb 0x00 -> 0xe9 0x00 0x00 0x00 0x00
      output(ctx)->data[offset - 2] = 0xe9;
      emit(output(ctx), 0x00);
      emit(output(ctx), 0x00);
      emit(output(ctx), 0x00);

      // Case of unconditional 8-bit jump
      memref->type = MEMREF_LEA_NEXT;
      memref->code_offset = pc_offset(output(ctx)) - 4;
      memref->unit_idx = node->id;
      memref->idx = target->id;
      memref->target_type = target->type;
      // Target depth is relative depth, need covertion here.
      memref->depth = node->depth - target->depth;
    } else if ((uint8_t)output(ctx)->data[offset - 5] >= 0x80 &&
               (uint8_t)output(ctx)->data[offset - 5] <= 0x8f) {
      // Case of 32-bit jcc
      memref->type = MEMREF_LEA_NEXT;
      memref->code_offset = offset - 4;
      memref->unit_idx = node->id;
      memref->idx = target->id;
      memref->target_type = target->type;
      // Target depth is relative depth, need covertion here.
      memref->depth = node->depth - target->depth;
//#if LS
      if (qs_enabled == 1) {
        emit_lfence(output(ctx));
      }
//#endif
    } else {
#if __DEBUG_TSGX__
      plog("[update_branch] unhandled - id: %zu, depth %zu\n", node->id,
           node->depth);
      assert(0);
#endif
    }
  }
}

__attribute__((unused)) size_t
update_aslr_branch(struct CompilerContext* ctx, struct CFGNode* node,
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
#if __DEBUG_TSGX__
  size_t original_offset = memref->code_offset;
#endif
  // Simply update the relocation entry of the jmp/jcc inserted by pass_caslr.
  if ((uint8_t)output(ctx)->data[offset - 5] == 0xe9) {
    output_buf_shrink(output(ctx), 5);
    // spill_register(ctx, GP_RAX);
    insert_lea(ctx, node, target);
    update_jump_to_springboard(ctx, memref_index, MEMREF_SPRINGBOARD_NEXT,
                               UcondBranch, COND_NONE);
  } else if ((uint8_t)output(ctx)->data[offset - 5] >= 0x80 &&
             (uint8_t)output(ctx)->data[offset - 5] <= 0x8f) {
    condition_t cond = (uint8_t)output(ctx)->data[offset - 5] & 0xf;
    output_buf_shrink(output(ctx), 6);
    // spill_register(ctx, GP_RAX);
    insert_lea(ctx, node, target);
    update_jump_to_springboard(ctx, memref_index, MEMREF_SPRINGBOARD_NEXT,
                               CondBranch, cond);
  } else {
    // Should not have other cases.
    assert(0);
  }
#if __DEBUG_TSGX__
  plog("[update_aslr_branch] update offset: %lx -> %lx\n", original_offset,
       memref->code_offset);
#endif
  return pc_offset(output(ctx)) - offset;
}

#if FIX_SIZE_UNIT
__attribute__((unused)) static void
update_memref(struct CompilerContext* ctx, size_t start_offset,
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
      printf("[update_memref] %zu -> %zu (%u), offset %lx", memref->unit_idx,
             memref->idx, memref->type, offset);
#endif
      memref->code_offset += extra_bytes;
#if __DEBUG_TSGX_SPLIT__
      printf(" -> %lx (%lx - %lx)\n", memref->code_offset, start_offset,
             end_offset);
#endif
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

#if 1 // For debug.
  const struct Function* func = ctx->func;

  if (is_in_whitelist(func->fun_index) != 1) {
    // Reset the fix_size_unit.
#if __DEBUG_TSGX__
    plog("[split_unit] skip fun #%zu\n", func->fun_index);
#endif
    reset_unit(code_unit);
    add_instr(code_unit, pc_offset(output(ctx)), 0); // Head node.
    return;
  }

/*node = &fun_cfg->data[fun_cfg->size - 1];
if (node->id >= skip_number) {
  //plog("[split_unit] skip fun #%zu, unit: %zu\n", func->fun_index, node->id);
  return;
}*/
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
    plog("Search for split point - size: %zu, offset %lx\n", instr->size,
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
    plog("split_offset: %lx, offset: %lx, diff: %zu\n", split_offset, offset,
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
  insert_jump(ctx, node, target, 0);

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
#if __DEBUG_TSGX__
          plog("[patch target] if-else branch id: %zu -> %zu\n",
               memrefs->data[i].unit_idx, memrefs->data[i].idx);
#endif
        } else if ((control_state == ControlEnd) &&
                   (control_type == CONTROL_IF) &&
                   (memrefs->data[i].depth == node->depth + 1)) {
          memrefs->data[i].idx = node->id;
          memrefs->data[i].target_type = TargetKnown;
#if __DEBUG_TSGX__
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

  (void)offset;

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
#if TSX_SUPPORT
    label_t label = { 0, 0 };
    bind_label(output(ctx), &label, pc_offset(output(ctx)));
    emit_xbegin(output(ctx), &label);
    /*label_t next_addr = { 0, 0 };
    build_operand(&next_addr, REG_UNKNOWN, REG_UNKNOWN, SCALE_NONE, 5);
    emit_lea_rm(output(ctx), GP_R15, &next_addr, VALTYPE_I64);
    jump_to_springboard(ctx, MEMREF_SPRINGBOARD_BEGIN, UcondBranch, COND_NONE);*/
#endif
  }
#if 0
  else if (func->fun_index == 0) { /* sum benchmark */
    label_t label = { 0, 0 };
    bind_label(output(ctx), &label, pc_offset(output(ctx)));
    emit_xbegin(output(ctx), &label);
  }
#endif
#if 0
  else if (func->fun_index == 1) { /* fib benchmark*/
    label_t label = { 0, 0 };
    bind_label(output(ctx), &label, pc_offset(output(ctx)));
    emit_xbegin(output(ctx), &label);
  }
#endif
}

__attribute__((unused)) static void
onFunctionEnd(struct CompilerContext* ctx, const struct Function* func)
{

  (void)ctx;
  (void)func;

  if (func->name && !strcmp(func->name, "_main")) {
#if TSX_SUPPORT
    emit_xend(output(ctx));
#endif
  }
#if 0
  else if (func->fun_index == 0) { /* sum benchmark */
    emit_xend(output(ctx));
  }
#endif
#if 0
  else if (func->fun_index == 1) { /* fib benchmark */
    emit_xend(output(ctx));
  }
#endif
}

__attribute__((unused)) static void
onControlStart(struct CompilerContext* ctx, const struct Function* func,
               control_type_t control_type, size_t depth)
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
  int split = 1;

  (void)func;
  (void)control_type;
  (void)depth;

  assert(node != NULL && next_node != NULL);

  if (fun_index < num_imports) {
    return;
  }

  if (target != NULL && target->type == TargetLoopKnown) {
#if __DEBUG_TSX__
    plog("[onControlStart] Target is loop, split = 0\n");
#endif
    split = 0;
  }

#if LOOP_OPT
  if (is_in_loop_optlist(fun_index)) {
    if (next_node->control_type == CONTROL_LOOP) {
      split = 1;
    } else {
      split = 0;
    }
  }
#endif

  if (is_in_skiplist(fun_index) || is_in_optlist(fun_index)) {
    split = 0;
  }

  // If pass_calsr is enabled already, we only need to
  // update the branch as it should be inserted already.
  if (!is_caslr_enabled) {
    extra_bytes = insert_jump(ctx, node, target, split, size);
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
onControlEnd(struct CompilerContext* ctx, const struct Function* func,
             control_type_t control_type, size_t depth)
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
  int split = 1;

  (void)func;
  (void)control_type;
  (void)depth;

  assert(node != NULL && next_node != NULL);

  if (fun_index < num_imports) {
    return;
  }

  if (target != NULL && target->type == TargetLoopKnown) {
#if __DEBUG_TSGX__
    plog("[onControlEnd] Target is loop, skip = 1\n");
#endif
    split = 0;
  }

#if LOOP_OPT
  if (is_in_loop_optlist(fun_index)) {
    if (next_node->control_type == CONTROL_LOOP) {
      split = 1;
    } else {
      split = 0;
    }
  }
#endif

  if (is_in_skiplist(fun_index) || is_in_optlist(fun_index)) {
    split = 0;
  }

  // If pass_calsr is enabled already, we only need to
  // update the branch as it should be inserted already.
  if (!is_caslr_enabled) {
    extra_bytes = insert_jump(ctx, node, target, split, size);
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
onInstructionStart(struct CompilerContext* ctx, const struct Function* func,
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

  if (is_in_skiplist(fun_index) || is_in_optlist(fun_index)) {
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
onInstructionEnd(struct CompilerContext* ctx, const struct Function* func,
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

  if (is_in_skiplist(fun_index) || is_in_optlist(fun_index)) {
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
onBranchEnd(struct CompilerContext* ctx, const struct Instr* instr,
            uint32_t depth, int split)
{
  assert(fun_cfg != NULL);
  (void)depth;

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
        update_branch(ctx, node, target, split);
      } else {
        update_aslr_branch(ctx, node, target);
      }
      last_branch = OPCODE_END;
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
        update_branch(ctx, node, target, split);
      } else {
        update_aslr_branch(ctx, node, target);
      }
      last_branch = OPCODE_IF;
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
        update_branch(ctx, node, target, split);
      } else {
        update_aslr_branch(ctx, node, target);
      }
      last_branch = OPCODE_ELSE;
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
        update_branch(ctx, node, target, split);
      } else {
        update_aslr_branch(ctx, node, target);
      }
      last_branch = OPCODE_BR;
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
        update_branch(ctx, node, target, split);
      } else {
        update_aslr_branch(ctx, node, target);
      }
      last_branch = OPCODE_BR_IF;
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
        update_branch(ctx, node, target, split);
      } else {
        update_aslr_branch(ctx, node, target);
      }
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

  size_t fun_index = func->fun_index;
  const struct Module* module = sgxwasm_get_module(func);
  size_t num_imports = module->n_imported_funcs;
  if (fun_index < num_imports) {
    return;
  }

  assert(minstr != NULL);
  switch (minstr->type) {
#if FIX_SIZE_UNIT
    case BrTableTarget:
    case BrCaseTarget: {
      // Set the label to bypass bind.
      minstr->label->pos = -1;
      break;
    }
#endif
    case BindLabel: {
      // Set the label to bypass bind.
      minstr->label->pos = -1;
      break;
    }
    case CallFunction: {
      uint32_t target_index = minstr->fun_index;
      if (is_in_optlist(fun_index) || is_in_skiplist(fun_index)) {
        break;
      }
      if (target_index < num_imports || is_in_skiplist(target_index)) {
#if TSX_SUPPORT
        emit_xend(output(ctx));
#endif
      }
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
  size_t fun_index = func->fun_index;
  const struct Module* module = sgxwasm_get_module(func);
  size_t num_imports = module->n_imported_funcs;
  int split = 1;
  struct CFGNode* node = &fun_cfg->data[fun_cfg->size - 1];
  if (fun_index < num_imports) {
    return;
  }

#if LOOP_OPT
  if (is_in_loop_optlist(fun_index)) {
    if (node->control_type == CONTROL_LOOP) {
      split = 1;
    } else {
      split = 0;
    }
  }
#endif

  // Force not to split.
  if (is_in_skiplist(fun_index) || is_in_optlist(fun_index)) {
    split = 0;
  }

  assert(minstr != NULL);
  switch (minstr->type) {
    case UcondBranch:
    case CondBranch: {
      const struct Instr* instr = minstr->instr;
      onBranchEnd(ctx, instr, minstr->depth, split);
      break;
    }
#if FIX_SIZE_UNIT
    case BrTableJmp:
    case BrTableTarget:
    case BrCaseJmp:
    case BrCaseTarget: {
      onLowBrTable(ctx, minstr);
      break;
    }
#endif
    case CallFunction: {
      uint32_t target_index = minstr->fun_index;
      if (is_in_optlist(fun_index) || is_in_skiplist(fun_index)) {
        break;
      }
      if (target_index < num_imports || is_in_skiplist(target_index)) {
#if TSX_SUPPORT
        label_t label = { 0, 0 };
        bind_label(output(ctx), &label, pc_offset(output(ctx)));
        emit_xbegin(output(ctx), &label);
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
  (void)opcode;
#if __DEBUG_TSGX_SPLIT__
  if (diff >= CODE_UNIT_SIZE) {
    plog("Low Inst %x, type: %u, size: %zu (large), counter: %zu\n", opcode,
         minstr->type, diff, size);
  } else {
    plog("Low Inst %x, type: %u, size: %zu, counter: %zu\n", opcode,
         minstr->type, diff, size);
  }
#endif
  if (size > (CODE_UNIT_SIZE - JMP_SIZE)) {
#if __DEBUG_TSGX_SPLIT__
    plog("Find low split point: (prev: %zu, curr: %zu)\n", prev_instr->size,
         size);
#endif
    split_unit(ctx, &fix_size_unit);

  } else {
    add_instr(&fix_size_unit, offset, size);
  }
#endif
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

  if (size == 0) {
    return;
  }

  size_t end_offset = size - 1;
  if (code[end_offset - 4] == 0xe9) {
    if (code[end_offset - 4 - 6] == 0x8d) {
      // plog("found lea-jmp\n");
    } else {
      // plog("found jmp\n");
    }
    return;
  }

  if (code[end_offset - 4] >= 0x80 && code[end_offset - 4] <= 0x8f) {
    if (code[end_offset - 4 - 6] == 0x8d) {
      // plog("found lea-jcc\n");
    } else {
      // plog("found jcc\n");
    }
    return;
  }
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
      //is_caslr_enabled = 1;
      break;
    }
  }
  assert(cfg);

  if (pass_is_enabled(pm, "qspectre")) {
    qs_enabled = 1;
  }

#endif
}

void
add_pass_tsgx(struct PassManager* pm)
{
  (void)pm;
#if __TSGX__
  struct Pass* pass = get_new_pass(pm);
  assert(pass != NULL);
  pass_init(pass, pass_name, PassInit, NULL, onFunctionStart, onFunctionEnd,
            onControlStart, onControlEnd, onInstructionStart, onInstructionEnd,
            onMachineInstrStart, onMachineInstrEnd, Validation);
  pass_add_dep(pass, "cfg");
#endif
}
