#include <sgxwasm/pass.h>

#if __TEST__
static const char* pass_name = "test";

#if MEMORY_TRACE
size_t prev_count = 0;
#endif

static int function_start_flag = 0;

void
onFunctionStart(struct CompilerContext* ctx, const struct Function* func)
{
  // struct FuncType* sig = &ctx->sig;
  // size_t fun_index = func->fun_index;
  // size_t num_params = parameter_count(sig);

  (void)ctx;
  (void)func;

  // plog("[onFunctionStart] %zu\n", fun_index);
#if MEMORY_TRACE
  prev_count = 0;
#endif
  /*CALL_HOOK_FUNC(2,
                 "[onFunctionStart] function id: %zu, number of inputs: %zu\n",
                 fun_index,
                 num_params);*/
  // Reset the flag.
  function_start_flag = 0;
}

void
onFunctionEnd(struct CompilerContext* ctx, const struct Function* func)
{
  (void)ctx;
  (void)func;
}

void
onControlStart(struct CompilerContext* ctx,
               const struct Function* func,
               control_type_t control_type,
               size_t depth)
{
  (void)ctx;
  (void)func;
  (void)control_type;
  (void)depth;
  /*CALL_HOOK_FUNC(3,
                 "[onControlStart] function id: %zu, control type: %zu, depth:
     %zu\n", fun_index, control_type - CONTROL_BLOCK, depth);*/
}

void
onControlEnd(struct CompilerContext* ctx,
             const struct Function* func,
             control_type_t control_type,
             size_t depth)
{
  (void)ctx;
  (void)func;
  (void)control_type;
  (void)depth;
#if MEMORY_TRACE
  size_t count = mem_tracer(ctx)->size;
  // printf("prev_count: %zu, current_count: %zu\n", prev_count, count);
  printf("[onControlEnd] # access: %zu\n", count - prev_count);
  prev_count = mem_tracer(ctx)->size;
#endif
}

__attribute__((unused)) static void
onInstructionStart(struct CompilerContext* ctx,
                   const struct Function* func,
                   const struct Instr* instr)
{
  struct FuncType* sig = &ctx->sig;
  size_t fun_index = func->fun_index;
  size_t num_params = parameter_count(sig);
  size_t i;

  (void)instr;

  // Spill the input parameters to stack before the call.
  struct ControlBlock* c = control_at(control(ctx), 0);
  if (control_depth(control(ctx)) == 1 && c->cont == 1 &&
      c->type == CONTROL_BLOCK && function_start_flag == 0) {
    for (i = 0; i < num_params; i++) {
      struct StackSlot* slot = &cache_state(ctx)->stack_state->data[i];
      plog("[onInstructionStart] Spill param: %zu, reg: %zu, count: %zu\n",
           i,
           slot->reg,
           cache_state(ctx)->register_use_count[slot->reg]);
      if (slot->loc != LOC_REGISTER) {
        continue;
      }
      assert(is_used(*(cache_state(ctx)->used_registers),
                     cache_state(ctx)->register_use_count,
                     slot->reg));
      Spill(
        output(ctx), num_used_spill_slots(ctx), i, slot->reg, 0, slot->type);
      dec_used(cache_state(ctx)->used_registers,
               cache_state(ctx)->register_use_count,
               slot->reg);
      slot->loc = LOC_STACK;
      slot->reg = REG_UNKNOWN;
    }

    CALL_HOOK_FUNC(2, "[FunctionStart] function id: %zu\n", fun_index);
    function_start_flag = 1;
  }
}
#endif

void
add_pass_test(struct PassManager* pm)
{
  (void)pm;
#if __TEST__
  struct Pass* pass = get_new_pass(pm);
  assert(pass != NULL);
  pass_init(pass,
            pass_name,
            NULL,
            NULL,
            onFunctionStart,
            onFunctionEnd,
            onControlStart,
            onControlEnd,
            onInstructionStart,
            NULL,
            NULL,
            NULL,
            NULL);
#endif
}
