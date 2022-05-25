#ifndef __SGXWASM_RELOCATE_H__
#define __SGXWASM_RELOCATE_H__

#include <sgxwasm/compile.h>
#include <sgxwasm/config.h>

// Table for indirect calls

struct IndirectCallTable
{
  size_t* size;
  uint32_t* sigs;
  uint64_t* refs;
};

// End of table for indirect calls

// Implementation of relocation.

enum RelocationType
{
  RELO_CALL = 0x10,
  RELO_MEM = 0x11,
  RELO_GLOBAL = 0x12,
  RELO_TABLE_REFS = 0x13,
  RELO_TABLE_SIZE = 0x14,
  RELO_TABLE_SIGS = 0x15,
  RELO_SPRINGBOARD_BEGIN = 0x16,
  RELO_SPRINGBOARD_NEXT = 0x17,
  RELO_SPRINGBOARD_END = 0x18,
  RELO_JMP_NEXT = 0x19,
  RELO_LEA_NEXT = 0x1a,
  RELO_CODE_UNIT = 0x1b,
  RELO_BR_TABLE_JMP = 0x1c,
  RELO_BR_CASE_JMP = 0x1d,
  RELO_BR_TABLE_TARGET = 0x1e,
  RELO_BR_CASE_TARGET = 0x1f,
  RELO_SSA_POLLING = 0x20,
};
typedef enum RelocationType relocation_type_t;

struct RelocationTable
{
  size_t size;
  struct RelocationEntries
  {
    size_t capacity;
    size_t size;
    struct Entry
    {
      relocation_type_t type;
      int64_t offset;
      size_t target_index;
      size_t unit_index;
      size_t unit_size;
    } * data;
  } * entries;
};
DECLARE_VECTOR_GROW(relo_entry, struct RelocationEntries);

struct CodeUnitTable
{
  size_t size;
  struct CodeUnits
  {
    int64_t entry_offset; // Fast path.
    size_t capacity;
    size_t size;
    struct CodeUnit
    {
      int64_t offset;
      size_t size;
    } * data;
  } * units;
};
DECLARE_VECTOR_GROW(code_unit, struct CodeUnits);

struct Springboard
{
  uint64_t begin;
  uint64_t next;
  uint64_t end;
};

int
init_relo_table(struct RelocationTable*, size_t);
void
free_relo_table(struct RelocationTable*, size_t);
int
init_code_unit_table(struct CodeUnitTable*, size_t);
void
free_code_unit_table(struct CodeUnitTable*, size_t);

void
add_code_unit(struct CodeUnitTable*, size_t, struct MemoryRef*);
void
convert_to_unit_relo_info(size_t,
                          struct RelocationTable*,
                          struct CodeUnitTable*);
void*
randomize_layout(struct Function*, struct CodeUnitTable*, void*, size_t, int);

void
construct_springboard(struct Springboard*);

void
construct_ssa_polling(uint64_t, uint64_t*);

void
set_code_entry_offset(struct CodeUnitTable*, size_t, uint64_t);
void
add_relo_entry(struct RelocationTable*,
               size_t,
               relocation_type_t,
               struct MemoryRef*);

void
relocate(struct Module*,
         struct RelocationTable*,
         struct IndirectCallTable*,
         struct CodeUnitTable*,
         struct Springboard*,
         uint64_t,
         int);

#endif
