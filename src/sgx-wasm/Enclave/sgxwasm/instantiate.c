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

#include <sgxwasm/emscripten.h>
#include <sgxwasm/instantiate.h>

#include <sgxwasm/compile.h>
#include <sgxwasm/parse.h>
#include <sgxwasm/relocate.h>
#include <sgxwasm/runtime.h>
#include <sgxwasm/util.h>

#include <sgxwasm/sys.h>

// Unused functions.
__attribute__((unused)) static int
is_unused_fun(struct Function* func)
{
  // Based on symbol name.
  if (func->name) {
    if (strcmp(func->name, "setTempRet0") == 0) {
      printf("ignore func %zu: %s\n", func->fun_index, func->name);
      return 1;
    }
    if (strcmp(func->name, "getTempRet0") == 0) {
      printf("ignore func %zu: %s\n", func->fun_index, func->name);
      return 1;
    }
  }
}

// Debuging functions.

__attribute__((unused)) static void
dump_compile_code(uint8_t* code, size_t size, size_t fun_index)
{
  size_t i;
#if __linux__
  printf("fun %zu (@0x%lx): ", fun_index, (uint64_t)code);
#else
  printf("fun %zu (@0x%llx): ", fun_index, (uint64_t)code);
#endif
  for (i = 0; i < size; i++) {
    uint8_t byte = code[i];
    if (byte < 16) {
      printf("0%x ", byte);
      continue;
    }
    printf("%x ", byte);
  }
  printf("\n");
}

__attribute__((unused)) static void
dump_code_units(struct CodeUnitTable* table)
{
  size_t i, j, k;
  size_t base = sgxwasm_get_code_base();
  for (i = 0; i < table->size; i++) {
    struct CodeUnits* unit_list = &table->units[i];
    for (j = 0; j < unit_list->size; j++) {
      struct CodeUnit* unit = &unit_list->data[j];
      uint64_t addr = base + unit->offset;
      size_t size = unit->size;
      uint8_t* code = (uint8_t*)addr;
#if __linux__
      printf("fun #%zu, unit #%zu, (@%lx): ", i, j, addr);
#else
      printf("fun #%zu, unit #%zu, (@%llx): ", i, j, addr);
#endif
      for (k = 0; k < size; k++) {
        uint8_t byte = code[k];
        if (byte < 16) {
          printf("0%x ", byte);
          continue;
        }
        printf("%x ", byte);
      }
      printf("\n");
    }
  }
}

__attribute__((unused)) static void
dump_table(struct Module* module)
{
  assert(module != NULL);
  size_t i;
  size_t number_tables = module->tables.size;

  printf("Number of tables: %lu\n", number_tables);
  for (i = 0; i < number_tables; i++) {
    struct Table* table = module->tables.data[i];
    printf("Table #%lu, init_len: %lu, max_len: %lu\n", i, table->length,
           table->max);
    size_t j;
    for (j = 0; j < table->length; j++) {
      struct Function* data = table->data[j];
      size_t fun_index;
      if (data == NULL) {
        continue;
      }
      fun_index = data->fun_index;
#if __linux__
      printf("Entry #%lu - Fun #%lu\n", j, fun_index);
#else
      printf("Entry #%lu - Fun #%lu\n", j, fun_index);
#endif
    }
  }
}

__attribute__((unused)) static void
dump_indirect_table(struct IndirectCallTable* table)
{
  assert(table != NULL);
  size_t i;
  size_t table_size = *table->size;

  printf("Table size: %lu\n", table_size);
  for (i = 0; i < table_size; i++) {
#if __linux__
    printf("Entry #%lu, sig_id: %u, refs: 0x%lx\n", i, table->sigs[i],
           table->refs[i]);
#else
    printf("Entry #%lu, sig_id: %u, refs: 0x%llx\n", i, table->sigs[i],
           table->refs[i]);
#endif
  }
}

__attribute__((unused)) static void
dump_mem(char* mem, size_t size)
{
  assert(mem != NULL);
  size_t i;
  size_t term = 1;

  printf("[dump_mem]\n");
  for (i = 0; i < size; i++) {
    char c = mem[i];
    if (c == 0x00) {
      term = 1;
      continue;
    }
    if (term == 1) {
      printf("\n%zu: ", i);
      term = 0;
    }
    printf("%c", c);
  }
}

__attribute__((unused)) static void
dump_mem_bytes(char* mem, size_t size)
{
  assert(mem != NULL);
  size_t i;
  printf("[dump_mem_bytes]\n");
  for (i = 0; i < size; i += 4) {
    uint32_t* addr = (uint32_t*)((uint64_t)mem + i);
    printf("%zu: %u\n", i, *addr);
  }
}

__attribute__((unused)) static void
dump_funs(struct Module* module, size_t number_funs)
{
  struct Function* func;
  size_t i;

  for (i = 0; i < number_funs; i++) {
    func = module->funcs.data[i];
#if __linux__
    printf("Fun #%zu (@%lx) ", i, (uint64_t)func->code);
#else
    printf("Fun #%zu (@%llu) ", i, (uint64_t)func->code);
#endif
    if (func->name) {
      printf("symbol: %s", func->name);
    }
    printf("\n");
  }
}

// End of debugging functions.

#if SGXWASM_BINARY_SIZE
__attribute__((unused)) static uint64_t
get_size_in_functions(struct Module* module, size_t offset, size_t n_funs)
{
  size_t i;
  uint64_t count = 0;
  for (i = 0; i < n_funs; i++) {
    size_t fun_index = i + offset;
    struct Function* func = module->funcs.data[fun_index];
    printf("fun #%zu: %zu\n", fun_index, func->size);
    count += func->size;
  }
  return count;
}

__attribute__((unused)) static uint64_t
get_size_in_units(struct CodeUnitTable* table)
{
  size_t i, j, k;
  uint64_t count = 0;
  size_t base = sgxwasm_get_code_base();
  for (i = 0; i < table->size; i++) {
    struct CodeUnits* unit_list = &table->units[i];
    size_t func_size = 0;
    for (j = 0; j < unit_list->size; j++) {
      struct CodeUnit* unit = &unit_list->data[j];
      size_t size = unit->size;
      // count += size;
      func_size += size;
    }
    printf("fun #%zu: %zu\n", i, func_size);
    count += func_size;
  }
  return count;
}
#endif

#if __PASS__
static void
passes_validation(struct PassManager* pm, struct CodeUnitTable* table)
{
  assert(pm != NULL && table != NULL);
  size_t i, j, k;
  size_t base = sgxwasm_get_code_base();
  void (*validation)(size_t, size_t, uint64_t, size_t) = NULL;
  for (i = 0; i < pm->size; i++) {
    struct Pass* pass = &pm->data[i];
    if (!pass->validation) {
      continue;
    }
    validation = pass->validation;
  }

  if (validation == NULL) {
    return;
  }

  for (i = 0; i < table->size; i++) {
    struct CodeUnits* unit_list = &table->units[i];
    for (j = 0; j < unit_list->size; j++) {
      struct CodeUnit* unit = &unit_list->data[j];
      uint64_t addr = base + unit->offset;
      size_t size = unit->size;
      validation(i, j, addr, size);
    }
  }
}
#endif

static int
func_sig_repr(char* why, size_t why_size, struct FuncType* type)
{
  int ret;
  size_t k;

  ret = snprintf(why, why_size, "[");
  for (k = 0; k < type->n_inputs; ++k) {
    ret += snprintf(why + ret, why_size - ret, "%s,",
                    sgxwasm_valtype_repr(type->input_types[k]));
  }
  ret += snprintf(why + ret, why_size - ret, "] -> [");
  for (k = 0; k < FUNC_TYPE_N_OUTPUTS(type); ++k) {
    ret += snprintf(why + ret, why_size - ret, "%s,",
                    sgxwasm_valtype_repr(FUNC_TYPE_OUTPUT_IDX(type, k)));
  }
  ret += snprintf(why + ret, why_size - ret, "]");

  return ret;
}

static int
read_constant_expression(struct Module* module, unsigned valtype,
                         struct Value* value, size_t n_instructions,
                         struct Instr* instructions)
{
  // XXX: Currently only allow one operation
  // ({i32, i64, f32, f64}.const and get_global)
  // follows by an end opcode.
  if (n_instructions != 2)
    goto error;

  if (instructions[0].opcode == OPCODE_GET_GLOBAL) {
    struct Global* global =
      module->globals.data[instructions[0].data.get_global.globalidx];

    if (global->mut)
      goto error;

    if (global->value.type != valtype)
      goto error;

    *value = global->value;

    return 1;
  }

  value->type = valtype;
  switch (valtype) {
    case VALTYPE_I32:
      if (instructions[0].opcode != OPCODE_I32_CONST)
        goto error;
      value->data.i32 = instructions[0].data.i32_const.value;
      break;
    case VALTYPE_I64:
      if (instructions[0].opcode != OPCODE_I64_CONST)
        goto error;
      value->data.i64 = instructions[0].data.i64_const.value;
      break;
    case VALTYPE_F32:
      if (instructions[0].opcode != OPCODE_F32_CONST)
        goto error;
      value->data.f32 = instructions[0].data.f32_const.value;
      break;
    case VALTYPE_F64:
      if (instructions[0].opcode != OPCODE_F64_CONST)
        goto error;
      value->data.f64 = instructions[0].data.f64_const.value;
      break;
    default:
      assert(0);
      break;
  }

  return 1;

error:
  return 0;
}

static int
fill_module_types(struct Module* module, struct ModuleTypes* module_types)
{
  size_t i;

  module_types->functypes =
    calloc(module->funcs.size, sizeof(module_types->functypes[0]));
  if (module->funcs.size && !module_types->functypes)
    goto error;

  module_types->tabletypes =
    calloc(module->tables.size, sizeof(module_types->tabletypes[0]));
  if (module->tables.size && !module_types->tabletypes)
    goto error;

  module_types->memorytypes =
    calloc(module->mems.size, sizeof(module_types->memorytypes[0]));
  if (module->mems.size && !module_types->memorytypes)
    goto error;

  module_types->globaltypes =
    calloc(module->globals.size, sizeof(module_types->globaltypes[0]));
  if (module->globals.size && !module_types->globaltypes)
    goto error;

  for (i = 0; i < module->funcs.size; ++i) {
    module_types->functypes[i] = module->funcs.data[i]->type;
  }

  for (i = 0; i < module->tables.size; ++i) {
    module_types->tabletypes[i].elemtype = module->tables.data[i]->elemtype;
    module_types->tabletypes[i].limits.min = module->tables.data[i]->length;
    module_types->tabletypes[i].limits.max = module->tables.data[i]->max;
  }

  for (i = 0; i < module->mems.size; ++i) {
    module_types->memorytypes[i].limits.min =
      module->mems.data[i]->size / WASM_PAGE_SIZE;
    module_types->memorytypes[i].limits.max =
      module->mems.data[i]->max / WASM_PAGE_SIZE;
  }

  for (i = 0; i < module->globals.size; ++i) {
    module_types->globaltypes[i].valtype = module->globals.data[i]->value.type;
    module_types->globaltypes[i].mut = module->globals.data[i]->mut;
  }

  return 1;

error:
  return 0;
}

struct Module*
sgxwasm_instantiate(const struct WASMModule* wasm_module,
                    struct PassManager* pm, struct SystemConfig* config,
                    size_t n_imports, const struct NamedModule* imports,
                    char* why, size_t why_size)
{
  uint32_t i;
  struct Module* module = NULL;
  struct ModuleTypes module_types;
  struct Function* tmp_func = NULL;
  struct Table* tmp_table = NULL;
  struct Memory* tmp_mem = NULL;
  struct Global* tmp_global = NULL;
  void *unmapped = NULL, *mapped = NULL;
  struct MemoryReferences memrefs = { 0, 0, NULL };
  size_t code_size;
  unsigned global_compile_flags;
  struct RelocationTable relo_table = { 0, NULL };
  size_t number_funs = 0;
  struct IndirectCallTable indirect_table = { 0, NULL, NULL };
  struct CodeUnitTable code_table = { 0, NULL };
  // T-SGX support
  struct Springboard springboard;
  // Vary support
  uint64_t ssa_polling_addr = 0;
  // Relocation at code-unit level.
  int use_code_unit = 0;
  int enable_aslr = 0;

  sgxwasm_init_code_region(CodeSize);

  global_compile_flags = 0;

  if (pass_is_enabled(pm, "aslr") || pass_is_enabled(pm, "caslr")) {
    enable_aslr = 1;
  }

  if (pass_is_enabled(pm, "aslr") || pass_is_enabled(pm, "caslr") ||
      pass_is_enabled(pm, "tsgx") || pass_is_enabled(pm, "varys") ||
      pass_is_enabled(pm, "lspectre")) {
    use_code_unit = 1;
  }

  memset(&module_types, 0, sizeof(module_types));
  module = calloc(1, sizeof(*module));
  if (!module)
    goto error;

#define LVECTOR_GROW(sstack)                                                   \
  do {                                                                         \
    if (!VECTOR_GROW(sstack))                                                  \
      goto error;                                                              \
  } while (0)

  for (i = 0; i < wasm_module->type_section.n_types; ++i) {
    LVECTOR_GROW(&module->types);

    module->types.data[module->types.size - 1] =
      wasm_module->type_section.types[i];
  }

#if DEBUG_INSTANTIATE
  printf("[sgxwasm_instantiate] import section\n");
#endif

  /* load imports */
  for (i = 0; i < wasm_module->import_section.n_imports; ++i) {
    size_t j;
    struct ImportSectionImport* import =
      &wasm_module->import_section.imports[i];
    struct Module* import_module = NULL;

    /* look for import module */
    for (j = 0; j < n_imports; ++j) {
      if (strcmp(imports[j].name, import->module))
        continue;
      import_module = imports[j].module;
    }

    if (!import_module) {
#if DEBUG_INSTANTIATE
      printf("Couldn't find module: %s\n", import->module);
#endif
      goto error;
    }

    for (j = 0; j < import_module->exports.size; ++j) {
      struct Export* export = &import_module->exports.data[j];

      if (strcmp(export->name, import->name))
        continue;

      if (export->type != import->desc_type) {
/* bad import type */
#if DEBUG_INSTANTIATE
        printf("bad import type\n");
#endif
        goto error;
      }

      switch (export->type) {
        case IMPORT_DESC_TYPE_FUNC: {
          struct Function* func = export->value.func;
/*struct TypeSectionType* type =
  &module->type_section.types[import->desc.functypeidx];
if (!sgxwasm_typecheck_func(type, func)) {
  int ret;
  ret = snprintf(why,
                 why_size,
                 "Mismatched types for %s.%s: ",
                 import->module,
                 import->name);
  ret += func_sig_repr(why + ret, why_size - ret, &func->type);
  ret += snprintf(why + ret, why_size - ret, " vs ");
  ret += func_sig_repr(why + ret, why_size - ret, type);
  goto error;
}*/

#if DEBUG_INSTANTIATE
          printf("import: %s\n", import->name);
#endif
          // Copy symbols.
          size_t name_length = strlen(import->name);
          if (name_length > 0) {
            func->name = malloc(name_length + 1);
            assert(func->name);
            memcpy(func->name, import->name, name_length + 1);
          }

          // Allow referencing module using func.
          func->module = module;

          /* add func to func table */
          size_t type_index =
            dedup_type(&module->types, import->desc.functypeidx);
          LVECTOR_GROW(&module->funcs);
          module->funcs.data[module->funcs.size - 1] = func;
          // module->funcs.data[module->funcs.size - 1]->type_index =
          //  import->desc.functypeidx;
          module->funcs.data[module->funcs.size - 1]->type_index = type_index;
          module->n_imported_funcs++;

          break;
        }
        // Workaround: To get rid the dependency of reading .js file
        // generated by emscripten, we declare the table based on the
        // information in the import section (because the initialization
        // of the table is based on the element section anyway).
        case IMPORT_DESC_TYPE_TABLE: {
          size_t table_min = import->desc.tabletype.limits.min;
          size_t table_max = import->desc.tabletype.limits.max;
          unsigned elemtype = import->desc.tabletype.elemtype;
          assert(tmp_table == NULL);
          tmp_table = calloc(1, sizeof(struct Table));
          if (!tmp_table)
            goto error;
          tmp_table->elemtype = elemtype;
          tmp_table->length = table_min;
          tmp_table->max = table_max;
          tmp_table->data = NULL;
          if (tmp_table->length) {
            tmp_table->data =
              calloc(tmp_table->length, sizeof(tmp_table->data[0]));
            if (!tmp_table->data)
              goto error;
          }
          /* add Table to table table */
          LVECTOR_GROW(&module->tables);
          module->tables.data[module->tables.size - 1] = tmp_table;
          module->n_imported_tables++;
          tmp_table = NULL;
          break;
        }
        case IMPORT_DESC_TYPE_MEM: {
          struct Memory* memory = export->value.mem;

          if (!sgxwasm_typecheck_memory(&import->desc.memtype, memory)) {
#if DEBUG_INSTANTIATE
            printf("Mismatched memory size for import "
                   "%s.%s: {%zu,%zu} vs {%" PRIu32 ",%" PRIu32 "}",
                   import->module, import->name, memory->size / WASM_PAGE_SIZE,
                   memory->max / WASM_PAGE_SIZE,
                   import->desc.memtype.limits.min,
                   import->desc.memtype.limits.max);
#endif
            goto error;
          }

          /* add Memory to mems table */
          LVECTOR_GROW(&module->mems);
          module->mems.data[module->mems.size - 1] = memory;
          module->n_imported_mems++;

          break;
        }
        case IMPORT_DESC_TYPE_GLOBAL: {
          struct Global* Global = export->value.global;

          if (!sgxwasm_typecheck_global(&import->desc.globaltype, Global)) {
#if DEBUG_INSTANTIATE
            printf(why, why_size, "Mismatched global for import "
                                  "%s.%s: %s%s vs %s%s",
                   import->module, import->name,
                   sgxwasm_valtype_repr(Global->value.type),
                   Global->mut ? " mut" : "",
                   sgxwasm_valtype_repr(import->desc.globaltype.valtype),
                   import->desc.globaltype.mut ? " mut" : "");
#endif
            goto error;
          }

          /* add Global to globals tables */
          LVECTOR_GROW(&module->globals);
          module->globals.data[module->globals.size - 1] = Global;
          module->n_imported_globals++;

          // Hack: make a universal entry point.
          size_t global_index = module->globals.size - 1;
          struct Value* value = &module->globals.data[global_index]->value;
          value->val = 0;
#if DEBUG_INSTANTIATE
          printf("global #%lu ", global_index);
#endif
          if (value->type == VALTYPE_I32) {
            value->val = value->data.i32;
#if DEBUG_INSTANTIATE
            printf("type: I32, value: %u\n", value->data.i32);
#endif
          } else if (value->type == VALTYPE_I64) {
            value->val = value->data.i64;
#if DEBUG_INSTANTIATE
#if __linux__
            printf("type: I64, value: %lu\n", value->data.i64);
#else
            printf("type: I64, value: %llu\n", value->data.i64);
#endif
#endif
          } else if (value->type == VALTYPE_F32) {
            value->val = f32_encoding(value->data.f32);
#if DEBUG_INSTANTIATE
            printf("type: F32, value: %f\n", value->data.f32);
#endif
          } else if (value->type == VALTYPE_F64) {
            value->val = f64_encoding(value->data.f64);
#if DEBUG_INSTANTIATE
            printf("type: F64, value: %lf\n", value->data.f64);
#endif
          }
#if DEBUG_INSTANTIATE
          else {
            printf("unknown type, unknown value\n");
          }
#endif
          break;
        }
        default:
          assert(0);
          break;
      }

      break;
    }

    if (j == import_module->exports.size) {
      /* couldn't find import */
      if (why)
        switch (import->desc_type) {
          case IMPORT_DESC_TYPE_FUNC: {
            int ret;
            struct TypeSectionType* type =
              &wasm_module->type_section.types[import->desc.functypeidx];
            ret = snprintf(why, why_size, "couldn't find func import: %s.%s ",
                           import->module, import->name);

            func_sig_repr(why + ret, why_size - ret, type);
            break;
          }
          case IMPORT_DESC_TYPE_TABLE:
            snprintf(why, why_size, "couldn't find table import: %s.%s",
                     import->module, import->name);
            break;
          case IMPORT_DESC_TYPE_MEM:
            snprintf(why, why_size, "couldn't find memory import: %s.%s "
                                    "{%" PRIu32 ", %" PRIu32 "}",
                     import->module, import->name,
                     import->desc.memtype.limits.min,
                     import->desc.memtype.limits.max);
            break;
          case IMPORT_DESC_TYPE_GLOBAL:
            snprintf(why, why_size, "couldn't find global import: %s.%s %s%s",
                     import->module, import->name,
                     sgxwasm_valtype_repr(import->desc.globaltype.valtype),
                     import->desc.globaltype.mut ? " mut" : "");
            break;
          default:
            assert(0);
            break;
        }
      goto error;
    }
  }

#if DEBUG_INSTANTIATE
  printf("[sgxwasm_instantiate] function section\n");
#endif

  for (i = 0; i < wasm_module->function_section.n_typeidxs; ++i) {
    assert(tmp_func == NULL);
    tmp_func = calloc(1, sizeof(*tmp_func));
    if (!tmp_func)
      goto error;

    // size_t type_index = wasm_module->function_section.typeidxs[i];
    size_t type_index =
      dedup_type(&module->types, wasm_module->function_section.typeidxs[i]);
    tmp_func->module = module;
    tmp_func->code = NULL;
    tmp_func->size = 0;
    // tmp_func->type = wasm_module->type_section.types[type_index];
    tmp_func->type = module->types.data[type_index];
    tmp_func->name = NULL;

    LVECTOR_GROW(&module->funcs);
    module->funcs.data[module->funcs.size - 1] = tmp_func;
    module->funcs.data[module->funcs.size - 1]->type_index = type_index;
    tmp_func = NULL;
  }

  for (i = 0; i < wasm_module->table_section.n_tables; ++i) {
    struct TableSectionTable* table = &wasm_module->table_section.tables[i];

    assert(!table->limits.max || table->limits.min <= table->limits.max);

    assert(tmp_table == NULL);
    tmp_table = calloc(1, sizeof(*tmp_table));
    if (!tmp_table)
      goto error;

    tmp_table->elemtype = table->elemtype;
    tmp_table->length = table->limits.min;
    tmp_table->max = table->limits.max;

    if (tmp_table->length) {
      tmp_table->data = calloc(tmp_table->length, sizeof(tmp_table->data[0]));
      if (!tmp_table->data)
        goto error;
    }

    LVECTOR_GROW(&module->tables);
    module->tables.data[module->tables.size - 1] = tmp_table;
    tmp_table = NULL;
  }

  for (i = 0; i < wasm_module->memory_section.n_memories; ++i) {
    struct MemorySectionMemory* memory =
      &wasm_module->memory_section.memories[i];
    size_t size, max;

    assert(!memory->memtype.limits.max ||
           memory->memtype.limits.min <= memory->memtype.limits.max);

    /*printf("Memory min: %u, max: %u\n",
           memory->memtype.limits.min,
           memory->memtype.limits.max);*/

    if (__builtin_umull_overflow(memory->memtype.limits.min, WASM_PAGE_SIZE,
                                 &size)) {
      goto error;
    }

    if (__builtin_umull_overflow(memory->memtype.limits.max, WASM_PAGE_SIZE,
                                 &max)) {
      goto error;
    }

    assert(tmp_mem == NULL);
    tmp_mem = calloc(1, sizeof(*tmp_mem));
    if (!tmp_mem)
      goto error;

    if (size) {
      tmp_mem->data = calloc(size, 1);
      if (!tmp_mem->data) {
        free(tmp_mem);
        goto error;
      }
    }

    tmp_mem->size = size;
    tmp_mem->max = max;

    // printf(
    //  "Allocate memory: size: 0x%lx, index: %zu\n", size, module->mems.size);
    LVECTOR_GROW(&module->mems);
    module->mems.data[module->mems.size - 1] = tmp_mem;
    tmp_mem = NULL;
  }

#if DEBUG_INSTANTIATE
  printf("[sgxwasm_instantiate] global section\n");
#endif

  for (i = 0; i < wasm_module->global_section.n_globals; ++i) {
    struct GlobalSectionGlobal* global =
      &wasm_module->global_section.globals[i];
    struct Value value;
    int rrr;

    rrr =
      read_constant_expression(module, global->type.valtype, &value,
                               global->n_instructions, global->instructions);
    if (!rrr)
      goto error;

    assert(tmp_global == NULL);
    tmp_global = calloc(1, sizeof(*tmp_global));
    if (!tmp_global)
      goto error;

    tmp_global->value = value;
    tmp_global->mut = global->type.mut;

    LVECTOR_GROW(&module->globals);
    module->globals.data[module->globals.size - 1] = tmp_global;
    tmp_global = NULL;

    // Hack: make a universal entry point.
    size_t global_index = module->globals.size - 1;
    struct Value* gv = &module->globals.data[global_index]->value;
    gv->val = 0;
#if DEBUG_INSTANTIATE
    printf("global #%lu ", global_index);
#endif
    if (gv->type == VALTYPE_I32) {
      gv->val = gv->data.i32;
#if DEBUG_INSTANTIATE
      printf("type: I32, value: %u\n", gv->data.i32);
#endif
    } else if (gv->type == VALTYPE_I64) {
      gv->val = gv->data.i64;
#if DEBUG_INSTANTIATE
#if __linux__
      printf("type: I64, value: %lu\n", gv->data.i64);
#else
      printf("type: I64, value: %llu\n", gv->data.i64);
#endif
#endif
    } else if (gv->type == VALTYPE_F32) {
      gv->val = f32_encoding(gv->data.f32);
#if DEBUG_INSTANTIATE
      printf("type: F32, value: %f\n", gv->data.f32);
#endif
    } else if (gv->type == VALTYPE_F64) {
      gv->val = f64_encoding(gv->data.f64);
#if DEBUG_INSTANTIATE
      printf("type: F64, value: %lf\n", gv->data.f64);
#endif
    }
#if DEBUG_INSTANTIATE
    else {
      printf("unknown type, unknown value\n");
    }
#endif
  }

#if DEBUG_INSTANTIATE
  printf("[sgxwasm_instantiate] export section\n");
#endif

  for (i = 0; i < wasm_module->export_section.n_exports; ++i) {
    struct Export* exportinst;
    struct ExportSectionExport* export =
      &wasm_module->export_section.exports[i];

    LVECTOR_GROW(&module->exports);
    exportinst = &module->exports.data[module->exports.size - 1];

    exportinst->name = strdup(export->name);
    if (!exportinst->name)
      goto error;

    exportinst->type = export->idx_type;

    switch (export->idx_type) {
      case IMPORT_DESC_TYPE_FUNC: {
        exportinst->value.func = module->funcs.data[export->idx];
        // Copy symbols.
        size_t name_length = strlen(exportinst->name);
        if (name_length > 0) {
          exportinst->value.func->name = malloc(name_length + 1);
          assert(exportinst->value.func->name);
          memcpy(exportinst->value.func->name, exportinst->name,
                 name_length + 1);
        }
        break;
      }
      case IMPORT_DESC_TYPE_TABLE:
        exportinst->value.table = module->tables.data[export->idx];
        break;
      case IMPORT_DESC_TYPE_MEM:
        exportinst->value.mem = module->mems.data[export->idx];
        break;
      case IMPORT_DESC_TYPE_GLOBAL:
        exportinst->value.global = module->globals.data[export->idx];
        break;
      default:
        assert(0);
        break;
    }
  }

#if DEBUG_INSTANTIATE
  printf("[sgxwasm_instantiate] element section\n");
#endif

  for (i = 0; i < wasm_module->element_section.n_elements; ++i) {
    struct ElementSectionElement* element =
      &wasm_module->element_section.elements[i];
    struct Table* table;
    int rrr;
    struct Value value;
    size_t j;

    rrr =
      read_constant_expression(module, VALTYPE_I32, &value,
                               element->n_instructions, element->instructions);
    if (!rrr)
      goto error;

    assert(element->tableidx < module->tables.size);
    table = module->tables.data[element->tableidx];

    if (value.data.i32 + element->n_funcidxs > table->length)
      goto error;

    for (j = 0; j < element->n_funcidxs; ++j) {
      assert(element->funcidxs[j] < module->funcs.size);
      // printf("ele #%zu: value.data.i32: %zu, j: %zu, funcidxs: %zu\n", i,
      //       value.data.i32, j, element->funcidxs[j]);
      table->data[value.data.i32 + j] =
        module->funcs.data[element->funcidxs[j]];
    }
  }

  if (!fill_module_types(module, &module_types))
    goto error;

  // Initialize relocation table.
  number_funs = wasm_module->code_section.n_codes + module->n_imported_funcs;
  if (!init_relo_table(&relo_table, number_funs))
    assert(0);
  if (!init_code_unit_table(&code_table, number_funs))
    assert(0);
  for (i = 0; i < module->n_imported_funcs; i++) {
    struct Function* import_fun = module->funcs.data[i];
    uint64_t addr = (uintptr_t)import_fun->code;
    set_code_entry_offset(&code_table, i, addr);
    // Also set the function index.
    import_fun->fun_index = i;
  }

  // T-SGX support.
  if (pass_is_enabled(pm, "tsgx")) {
    construct_springboard(&springboard);
  }

  if (pass_is_enabled(pm, "varys")) {
    assert(config->exittype_addr != 0);
    construct_ssa_polling(config->exittype_addr, &ssa_polling_addr);
#if __DEBUG_VARYS__
#if __linux__
    printf("ssa_polling_addr: %lx\n", ssa_polling_addr);
#else
    printf("ssa_polling_addr: %llx\n", ssa_polling_addr);

#endif
#endif
  }

#if DEBUG_INSTANTIATE
  printf("[sgxwasm_instantiate] code section\n");
#endif

  for (i = 0; i < wasm_module->code_section.n_codes; ++i) {
    struct CodeSectionCode* code = &wasm_module->code_section.codes[i];
    struct Memory* memory;
    struct Function* func;
    size_t j;
    size_t fun_index = i + module->n_imported_funcs;

    func = module->funcs.data[fun_index];
    func->fun_index = fun_index;

    // Allow referencing module using Function.
    func->module = module;

    if (memrefs.data) {
      free(memrefs.data);
      memrefs.capacity = 0;
      memrefs.size = 0;
      memrefs.data = NULL;
    }

    // assert(module->mems.size > 0);
    // XXX: Current spec supports only one memory.
    if (module->mems.size > 0) {
      memory = module->mems.data[0];
    }

    if (unmapped)
      free(unmapped);

    assert(mapped == NULL);
    unmapped = sgxwasm_compile_function(
      pm, &module->types, &module_types, func, memory, number_funs, code,
      &memrefs, &code_size, &func->stack_usage, global_compile_flags);
    if (!unmapped)
      goto error;

    // Collect relocation information.
    for (j = 0; j < memrefs.size; ++j) {
      switch (memrefs.data[j].type) {
        case MEMREF_FUNC: {
          add_relo_entry(&relo_table, func->fun_index, RELO_CALL,
                         &memrefs.data[j]);
          break;
        }
        case MEMREF_MEM: {
          add_relo_entry(&relo_table, func->fun_index, RELO_MEM,
                         &memrefs.data[j]);
          break;
        }
        case MEMREF_GLOBAL: {
          add_relo_entry(&relo_table, func->fun_index, RELO_GLOBAL,
                         &memrefs.data[j]);
          break;
        }
        case MEMREF_TABLE: {
          add_relo_entry(&relo_table, func->fun_index, RELO_TABLE_REFS,
                         &memrefs.data[j]);
          break;
        }
        case MEMREF_TABLE_SIGS: {
          add_relo_entry(&relo_table, func->fun_index, RELO_TABLE_SIGS,
                         &memrefs.data[j]);
          break;
        }
        case MEMREF_TABLE_SIZE: {
          add_relo_entry(&relo_table, func->fun_index, RELO_TABLE_SIZE,
                         &memrefs.data[j]);
          break;
        }
        case MEMREF_SPRINGBOARD_BEGIN: {
          add_relo_entry(&relo_table, func->fun_index, RELO_SPRINGBOARD_BEGIN,
                         &memrefs.data[j]);
          break;
        }
        case MEMREF_SPRINGBOARD_NEXT: {
          add_relo_entry(&relo_table, func->fun_index, RELO_SPRINGBOARD_NEXT,
                         &memrefs.data[j]);
          break;
        }
        case MEMREF_SPRINGBOARD_END: {
          add_relo_entry(&relo_table, func->fun_index, RELO_SPRINGBOARD_END,
                         &memrefs.data[j]);
          break;
        }
        case MEMREF_SSA_POLLING: {
          add_relo_entry(&relo_table, func->fun_index, RELO_SSA_POLLING,
                         &memrefs.data[j]);
          break;
        }
        case MEMREF_JMP_NEXT: {
          add_relo_entry(&relo_table, func->fun_index, RELO_JMP_NEXT,
                         &memrefs.data[j]);
          break;
        }
        case MEMREF_LEA_NEXT: {
          add_relo_entry(&relo_table, func->fun_index, RELO_LEA_NEXT,
                         &memrefs.data[j]);
          break;
        }
        case MEMREF_BR_TABLE_JMP: {
          add_relo_entry(&relo_table, func->fun_index, RELO_BR_TABLE_JMP,
                         &memrefs.data[j]);
          break;
        }
        case MEMREF_BR_CASE_JMP: {
          add_relo_entry(&relo_table, func->fun_index, RELO_BR_CASE_JMP,
                         &memrefs.data[j]);
          break;
        }
        case MEMREF_BR_TABLE_TARGET: {
          add_relo_entry(&relo_table, func->fun_index, RELO_BR_TABLE_TARGET,
                         &memrefs.data[j]);
          break;
        }
        case MEMREF_BR_CASE_TARGET: {
          add_relo_entry(&relo_table, func->fun_index, RELO_BR_CASE_TARGET,
                         &memrefs.data[j]);
          break;
        }
        case MEMREF_CODE_UNIT: {
          add_code_unit(&code_table, func->fun_index, &memrefs.data[j]);
          break;
        }
        case MEMREF_TRAP:
          break;
        default:
          break;
      }
    }

    // enable_aslr = 0;

    if (use_code_unit) {
      convert_to_unit_relo_info(fun_index, &relo_table, &code_table);
      mapped =
        randomize_layout(func, &code_table, unmapped, code_size, enable_aslr);
    } else {
      mapped = sgxwasm_allocate_code(code_size, UnitSize, 0);
      if (!mapped)
        goto error;
      memcpy(mapped, unmapped, code_size);
    }
    func->code = mapped;
    func->size = code_size;
    set_code_entry_offset(&code_table, func->fun_index, (uint64_t)func->code);
    mapped = NULL;

//#if DEBUG_INSTANTIATE
    if (func->fun_index == 17) {
    dump_compile_code(func->code, func->size, func->fun_index);
    }
//#endif
  }

#if DEBUG_INSTANTIATE // Debug
  dump_table(module);
#endif
  // Construct the table for indirect calls.
  if (module->tables.size > 0) {
    // Current spec only allow to have one table.
    assert(module->tables.size == 1);
    struct Table* table = module->tables.data[0];
    size_t table_size = table->length;
    // Hold the reference so the table size can be retrieve
    // during the runtime.
    indirect_table.size = &table->length;
    indirect_table.sigs = malloc(sizeof(uint32_t) * table_size);
    indirect_table.refs = malloc(sizeof(uint64_t) * table_size);
    for (i = 0; i < table_size; i++) {
      struct Function* target = table->data[i];
      if (target == NULL) {
        indirect_table.sigs[i] = 0;
        indirect_table.refs[i] = 0;
        continue;
      }
      // size_t type_index = dedup_type(&wasm_module->type_section,
      // target->type_index);
      // indirect_table.sigs[i] = type_index;
      indirect_table.sigs[i] = target->type_index;
      indirect_table.refs[i] = (uint64_t)target->code;
    }
#if DEBUG_INSTANTIATE // Debug
    dump_indirect_table(&indirect_table);
#endif
  }

  // Relocation
  relocate(module, &relo_table, &indirect_table, &code_table, &springboard,
           ssa_polling_addr, use_code_unit);

#if __PASS__
  passes_validation(pm, &code_table);
#endif
#if DEBUG_RELOCATE
  dump_code_units(&code_table);
#endif

#if 0
  // Mark the jited code as RX only.
  for (i = 0; i < wasm_module->code_section.n_codes; ++i) {
    struct Function* func;
    size_t fun_index;

    fun_index = i + module->n_imported_funcs;
    func = module->funcs.data[fun_index];

    //dump_compile_code(func->code, func->size, func->fun_index);

    if (!sgxwasm_mark_code_segment_executable(func->code, func->size)) {
      goto error;
    }
  }
#endif
#if 0
  for (i = 0; i < wasm_module->code_section.n_codes; ++i) {
    size_t fun_index = i + module->n_imported_funcs;
    struct Function* func = module->funcs.data[fun_index];
    dump_compile_code(func->code, func->size, func->fun_index);
  }
#endif
#if !__SGX__
  // Chage the permissions of code region to RX only.
  // NOTE: For SGX, this can only be supported with SGX2.
  if (!sgxwasm_commit_code_region()) {
    printf("[sgxwasm_commit_code_region] failed\n");
    goto error;
  }
#endif

#if DEBUG_INSTANTIATE
  printf("[sgxwasm_instantiate] data section\n");
#endif

  // Initialize data.
  for (i = 0; i < wasm_module->data_section.n_datas; ++i) {
    struct DataSectionData* data = &wasm_module->data_section.datas[i];
    struct Memory* memory = module->mems.data[data->memidx];
    struct Value value;
    int rrr;

    rrr = read_constant_expression(module, VALTYPE_I32, &value,
                                   data->n_instructions, data->instructions);
    if (!rrr)
      goto error;

    if (data->buf_size > memory->size)
      goto error;

    if (value.data.i32 > memory->size - data->buf_size)
      goto error;

    memcpy(memory->data + value.data.i32, data->buf, data->buf_size);

#if DEBUG_INSTANTIATE // Debug
    dump_mem(memory->data, value.data.i32 + data->buf_size);
    dump_mem_bytes(memory->data, value.data.i32 + data->buf_size);
#endif
  }

#if SGXWASM_BINARY_SIZE
  uint64_t binary_size;
  if (use_code_unit) {
    binary_size = get_size_in_units(&code_table);
  } else {
    binary_size = get_size_in_functions(module, module->n_imported_funcs,
                                        wasm_module->code_section.n_codes);
  }
  // printf("binary size: %lu\n", binary_size);
  printf("%lu\n", binary_size);
#endif

  /* add start function */
  if (wasm_module->start_section.has_start) {
    size_t index = wasm_module->start_section.funcidx;
    struct Function* start = module->funcs.data[index];
    assert(parameter_count(&start->type) == 0 &&
           return_count(&start->type) == 0);
    void((*_start)()) = start->code;
    _start();
  }

#if DEBUG_INSTANTIATE
  dump_funs(module, number_funs);
#endif

  passes_end(pm);

#if SGXWASM_LOADTIME_MEMORY
  uint64_t counter = sgxwasm_get_alloc_counter();
  printf("load-time memory: %lu\n", counter);
#endif

  if (0) {
  error:
    if (module)
      sgxwasm_free_module(module);
    module = NULL;
  }

  if (tmp_func)
    free(tmp_func);
  if (tmp_table) {
    if (tmp_table->data)
      free(tmp_table->data);
    free(tmp_table);
  }
  if (tmp_mem) {
    if (tmp_mem->data)
      free(tmp_mem->data);
    free(tmp_mem);
  }
  if (tmp_global)
    free(tmp_global);
  if (unmapped)
    free(unmapped);
  if (memrefs.data)
    free(memrefs.data);
  if (module_types.functypes)
    free(module_types.functypes);
  if (module_types.tabletypes)
    free(module_types.tabletypes);
  if (module_types.memorytypes)
    free(module_types.memorytypes);
  if (module_types.globaltypes)
    free(module_types.globaltypes);

  // We might need to keep these information if we want to support
  // runtime ASLR in the future.
  free_relo_table(&relo_table, number_funs);

  // XXX: Cannot free them here as they are needed during runtime.
  /*if (indirect_table.sigs)
    free(indirect_table.sigs);
  if (indirect_table.refs)
    free(indirect_table.refs);*/

  return module;
}
