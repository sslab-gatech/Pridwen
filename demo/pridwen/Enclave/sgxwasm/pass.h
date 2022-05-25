#ifndef __SGXWASM__PASS_H__
#define __SGXWASM__PASS_H__

#include <sgxwasm/compile.h>

struct PassManager
{
  size_t capacity;
  size_t size;
  struct Pass
  {
    const char* name;
    int active;
    struct DepList
    {
      size_t capacity;
      size_t size;
      char** data;
    } * dep_list;
    void((*pass_initialize)(struct PassManager*));
    void((*pass_end)(struct PassManager*));
    void((*function_start)(struct CompilerContext*, const struct Function*));
    void((*function_end)(struct CompilerContext*, const struct Function*));
    void((*control_start)(struct CompilerContext*,
                          const struct Function*,
                          control_type_t,
                          size_t));
    void((*control_end)(struct CompilerContext*,
                        const struct Function*,
                        control_type_t,
                        size_t));
    void((*instruction_start)(struct CompilerContext*,
                              const struct Function*,
                              const struct Instr*));
    void((*instruction_end)(struct CompilerContext*,
                            const struct Function*,
                            const struct Instr*));
    void((*machine_inst_start)(struct CompilerContext*,
                               const struct Function*,
                               const struct MachineInstr*));
    void((*machine_inst_end)(struct CompilerContext*,
                             const struct Function*,
                             const struct MachineInstr*));
    void((*validation))(size_t fun_id, size_t unit_id, uint64_t addr,
                        size_t size);
  } * data;
};

void
pass_manager_init(struct PassManager*);

void
passes_init(struct PassManager*);

void
passes_end(struct PassManager*);

struct Pass*
get_new_pass(struct PassManager*);

void
pass_init(struct Pass*,
          const char*,
          void*,
          void*,
          void*,
          void*,
          void*,
          void*,
          void*,
          void*,
          void*,
          void*,
          void*);

void
pass_add_dep(struct Pass*, char*);

void
pass_disable(struct PassManager*, char*);

int
pass_is_enabled(struct PassManager*, char*);

#define PASS_LIST(V)                                                           \
  V(pm, test)                                                                  \
  V(pm, cfg)                                                                   \
  V(pm, lspectre)                                                              \
  V(pm, caslr)                                                                 \
  V(pm, varys)                                                                 \
  V(pm, tsgx)                                                                  \
  V(pm, aslr)

#define DECLAR_ADD_PASS(_pm, _name) void add_pass_##_name(struct PassManager*);

#define ADD_PASS(_pm, _name) add_pass_##_name(_pm);

void
dump_passes(struct PassManager*);

void
dump_active_passes(struct PassManager*);

void
pass_log(const char*, char*, ...);

#define plog(fmt, ...) pass_log(pass_name, fmt, ##__VA_ARGS__)

#endif
