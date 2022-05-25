#include <sgxwasm/relocate.h>

// Define grow functions.

DEFINE_VECTOR_GROW(relo_entry, struct RelocationEntries);
DEFINE_VECTOR_GROW(code_unit, struct CodeUnits);

int
init_relo_table(struct RelocationTable* table, size_t number_funs)
{
  table->entries = malloc(sizeof(struct RelocationEntries) * number_funs);
  if (!table->entries)
    return 0;

  table->size = number_funs;
  size_t i;
  for (i = 0; i < number_funs; i++) {
    table->entries[i].capacity = 0;
    table->entries[i].size = 0;
    table->entries[i].data = NULL;
  }
  return 1;
}

void
free_relo_table(struct RelocationTable* table, size_t number_funs)
{
  size_t i;
  assert(table->size == number_funs);
  for (i = 0; i < number_funs; i++) {
    if (table->entries[i].data) {
      free(table->entries[i].data);
    }
  }
  free(table->entries);
}

int
init_code_unit_table(struct CodeUnitTable* table, size_t number_funs)
{
  table->units = malloc(sizeof(struct CodeUnits) * number_funs);
  if (!table->units)
    return 0;

  table->size = number_funs;
  size_t i;
  for (i = 0; i < number_funs; i++) {
    table->units[i].entry_offset = 0;
    table->units[i].capacity = 0;
    table->units[i].size = 0;
    table->units[i].data = NULL;
  }
  return 1;
}

void
free_code_unit_table(struct CodeUnitTable* table, size_t number_funs)
{
  size_t i;
  assert(table->size == number_funs);
  for (i = 0; i < number_funs; i++) {
    if (table->units[i].data) {
      free(table->units[i].data);
    }
  }
  free(table->units);
}

void
set_code_entry_offset(struct CodeUnitTable* table,
                      size_t fun_index,
                      uint64_t addr)
{
  assert(fun_index < table->size);
  assert(table->units[fun_index].entry_offset == 0);
  uint64_t code_base = sgxwasm_get_code_base();

  table->units[fun_index].entry_offset = addr - code_base;
#if DEBUG_RELOCATE
#if __linux__
  printf(
    "[set_code_entry_offset] fun_index: %zu, addr: 0x%lx\n", fun_index, addr);
#else
  printf(
    "[set_code_entry_offset] fun_index: %zu, addr: 0x%llx\n", fun_index, addr);
#endif
#endif
}

void
add_relo_entry(struct RelocationTable* table,
               size_t fun_index,
               relocation_type_t type,
               struct MemoryRef* memref)
{
  size_t offset = memref->code_offset;
  size_t target_index = memref->idx;
  // assert(fun_index < table->size && target_index < table->size);
  size_t unit_index = memref->unit_idx;
  size_t unit_size = memref->size;

  struct RelocationEntries* entries = &table->entries[fun_index];
  if (!relo_entry_grow(entries))
    assert(0);

  size_t num_entries = entries->size;
  struct Entry* entry = &entries->data[num_entries - 1];
  entry->type = type;
  entry->offset = offset;
  entry->target_index = target_index;
  entry->unit_index = unit_index;
  entry->unit_size = unit_size;
#if __DEBUG_ASLR__
  if (type == RELO_CODE_UNIT) {
    printf("[add_relo_entry] fun: %zu, unit %zu, offset: %lld, size: %zu\n",
           fun_index,
           entry->unit_index,
           entry->offset,
           entry->unit_size);
  }
  if (type == RELO_JMP_NEXT) {
    printf("[add_relo_entry] fun: %zu, unit: %zu -> %zu (%zu), offset: %lld\n",
           fun_index,
           entry->unit_index,
           entry->target_index,
           memref->target_type - 0xe0,
           entry->offset);
  }
#endif
#if DEBUG_RELOCATE
  printf("[add_relo_entry] fun_index: %zu, offset: %lld, target_index: %zu\n",
         fun_index,
         entry->offset,
         entry->target_index);
#endif
}

void
relocate(struct Module* module,
         struct RelocationTable* table,
         struct IndirectCallTable* indirect_table,
         struct CodeUnitTable* code_table,
         struct Springboard* springboard,
         uint64_t ssa_polling_addr,
         int use_code_unit)
{
  size_t i = 0;
  uint64_t code_base = sgxwasm_get_code_base();
  for (i = 0; i < table->size; i++) {
    struct RelocationEntries* entries = &table->entries[i];
    struct CodeUnits* unit_list = &code_table->units[i];
    size_t j;
    for (j = 0; j < entries->size; j++) {
      struct Entry* entry = &entries->data[j];
      struct CodeUnit* unit;
      int64_t code_offset;
      uint64_t relo_addr;
      if (use_code_unit) {
        unit = &unit_list->data[entry->unit_index];
        code_offset = unit->offset;
      } else {
        code_offset = unit_list->entry_offset;
      }
      relo_addr = code_base + code_offset + entry->offset;
      switch (entry->type) {
        case RELO_CALL: {
          size_t target_index = entry->target_index; // Function index.
          assert(target_index < table->size);
          int64_t target_offset = code_table->units[target_index].entry_offset;
          uint64_t target_val = code_base + target_offset;
          encode_le_uint64_t(target_val, (char*)relo_addr);
#if DEBUG_RELOCATE
#if __linux__
          printf("[relocate_call] addr 0x%lx <- target_val 0x%lx\n",
                 relo_addr,
                 target_val);
#else
          printf("[relocate_call] addr 0x%llx <- target_val 0x%llx\n",
                 relo_addr,
                 target_val);
#endif
#endif
          break;
        }
        case RELO_MEM: {
          size_t target_index = entry->target_index;
          uint64_t target_val =
            (uintptr_t)module->mems.data[target_index]->data;
          encode_le_uint64_t(target_val, (char*)relo_addr);
#if DEBUG_RELOCATE
#if __linux__
          printf("[relocate_mem] addr 0x%lx <- target_val 0x%lx\n",
                 relo_addr,
                 target_val);
#else
          printf("[relocate_mem] addr 0x%llx <- target_val 0x%llx\n",
                 relo_addr,
                 target_val);
#endif
#endif
          break;
        }
        case RELO_GLOBAL: {
          size_t target_index = entry->target_index;
          uint64_t target_val =
            (uintptr_t)&module->globals.data[target_index]->value.val;
          encode_le_uint64_t(target_val, (char*)relo_addr);
#if DEBUG_RELOCATE
#if __linux__
          printf("[relocate_global] addr 0x%lx <- target_val 0x%lx\n",
                 relo_addr,
                 target_val);
#else
          printf("[relocate_global] addr 0x%llx <- target_val 0x%llx\n",
                 relo_addr,
                 target_val);
#endif
#endif
          break;
        }
        case RELO_TABLE_SIGS: {
          uint64_t target_val = (uint64_t)indirect_table->sigs;
          encode_le_uint64_t(target_val, (char*)relo_addr);
#if DEBUG_RELOCATE
#if __linux__
          printf("[relocate_table_sigs] addr 0x%lx <- target_val 0x%lx\n",
                 relo_addr,
                 target_val);
#else
          printf("[relocate_table_sigs] addr 0x%llx <- target_val 0x%llx\n",
                 relo_addr,
                 target_val);
#endif
#endif
          break;
        }
        case RELO_TABLE_REFS: {
          uint64_t target_val = (uint64_t)indirect_table->refs;
          encode_le_uint64_t(target_val, (char*)relo_addr);
#if DEBUG_RELOCATE
#if __linux__
          printf("[relocate_table_refs] addr 0x%lx <- target_val 0x%lx\n",
                 relo_addr,
                 target_val);
#else
          printf("[relocate_table_refs] addr 0x%llx <- target_val 0x%llx\n",
                 relo_addr,
                 target_val);
#endif
#endif
          break;
        }
        case RELO_TABLE_SIZE: {
          uint64_t target_val = (uint64_t)indirect_table->size;
          encode_le_uint64_t(target_val, (char*)relo_addr);
#if DEBUG_RELOCATE
#if __linux__
          printf("[relocate_table_size] addr 0x%lx <- target_val 0x%lx\n",
                 relo_addr,
                 target_val);
#else
          printf("[relocate_table_size] addr 0x%llx <- target_val 0x%llx\n",
                 relo_addr,
                 target_val);
#endif
#endif
          break;
        }
        // T-SGX support.
        // Note that we assume the offset between the patching address
        // and the springboard is less than 32-bit long. Otherwise,
        // using jmp is not sufficient.
        case RELO_SPRINGBOARD_BEGIN: {
          const uint32_t jmp_offset = 4;
          uint32_t target_val = springboard->begin - relo_addr - jmp_offset;
          encode_le_uint32_t(target_val, (char*)relo_addr);
#if __DEBUG_TSGX__
#if __linux__
#else
          printf(
            "[relocate_springboard_begin] addr 0x%llx <- target_val 0x%x\n",
            relo_addr,
            target_val);
#endif
#endif
          break;
        }
        case RELO_SPRINGBOARD_NEXT: {
          const uint32_t jmp_offset = 4;
          uint32_t target_val = springboard->next - relo_addr - jmp_offset;
          encode_le_uint32_t(target_val, (char*)relo_addr);
#if __DEBUG_TSGX__
#if __linux__
#else
          printf("[relocate_springboard_next] addr 0x%llx <- target_val 0x%x\n",
                 relo_addr,
                 target_val);
#endif
#endif
          break;
        }
        case RELO_SPRINGBOARD_END: {
          const uint32_t jmp_offset = 4;
          uint32_t target_val = springboard->end - relo_addr - jmp_offset;
          encode_le_uint32_t(target_val, (char*)relo_addr);
#if __DEBUG_TSGX__
#if __linux__
#else
          printf("[relocate_springboard_end] addr 0x%llx <- target_val 0x%x\n",
                 relo_addr,
                 target_val);
#endif
#endif
          break;
        }
        // End of TSGX support.
        // ASLR support
        case RELO_JMP_NEXT: {
          const uint32_t jmp_offset = 4;
          size_t target_index = entry->target_index; // Unit index.
          struct CodeUnit* target = &unit_list->data[target_index];
          // For non-ASLR case.
          // uint64_t code_base = unit_list->entry_offset + global_code_base;
          uint32_t target_val;

          while (target_index < unit_list->size) {
            target = &unit_list->data[target_index];
            if (target->size != 0) {
              break;
            }
            target_index++;
            target = NULL;
          }
          assert(target != NULL);
          // Target calculation: target address - current address - jmp offset
          target_val = code_base + target->offset - relo_addr - jmp_offset;
          encode_le_uint32_t(target_val, (char*)relo_addr);
#if __DEBUG_CASLR__
#if __linux__
          printf("[relocate_jmp_next] addr 0x%lx <- target_val 0x%x\n",
                 relo_addr,
                 target_val);
#else
          printf("[relocate_jmp_next] addr 0x%llx <- target_val 0x%x\n",
                 relo_addr,
                 target_val);
#endif
#endif
          break;
        }
        case RELO_LEA_NEXT: {
          const uint32_t ucond_offset = 4;
          size_t target_index = entry->target_index; // Unit index.
          struct CodeUnit* target = &unit_list->data[target_index];
          // For non-ASLR case.
          // uint64_t code_base = unit_list->entry_offset + global_code_base;
          uint32_t target_val;

          while (target_index < unit_list->size) {
            target = &unit_list->data[target_index];
            if (target->size != 0) {
              break;
            }
            target_index++;
            target = NULL;
          }
          assert(target != NULL);
          // Target calculation: target address - current address - offset
          target_val = code_base + target->offset - relo_addr - ucond_offset;
          encode_le_uint32_t(target_val, (char*)relo_addr);
#if __DEBUG_TSGX__
#if __linux__
          printf("[relocate_lea_next] addr 0x%lx <- target_val 0x%x\n",
                 relo_addr,
                 target_val);
#else
          printf("[relocate_lea_next] addr 0x%llx <- target_val 0x%x\n",
                 relo_addr,
                 target_val);
#endif
#endif
          break;
        }
        // Varys support
        case RELO_SSA_POLLING: {
          assert(ssa_polling_addr != 0);
          encode_le_uint64_t(ssa_polling_addr, (char*)relo_addr);
#if __DEBUG_VARYS__
#if __linux__
          printf("[relocate_ssa_polling] addr 0x%lx <- target_val 0x%lx\n",
                 relo_addr,
                 ssa_polling_addr);
#else
          printf("[relocate_ssa_polling] addr 0x%llx <- target_val 0x%llx\n",
                 relo_addr,
                 ssa_polling_addr);
#endif
#endif
          break;
        }
        // Fix-sized ASLR support
        case RELO_BR_TABLE_JMP: {
          const uint32_t jmp_offset = 4;
          size_t target_index = entry->target_index;
          struct CodeUnit* target = NULL;
          uint32_t t_offset;
          uint32_t target_val = 0;
          size_t k;
#if DEBUG__RELOCATE
          printf("[relocate] BrTableJmp id: %lx\n", target_index);
#endif
          for (k = 0; k < entries->size; k++) {
            struct Entry* t = &entries->data[k];
            if (t->type != RELO_BR_TABLE_TARGET) {
              continue;
            }
            if (t->target_index == target_index) {
              target = &unit_list->data[t->unit_index];
              t_offset = t->offset;
              break;
            }
          }
          assert(target != NULL);
          target_val =
            code_base + target->offset + t_offset - relo_addr - jmp_offset;
          encode_le_uint32_t(target_val, (char*)relo_addr);
          break;
        }
        case RELO_BR_CASE_JMP: {
          const uint32_t jmp_offset = 4;
          size_t target_index = entry->target_index;
          struct CodeUnit* target = NULL;
          uint32_t t_offset;
          uint32_t target_val = 0;
          size_t k;
#if DEBUG__RELOCATE
          printf("[relocate] BrCaseJmp id: %lx\n", target_index);
#endif
          for (k = 0; k < entries->size; k++) {
            struct Entry* t = &entries->data[k];
            if (t->type != RELO_BR_CASE_TARGET) {
              continue;
            }
            if (t->target_index == target_index) {
              target = &unit_list->data[t->unit_index];
              t_offset = t->offset;
              break;
            }
          }
          assert(target != NULL);
          target_val =
            code_base + target->offset + t_offset - relo_addr - jmp_offset;
          encode_le_uint32_t(target_val, (char*)relo_addr);
          break;
        }
        default: {
          break;
        }
      }
    }
  }
}

// End of relocation.

void
DebugSpringboard(uint64_t addr, uint64_t rax)
{
#if __linux__
  printf("[Springboard] @0x%lx, rax: %lu\n", addr, rax);
#else
  printf("[Springboard] @0x%llx, rax: %llu\n", addr, rax);
#endif
}

// T-SGX Support.
void
construct_springboard(struct Springboard* springboard)
{
  void* mapped;
  struct SizedBuffer output = { 0, 0, NULL };

  // springboard:
  // next:
  //     xend
  //     mov r14, rax # store rax
  // begin:
  //     xbegin begin
  //     mov rax, r14 # restore rax upon retry
  //     jmp r15
  // end:
  //     xend
  //     jmp r15
  label_t begin = { 0, 0 };
  springboard->next = pc_offset(&output);
  emit_mov_rr(&output, GP_R14, GP_RAX, VALTYPE_I64);
#if TSX_SUPPORT
  emit_xend(&output);
#endif

#if TSX_SIMULATE && !TSX_SUPPORT
  label_t label = { 0, 0 };
  bind_label(&output, &label, pc_offset(&output));
  emit_xbegin(&output, &label);
  emit_xend(&output);
#endif

  bind_label(&output, &begin, pc_offset(&output));
#if 0
  // Debug code
  SaveRegisterStates(&output);
  emit_mov_rr(&output, GP_RDI, GP_R15, VALTYPE_I64);
  emit_mov_rr(&output, GP_RSI, GP_RAX, VALTYPE_I64);
  emit_movq_ri(&output, GP_RAX, (uint64_t)DebugSpringboard);
  emit_call_r(&output, GP_RAX);
  RestoreRegisterStates(&output);
#endif
  springboard->begin = pc_offset(&output);
#if TSX_SUPPORT
  emit_xbegin(&output, &begin);
#endif
  emit_mov_rr(&output, GP_RAX, GP_R14, VALTYPE_I64);
  emit_jmp_r(&output, GP_R15);
  springboard->end = pc_offset(&output);
#if TSX_SUPPORT
  emit_xend(&output);
#endif
  emit_jmp_r(&output, GP_R15);

  mapped = sgxwasm_allocate_code(output.size, PageSize, 0);
  memcpy(mapped, output.data, output.size);

  springboard->begin += (uint64_t)mapped;
  springboard->next += (uint64_t)mapped;
  springboard->end += (uint64_t)mapped;
  if (!sgxwasm_mark_code_segment_executable(mapped, output.size)) {
    assert(0);
  }
#if __DEBUG_TSGX__
  size_t i;
  printf("springboard: \n");
  for (i = 0; i < output.size; i++) {
    uint8_t byte = ((uint8_t*)mapped)[i];
    if (byte < 16) {
      printf("0%x ", byte);
      continue;
    }
    printf("%x ", byte);
  }
  printf("\n");
#endif
}

void
add_code_unit(struct CodeUnitTable* table,
              size_t fun_index,
              struct MemoryRef* memref)
{
  size_t offset = memref->code_offset;
  size_t size = memref->size;

  struct CodeUnits* unit_list = &table->units[fun_index];
  if (!code_unit_grow(unit_list))
    assert(0);

  size_t num_units = unit_list->size;
  struct CodeUnit* unit = &unit_list->data[num_units - 1];
  unit->offset = offset;
  unit->size = size;
#if __DEBUG_ASLR__
  printf("[add_code_unit] fun: %zu, unit %zu, offset: %llu, size: %zu\n",
         fun_index,
         num_units - 1,
         unit->offset,
         unit->size);
#endif
}

void
convert_to_unit_relo_info(size_t fun_index,
                          struct RelocationTable* relo_table,
                          struct CodeUnitTable* code_table)
{
  assert(fun_index < relo_table->size && fun_index < code_table->size);
  struct RelocationEntries* entries = &relo_table->entries[fun_index];
  struct CodeUnits* unit_list = &code_table->units[fun_index];
  size_t i, j;
  assert(entries != NULL && unit_list != NULL);
  for (i = 0; i < entries->size; i++) {
    struct Entry* entry = &entries->data[i];
    for (j = unit_list->size - 1; j != 0; j--) {
      struct CodeUnit* unit = &unit_list->data[j];
      if (entry->offset >= unit->offset) {
#if __DEBUG_ASLR__ || __DEBUG_TSGX__
        printf("[convert_unit_relo_into] fun %zu offset: %llx -> %llx, id: %zu "
               "-> %zu\n",
               fun_index,
               entry->offset,
               entry->offset - unit->offset,
               entry->unit_index,
               j);
#endif
        entry->offset -= unit->offset;
        entry->unit_index = j;
        break;
      }
    }
  }
}

// ASLR support.
void*
randomize_layout(struct Function* func,
                 struct CodeUnitTable* table,
                 void* code,
                 size_t code_size,
                 int rand)
{
  assert(func != NULL);
  size_t fun_index = func->fun_index;
  assert(fun_index < table->size);
  struct CodeUnits* unit_list = &table->units[fun_index];
  assert(unit_list != NULL);
  uint64_t code_base = sgxwasm_get_code_base();
  void* fun_entry = NULL;
  size_t track_size = 0;
  size_t i;

  for (i = 0; i < unit_list->size; i++) {
    struct CodeUnit* unit = &unit_list->data[i];
    size_t size = unit->size;
    uint64_t offset = unit->offset;
    uint64_t src, dst;
    if (size == 0) {
      continue;
    }
    src = (uint64_t)code + offset;
    dst = (uint64_t)sgxwasm_allocate_code(size, UnitSize, rand);

    memcpy((void*)dst, (void*)src, size);
    unit->offset = dst - code_base;
#if DEBUG_RELOCATE
#if __linux__
    printf("[randomize_layout] fun #%zu, id: %zu, offset: %lx (+%zu)\n",
           fun_index,
           i,
           unit->offset,
           size);
#else
    printf("[randomize_layout] fun #%zu, id: %zu, offset: %llx (+%zu)\n",
           fun_index,
           i,
           unit->offset,
           size);
#endif
#endif
    track_size += size;

    // Set the entry offset to the function.
    if (i == 0) {
      fun_entry = (void*)dst;
    }
  }
#if DEBUG_RELOCATE
  printf("[randomize_layout] track_size: %zu, code_size: %zu\n",
         track_size, code_size);
#endif
  assert(track_size == code_size);
  return fun_entry;
}

void
DebugVarys(uint32_t r14)
{
  printf("[Varys] r14: %u\n", r14);
}

// Varys Support.

// Varys check includes two part:
// 1. AEX detection
// 2. Check # of instructions executed since last AEX.
void
construct_ssa_polling(uint64_t ssa_addr,
                      uint64_t* fun_addr)
{
  void* mapped;
  struct SizedBuffer output = { 0, 0, NULL };
  struct Operand ssa;
  label_t reset = { 0, 0 };

  /*
   varys_check (rdi <- count):
     add r14, rdi
     mov r15, ssa_addr
     mov rdi, [r15]
     test rdi, rdi
     je end
     cmp r14, threshold
     jle reset
     call abort
   reset:
     xor r14, r14
     mov [r15], r14
   end:
     ret
   */

  emit_movq_ri(&output, GP_R15, ssa_addr);
  build_operand(&ssa, GP_R15, REG_UNKNOWN, SCALE_NONE, 0);
  emit_mov_rm(&output, GP_R14, &ssa, VALTYPE_I32);

#if 0
  // Debug code
  SaveRegisterStates(&output);
  emit_mov_rr(&output, GP_RDI, GP_R14, VALTYPE_I64);
  emit_movq_ri(&output, GP_RAX, (uint64_t)DebugVarys);
  emit_call_r(&output, GP_RAX);
  RestoreRegisterStates(&output);
#endif

  emit_cmp_ri(&output, GP_R14, VARYS_MAGIC, VALTYPE_I32);
  emit_jcc(&output, COND_EQ, &reset, Near);

#if 0
  // Debug code
  SaveRegisterStates(&output);
  //emit_mov_rr(&output, GP_RDI, GP_R15, VALTYPE_I64);
  emit_mov_rr(&output, GP_RSI, GP_R14, VALTYPE_I64);
  emit_movq_ri(&output, GP_RAX, (uint64_t)DebugVarys);
  emit_call_r(&output, GP_RAX);
  RestoreRegisterStates(&output);
#endif
  //emit_movq_ri(&output, GP_RAX, (uint64_t)abort);
  //emit_call_r(&output, GP_RAX);
  bind_label(&output, &reset, pc_offset(&output));
  emit_movl_ri(&output, GP_R14, VARYS_MAGIC);
  emit_mov_mr(&output, &ssa, GP_R14, VALTYPE_I32);
  emit_xor_rr(&output, GP_R14, GP_R14, VALTYPE_I64);

  emit_ret(&output, 0);

  mapped = sgxwasm_allocate_code(output.size, PageSize, 0);
  memcpy(mapped, output.data, output.size);

  *fun_addr = (uint64_t) mapped;
  if (!sgxwasm_mark_code_segment_executable(mapped, output.size)) {
    assert(0);
  }

#if __DEBUG_VARYS__
  size_t i;
  printf("varys_check: \n");
  for (i = 0; i < output.size; i++) {
    uint8_t byte = ((uint8_t*)mapped)[i];
    if (byte < 16) {
      printf("0%x ", byte);
      continue;
    }
    printf("%x ", byte);
  }
  printf("\n");
#endif
}
