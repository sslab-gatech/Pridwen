#include <sgxwasm/config.h>

#include <sgxwasm/emscripten.h>
#include <sgxwasm/emscripten_runtime_sys.h>
#include <sgxwasm/runtime.h>
#include <sgxwasm/sys.h>
#if !__SGX__
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h> // chmod
#include <sys/types.h>
#endif
#include <time.h>
#if __SGX__
#include "ocall_type.h"
#endif
// XXX: Temporarily set.
#define DEFAULT_BUF_SIZE 256

// TODO: make three paramters as part of EmscriptenContext,
//       and pass the pointer of ctx to WASM program.
static uint64_t emscripten_mem_base = 0;
static uint32_t emscripten_mem_max_size = 0;
static uint32_t emscripten_total_memory = 16777216;
static uint32_t (*emscripten_stack_alloc)(uint32_t) = NULL;
static struct Module* module_ref = NULL;

#define __MMAP0(args, m, ...)
#define __MMAP1(args, m, t, a, ...) m(args, t, a)
#define __MMAP2(args, m, t, a, ...) m(args, t, a) __MMAP1(args, m, __VA_ARGS__)
#define __MMAP3(args, m, t, a, ...) m(args, t, a) __MMAP2(args, m, __VA_ARGS__)
#define __MMAP4(args, m, t, a, ...) m(args, t, a) __MMAP3(args, m, __VA_ARGS__)
#define __MMAP5(args, m, t, a, ...) m(args, t, a) __MMAP4(args, m, __VA_ARGS__)
#define __MMAP6(args, m, t, a, ...) m(args, t, a) __MMAP5(args, m, __VA_ARGS__)
#define __MMAP7(args, m, t, a, ...) m(args, t, a) __MMAP6(args, m, __VA_ARGS__)
#define __MMAP(args, n, ...) __MMAP##n(args, __VA_ARGS__)

#define __DECL(args, t, a) t a;
#define __SWAP(args, t, a) args.a = t##_swap_bytes(args.a);

__attribute__((unused)) static int32_t
int32_t_swap_bytes(int32_t a)
{
  return uint32_t_swap_bytes(a);
}

#define LOAD_ARGS_CUSTOM(args, offset, n, ...)                                 \
  struct                                                                       \
  {                                                                            \
    __MMAP(args, n, __DECL, __VA_ARGS__)                                       \
  } args;                                                                      \
  uint64_t src = emscripten_getMemBase() + offset;                             \
  if (emscripten_copy_from_wasm(&args, (void*)src, sizeof(args)))              \
    assert(0);                                                                 \
  __MMAP(args, n, __SWAP, __VA_ARGS__)

#define LOAD_ARGS(...) LOAD_ARGS_CUSTOM(args, __VA_ARGS__)

enum
{
#define ERRNO(name, value) EM_##name = value,
#include <sgxwasm/emscripten_runtime_sys_errno_def.h>
#undef ERRNO
};

#ifndef __LONG_WIDTH__
#ifdef __LP64__
#define __LONG_WIDTH__ 64
#else
#error Please define __LONG_WIDTH__
#endif
#endif

/* error codes are the same for these targets */
#if defined(__linux__) && defined(__x86_64__)

static int32_t
check_ret(long errno_)
{
#if __LONG_WIDTH__ > 32
  if (errno_ < -2147483648)
    sgxwasm_trap(SGXWASM_TRAP_INTEGER_OVERFLOW);
#endif
#if __LONG_WIDTH__ > 32
  if (errno_ > INT32_MAX)
    sgxwasm_trap(SGXWASM_TRAP_INTEGER_OVERFLOW);
#endif
  return errno_;
}

#else

__attribute__((unused)) static int32_t
check_ret(long errno_)
{
  static int32_t to_sys_errno[] = {
#define ERRNO(name, value) [name] = -value,
#include <sgxwasm/emscripten_runtime_sys_errno_def.h>
#undef ERRNO
  };

  int32_t toret;

  if (errno_ >= 0) {
    return errno_;
  }

  errno_ = -errno_;

  if ((size_t)errno_ >= sizeof(to_sys_errno) / sizeof(to_sys_errno[0])) {
    toret = -EM_EINVAL;
  } else {
    toret = to_sys_errno[errno_];
    if (!toret)
      toret = -EM_EINVAL;
  }

  return toret;
}

#endif

__attribute__((unused)) static void
emscripten_debug_string(uint32_t index)
{
  uint64_t base = emscripten_getMemBase();
  uint64_t addr = base + index;
  sgxwasm_log("[emscripten_debug_string] %u: %s\n", index, (char*)addr);
}

__attribute__((unused)) static void
emscripten_debug_mem(uint32_t index)
{
  uint64_t base = emscripten_getMemBase();
  uint64_t addr = base + index;
  sgxwasm_log("[emscripten_debug_mem] %u: %u\n", index, *(uint32_t*)addr);
}

__attribute__((unused)) static void
emscripten_debug_global(uint32_t index)
{
  struct Value* value = &module_ref->globals.data[index]->value;
  switch (value->type) {
    case VALTYPE_I32:
      sgxwasm_log("[emscripten_debug_global] %u (i32): %u\n", index,
                  value->val);
      break;
    case VALTYPE_I64:
#if __linux__
      sgxwasm_log("[emscripten_debug_global] %u (i64): %lu\n", index,
                  value->val);
#else
      sgxwasm_log("[emscripten_debug_global] %u (i64): %llu\n", index,
                  value->val);
#endif
      break;
    case VALTYPE_F32:
      sgxwasm_log("[emscripten_debug_global] %u (f32): %f\n", index,
                  value->val);
      break;
    case VALTYPE_F64:
      sgxwasm_log("[emscripten_debug_global] %u (f64): %lf\n", index,
                  value->val);
      break;
    default:
      sgxwasm_log("[emscripten_debug_global] %u (unknown)\n", index);
      break;
  }
}
#define emscripten_check_mem_range(addr, size)                                 \
  (((uint64_t)addr >= emscripten_mem_base) &&                                  \
   (((uint64_t)addr + size) < emscripten_mem_base + emscripten_mem_max_size))

#define emscripten_check_mem(addr) emscripten_check_mem_range(addr, 0)

int
emscripten_copy_from_wasm(void* dst, void* src, size_t size)
{
#if 0
  sgxwasm_log("[emscripten_copy_from_wasm] dst: 0x%llx <- src: 0x%llx, size: %zu\n",
         (uint64_t)dst,
         (uint64_t)src,
         size);
#endif
  if (!emscripten_check_mem_range(src, size)) {
    return size;
  }
  memcpy(dst, src, size);
  return 0;
}

int
emscripten_copy_to_wasm(void* dst, void* src, size_t size)
{
#if 0
  sgxwasm_log("[emscripten_copy_to_wasm] dst: 0x%llx <- src: 0x%llx, size: %zu\n",
         (uint64_t)dst,
         (uint64_t)src,
         size);
#endif
  if (!emscripten_check_mem_range(dst, size)) {
    return size;
  }
  memcpy(dst, src, size);
  return 0;
}

// Reference implementation of emscripten arguments handling.

// Get an value at base + offset + pos with given size.
static void
emscripten_get_value(void* ret, uint32_t offset, uint32_t pos, size_t size)
{
  assert(ret != NULL);
  uint64_t base = emscripten_getMemBase();
  uint64_t target_addr = base + offset + pos;
  if (!emscripten_check_mem(target_addr)) {
    assert(0);
  }
  if (emscripten_copy_from_wasm(ret, (void*)target_addr, size)) {
    assert(0);
  }
}

// Write an value to base + offset + pos with given size.
static void
emscripten_set_value(void* ptr, uint32_t offset, uint32_t pos, size_t size)
{
  assert(ptr != NULL);
  uint64_t base = emscripten_getMemBase();
  uint64_t target_addr = base + offset + pos;
  if (offset == 0) {
    return;
  }
  if (!emscripten_check_mem(target_addr)) {
    assert(0);
  }
  if (emscripten_copy_to_wasm((void*)target_addr, ptr, size)) {
    assert(0);
  }
}

// Get an i32 argument.
static uint32_t
emscripten_get(uint32_t* varargs)
{
  assert(varargs != NULL);
  uint64_t base = emscripten_getMemBase();
  uint64_t target_addr = base + *varargs;
  uint32_t ret;
  if (!emscripten_check_mem(target_addr)) {
    assert(0);
  }
  if (emscripten_copy_from_wasm(&ret, (void*)target_addr, sizeof(uint32_t))) {
    assert(0);
  }
  *varargs += 4;
  return ret;
}

// Get an i32 argument.
__attribute__((unused)) static uint32_t
emscripten_get_64(uint32_t* varargs)
{
  assert(varargs != NULL);
  uint64_t base = emscripten_getMemBase();
  uint64_t target_addr = base + *varargs;
  uint32_t ret;
  if (!emscripten_check_mem(target_addr)) {
    assert(0);
  }
  if (emscripten_copy_from_wasm(&ret, (void*)target_addr, sizeof(uint32_t))) {
    assert(0);
  }
  *varargs += 8;
  return ret;
}

static char*
emscripten_get_string_at(uint32_t offset)
{
  size_t str_len;
  char* str;
  uint64_t base = emscripten_getMemBase();
  const char* wasm_str;
  if (!emscripten_check_mem(base + offset)) {
    return NULL;
  }
  wasm_str = (char*)(base + offset);
  str_len = strlen(wasm_str);
  // Check ths size of the string.
  assert(str_len < PATH_MAX);
  str = malloc(str_len + 1);
  if (emscripten_copy_from_wasm(str, (void*)wasm_str, str_len)) {
    return NULL;
  }
  // Insert string terminator.
  str[str_len] = '\0';
  return str;
}

static char*
emscripten_get_string(uint32_t* varargs)
{
  assert(varargs != NULL);
  uint32_t offset = emscripten_get(varargs);
  return emscripten_get_string_at(offset);
}

// Implementation for Emscripten Runtime

void
emscripten_context_init(struct EmscriptenContext* ctx)
{
  memset(ctx, 0, sizeof(struct EmscriptenContext));
}

#define alignMemory(size, factor)                                              \
  (((size) % (factor)) ? ((size) - ((size) % (factor)) + (factor)) : (size))

static uint32_t
staticAlloc(uint32_t* static_top, uint32_t size)
{
  uint32_t ret = *static_top;
  *static_top = (*static_top + size + 15) & ((uint32_t)-16);
  // Memory check.
  return ret;
}

// NOTE: These parameters are set in the .js file
//       generated by emscripten.
struct EmscriptenGlobals*
emscripten_context_derive_memory_globals(struct EmscriptenContext* ctx,
                                         uint32_t static_bump)
{
  struct EmscriptenGlobals* out = &ctx->globals;

#define TOTAL_STACK 5242880
#define STACK_ALIGN_V 16

  uint32_t global_base = 1024; // XXX: Check if it's always fixed.
  uint32_t static_top = global_base + static_bump;

  out->memoryBase = global_base;
  out->tempDoublePtr = staticAlloc(&static_top, 16);
  out->DYNAMICTOP_PTR = staticAlloc(&static_top, 4);
  out->STACKTOP = alignMemory(static_top, STACK_ALIGN_V);
  out->STACK_MAX = out->STACKTOP + TOTAL_STACK;

  assert(out->STACKTOP > out->DYNAMICTOP_PTR &&
         out->DYNAMICTOP_PTR > out->tempDoublePtr);
#if 0
  sgxwasm_log("static_bump: %u, memoryBase: %u, dynamictop_ptr: %u, tempdouble_ptr: %u, stacktop: %u, stackmax: %u\n", static_bump, out->memoryBase, out->DYNAMICTOP_PTR, out->tempDoublePtr, out->STACKTOP, out->STACK_MAX);
#endif
  return out;
}

static void
create_function_type(struct FuncType* ft, size_t n_inputs,
                     sgxwasm_valtype_t* input_types, size_t n_outputs,
                     sgxwasm_valtype_t* output_types)
{
  assert(n_outputs <= 1);
  assert(n_inputs <= sizeof(ft->input_types) / sizeof(ft->input_types[0]));
  memset(ft, 0, sizeof(*ft));

  ft->n_inputs = n_inputs;
  memcpy(ft->input_types, input_types, sizeof(input_types[0]) * n_inputs);

  if (n_outputs) {
    ft->output_type = output_types[0];
  } else {
    ft->output_type = VALTYPE_NULL;
  }
}

int
emscripten_context_set_imports(struct EmscriptenContext* ctx,
                               struct Function* em_errno_location,
                               struct Function* em_malloc,
                               struct Function* em_free, char* envp[])
{
  assert(em_malloc);
  {
    struct FuncType malloc_type;
    sgxwasm_valtype_t malloc_input_type = VALTYPE_I32;
    sgxwasm_valtype_t malloc_return_type = VALTYPE_I32;

    create_function_type(&malloc_type, 1, &malloc_input_type, 1,
                         &malloc_return_type);

    if (!sgxwasm_typecheck_func(&malloc_type, em_malloc))
      return -1;
  }

  if (em_free) {
    struct FuncType free_type;
    sgxwasm_valtype_t free_input_type = VALTYPE_I32;

    create_function_type(&free_type, 1, &free_input_type, 0, NULL);

    if (!sgxwasm_typecheck_func(&free_type, em_free))
      return -1;
  }

  if (em_errno_location) {
    struct FuncType errno_location_type;
    sgxwasm_valtype_t errno_location_return_type = VALTYPE_I32;

    create_function_type(&errno_location_type, 0, NULL, 1,
                         &errno_location_return_type);

    if (!sgxwasm_typecheck_func(&errno_location_type, em_errno_location)) {
      return -1;
    }
  }

  ctx->imports.errno_location = em_errno_location;
  ctx->imports.malloc = em_malloc;
  ctx->imports.free = em_free;
  ctx->environ = envp;
  ctx->buildEnvironmentCalled = 0;

  return 0;
}

void
emscripten_cleanup(struct Module* module)
{
  (void)module;
  /* TODO: implement */
}

void
emscripten_setMemBase(uint64_t addr, uint64_t max_size)
{
  emscripten_mem_base = addr;
  emscripten_mem_max_size = max_size;
  // sgxwasm_log("[setMemBase] base: %lx, size: %zu\n", addr, max_size);
}

uint64_t
emscripten_getMemBase()
{
  assert(emscripten_mem_base != 0);
  return emscripten_mem_base;
}

int
emscripten_setDynamicBase(struct EmscriptenContext* ctx)
{
  uint32_t dyamictop_ptr = ctx->globals.DYNAMICTOP_PTR;
  uint32_t stack_max = ctx->globals.STACK_MAX;
  uint64_t base = emscripten_getMemBase();
  uint32_t dynamic_base;
  int ret;

  dynamic_base = alignMemory(stack_max, 16);
  // Hold the same on X86.
  dynamic_base = uint32_t_swap_bytes(dynamic_base);
  // This should be performed after memory is allocated.
  ret = emscripten_copy_to_wasm((void*)(base + dyamictop_ptr), &dynamic_base,
                                sizeof(dynamic_base));

  // sgxwasm_log("[setDynamicBase] base: %zu\n", dynamic_base);
  // emscripten_debug_mem(dyamictop_ptr);

  return ret;
}

void
emscripten_setModuleRef(struct Module* module)
{
  module_ref = module;
}

int
emscripten_build_environment(struct Function* constructor)
{
  (void)constructor;
  return 0;
}

void
memory_preloading()
{
  uint64_t mem_start = emscripten_mem_base & 0xfffffffffffff000;
  uint64_t mem_end = (mem_start + emscripten_total_memory) & 0xfffffffffffff000;
  uint8_t access;

#if __DEBUG_TSGX__
  sgxwasm_log("memory_preloading (@0x%llx - @0x%llx)\n", mem_start, mem_end);
#endif
  while (mem_start < mem_end) {
    access = *((uint8_t*)(mem_start));
    *((uint8_t*)(mem_start)) = access;
    mem_start += 4096;
#if __DEBUG_TSGX__
    sgxwasm_log("preloading: @0x%llx, byte: %x\n", mem_start, access);
#endif
  }
}

int
emscripten_invoke_main(struct Function* em_stack_alloc,
                       struct Function* em_main, int argc, char* argv[])
{
  // uint32_t (*stack_alloc)(uint32_t);
  int ret = -1;

  assert(parameter_count(&em_stack_alloc->type) == 1 &&
         parameter_types(&em_stack_alloc->type)[0] == VALTYPE_I32 &&
         return_type(&em_stack_alloc->type) == VALTYPE_I32);

  emscripten_stack_alloc = em_stack_alloc->code;

#if __TSGX__
  // Ensure the linear memory is mapped.
  memory_preloading();
#endif

#if __DEMO__
  printf("[PRIDWEN] Invoking synthesized binary ... \n");
#endif

  if (parameter_count(&em_main->type) == 0) { // int main()
    assert(return_type(&em_main->type) == VALTYPE_I32);
    uint32_t((*_main)()) = em_main->code;
// invoke main.
#if 0 // Debug.
    sync_state = 1;
    while (sync_state == 1) {}
    uint64_t start, end;
    unsigned tmp;
    start = counter;
    tmp = mem[0];
    end = counter;
    printf("timing: %lu (%lu - %lu)\n", end - start, end, start);
#endif
#if SGXWASM_BENCH
    uint64_t t1, t2;
    ocall_sgx_rdtsc(&t1);
    //for (int i = 0; i < 1000; i++) {
      ret = _main();
    //}
    ocall_sgx_rdtsc(&t2);
    printf("%lu\n", t2 - t1);
    //printf("%.3lf\n", (double)(t2 - t1)/1000);
#else
    ret = _main();
#endif
  } else if (parameter_count(&em_main->type) ==
             2) { // int main(int argc, char *argv[])
    assert(parameter_types(&em_main->type)[0] == VALTYPE_I32 &&
           parameter_types(&em_main->type)[1] == VALTYPE_I32 &&
           return_type(&em_main->type) == VALTYPE_I32);
    // Stack layout for parameter passing:
    // | argv_index_0 ... argv_index_n | argv[0] ... argv[n]
    uint32_t((*_main)(uint32_t, uint32_t)) = em_main->code;
    uint64_t base = emscripten_getMemBase();
    // uint32_t argv_index = stack_alloc((argc + 1) * I32Size);
    uint32_t argv_index = emscripten_stack_alloc((argc + 1) * I32Size);
    const uint32_t zero = 0;
    int i;

    for (i = 0; i < argc; i++) {
      size_t len = strlen(argv[i]) + 1;
      // uint32_t offset = stack_alloc(len);
      uint32_t offset = emscripten_stack_alloc(len);
      if (emscripten_copy_to_wasm((void*)(base + offset), argv[i], len)) {
        ret = -1;
        goto done;
      }
      offset = uint32_t_swap_bytes(offset);
      if (emscripten_copy_to_wasm((void*)(base + argv_index + i * I32Size),
                                  &offset, I32Size)) {
        ret = -1;
        goto done;
      }
    }
    // Check: padding zero.
    if (emscripten_copy_to_wasm((void*)(base + argv_index * argc * I32Size),
                                (void*)&zero, I32Size)) {
      ret = -1;
      goto done;
    }

// invoke main
#if SGXWASM_BENCH
    uint64_t t1, t2;
    ocall_sgx_rdtsc(&t1);
#endif
    ret = _main(argc, argv_index);
#if SGXWASM_BENCH
    ocall_sgx_rdtsc(&t2);
    printf("%lu\n", t2 - t1);
#endif
  }
done:
  return ret;
}

// Implementation of imported functions from emscripten.

uint32_t
emscripten_enlargeMemory()
{
  sgxwasm_log("[enlargeMemory]\n");
  emscripten_debug_global(8);
  emscripten_debug_mem(7040);
  emscripten_debug_global(9);
  emscripten_debug_mem(7024);
  return 0;
}

uint32_t
emscripten_getTotalMemory()
{
  return emscripten_total_memory;
}

uint32_t
emscripten_abortOnCannotGrowMemory()
{
  sgxwasm_log("[abortOnCannotGrowMemory]\n");
  return 0;
}

void
emscripten_abortStackOverflow(uint32_t arg)
{
  sgxwasm_log("[abortStackOverflow]: arg - %d\n", arg);
}

void
emscripten_nullFunc_i(uint32_t arg)
{
  sgxwasm_log("[nullFunc_ii]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_ii(uint32_t arg)
{
  sgxwasm_log("[nullFunc_ii]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_iii(uint32_t arg)
{
  sgxwasm_log("[nullFunc_iii]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_iiii(uint32_t arg)
{
  sgxwasm_log("[nullFunc_iiii]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_iiiii(uint32_t arg)
{
  sgxwasm_log("[nullFunc_iiiii]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_iiiiii(uint32_t arg)
{
  sgxwasm_log("[nullFunc_iiiiii]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_iiiiiii(uint32_t arg)
{
  sgxwasm_log("[nullFunc_iiiiiii]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_iiiij(uint32_t arg)
{
  sgxwasm_log("[nullFunc_iiiij]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_iij(uint32_t arg)
{
  sgxwasm_log("[nullFunc_iij]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_iiji(uint32_t arg)
{
  sgxwasm_log("[nullFunc_iiji]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_iijii(uint32_t arg)
{
  sgxwasm_log("[nullFunc_iijii]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_v(uint32_t arg)
{
  sgxwasm_log("[nullFunc_v]: arg - %d\n", arg);
  (void)arg;
  // abort();
}

void
emscripten_nullFunc_vi(uint32_t arg)
{
  sgxwasm_log("[nullFunc_vi]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_vii(uint32_t arg)
{
  sgxwasm_log("[nullFunc_vii]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_viii(uint32_t arg)
{
  sgxwasm_log("[nullFunc_viii]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_viiii(uint32_t arg)
{
  sgxwasm_log("[nullFunc_viiii]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_viiiii(uint32_t arg)
{
  sgxwasm_log("[nullFunc_viiiii]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_viiiiii(uint32_t arg)
{
  sgxwasm_log("[nullFunc_viiiiii]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_viiiiiii(uint32_t arg)
{
  sgxwasm_log("[nullFunc_viiiiiii]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_viiiij(uint32_t arg)
{
  sgxwasm_log("[nullFunc_viiiij]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten_nullFunc_viij(uint32_t arg)
{
  sgxwasm_log("[nullFunc_viij]: arg - %d\n", arg);
  (void)arg;
  abort();
}

void
emscripten____assert_fail(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                          uint32_t arg4)
{
  //sgxwasm_log("[___assert_fail]: arg - (%d, %d, %d, %d)\n", arg1, arg2, arg3,
  //            arg4);
}

void
emscripten____lock(uint32_t arg)
{
  (void)arg;
#if DEBUG_EM_CALLS
  sgxwasm_log("[___lock]: arg - %d\n", arg);
#endif
}

void
emscripten____setErrNo(uint32_t arg)
{
  sgxwasm_log("[___setErrNo]: arg - %d\n", arg);
}

uint32_t
emscripten____map_file(uint32_t arg1, uint32_t arg2)
{
  sgxwasm_log("[___map_file]: arg1 - %u, arg2: %u\n", arg1, arg2);
  return 0;
}

// Helper functions for syscalls support

static void
read_iov(struct iovec* iov, uint32_t iovp, uint32_t iovcnt)
{
  size_t i;
  assert(iov != NULL);

  for (i = 0; i < iovcnt; i++) {
    // Fix size of struct iovec to 8.
    uint32_t base = iovp + 8 * i;
    uint32_t ptr;
    uint32_t len;
    emscripten_get_value((void*)&ptr, base, 0, sizeof(uint32_t));
    emscripten_get_value((void*)&len, base, 4, sizeof(uint32_t));
    // iov_len = uint32_t_swap_bytes(iovec.iov_len);
    // iov_base = uint32_t_swap_bytes(iovec.iov_base);
    iov[i].iov_len = len;
    iov[i].iov_base = NULL;
    if (iov[i].iov_len != 0) {
      iov[i].iov_base = malloc(iov[i].iov_len);
      emscripten_get_value((void*)iov[i].iov_base, ptr, 0, iov[i].iov_len);
    }
  }
}

static void
init_iov(struct iovec* iov, uint32_t iovp, uint32_t iovcnt)
{
  size_t i;
  assert(iov != NULL);

  for (i = 0; i < iovcnt; i++) {
    // Fix size of struct iovec to 8.
    uint32_t base = iovp + 8 * i;
    uint32_t len;
    emscripten_get_value((void*)&len, base, 4, sizeof(uint32_t));
    // iov_len = uint32_t_swap_bytes(iovec.iov_len);
    // iov_base = uint32_t_swap_bytes(iovec.iov_base);
    iov[i].iov_len = len;
    if (iov[i].iov_len != 0) {
      iov[i].iov_base = calloc(1, iov[i].iov_len);
    }
  }
}

static void
write_iov(uint32_t iovp, uint32_t iovcnt, struct iovec* iov)
{
  size_t i;
  assert(iov != NULL);

  for (i = 0; i < iovcnt; i++) {
    // Fix size of struct iovec to 8.
    uint32_t base = iovp + 8 * i;
    uint32_t ptr;
    uint32_t len;
    emscripten_get_value((void*)&ptr, base, 0, sizeof(uint32_t));
    emscripten_get_value((void*)&len, base, 4, sizeof(uint32_t));
    // iov_len = uint32_t_swap_bytes(iovec.iov_len);
    // iov_base = uint32_t_swap_bytes(iovec.iov_base);
    if (iov[i].iov_len != 0) {
      assert(iov[i].iov_len <= len);
      // if (ptr >= 1024) {
      emscripten_set_value((void*)iov[i].iov_base, ptr, 0, iov[i].iov_len);
      //}
    }
  }
}

/*
st_dev: 0
st_ino: 72
st_nlink: 16
st_mode: 12
st_uid: 20
st_gid: 24
st_rdev: 28
st_size: 36
st_blksize: 40
st_blocks: 44
st_atim: 48
st_atime_nsec: 52
st_mtim: 56
st_mtime_nsec: 60
st_ctim: 64
st_ctime_nsec: 68
*/

static void
write_stat(uint32_t bufp, struct stat* statbuf)
{
  emscripten_set_value((void*)&statbuf->st_dev, bufp, 0, sizeof(uint32_t));
  // emscripten_set_value((void *)&zero, bufp, 4, sizeof(uint32_t));
  emscripten_set_value((void*)&statbuf->st_mode, bufp, 12, sizeof(uint16_t));
  emscripten_set_value((void*)&statbuf->st_nlink, bufp, 16, sizeof(uint16_t));
  emscripten_set_value((void*)&statbuf->st_uid, bufp, 20, sizeof(uint32_t));
  emscripten_set_value((void*)&statbuf->st_gid, bufp, 24, sizeof(uint32_t));
  emscripten_set_value((void*)&statbuf->st_rdev, bufp, 28, sizeof(uint32_t));
  emscripten_set_value((void*)&statbuf->st_size, bufp, 36, sizeof(uint32_t));
  emscripten_set_value((void*)&statbuf->st_blksize, bufp, 40, sizeof(uint32_t));
  emscripten_set_value((void*)&statbuf->st_blocks, bufp, 44, sizeof(uint32_t));
#if !__linux__
  emscripten_set_value((void*)&statbuf->st_atimespec.tv_sec, bufp, 48,
                       sizeof(uint32_t));
  emscripten_set_value((void*)&statbuf->st_atimespec.tv_nsec, bufp, 52,
                       sizeof(uint32_t));
  emscripten_set_value((void*)&statbuf->st_mtimespec.tv_sec, bufp, 56,
                       sizeof(uint32_t));
  emscripten_set_value((void*)&statbuf->st_mtimespec.tv_nsec, bufp, 60,
                       sizeof(uint32_t));
  emscripten_set_value((void*)&statbuf->st_ctimespec.tv_sec, bufp, 64,
                       sizeof(uint32_t));
  emscripten_set_value((void*)&statbuf->st_ctimespec.tv_nsec, bufp, 68,
                       sizeof(uint32_t));
#else
  emscripten_set_value((void*)&statbuf->st_atim.tv_sec, bufp, 48,
                       sizeof(uint32_t));
  emscripten_set_value((void*)&statbuf->st_atim.tv_nsec, bufp, 52,
                       sizeof(uint32_t));
  emscripten_set_value((void*)&statbuf->st_mtim.tv_sec, bufp, 56,
                       sizeof(uint32_t));
  emscripten_set_value((void*)&statbuf->st_mtim.tv_nsec, bufp, 60,
                       sizeof(uint32_t));
  emscripten_set_value((void*)&statbuf->st_ctim.tv_sec, bufp, 64,
                       sizeof(uint32_t));
  emscripten_set_value((void*)&statbuf->st_ctim.tv_nsec, bufp, 68,
                       sizeof(uint32_t));
#endif
  emscripten_set_value((void*)&statbuf->st_ino, bufp, 72, sizeof(uint32_t));
}

// Emscripten socket support

#if !__linux__
enum
{
  OSX_O_CREAT = 0x200
};
__attribute__((unused)) static void
convert_flags(uint32_t* flags)
{
  // O_CREAT is 0x40 on linux.
  if (*flags & 0x40) {
    *flags ^= 0x40;
    *flags &= OSX_O_CREAT;
  }
}
#endif

enum
{
  EM_AF_INET = 2,
  EM_AF_INET6 = 10
};

static uint32_t
get_af_family(uint32_t em_family)
{
  switch (em_family) {
    case EM_AF_INET:
      return AF_INET;
    case EM_AF_INET6:
      return AF_INET6;
    default:
      return em_family;
  }
}

static void
write_sockaddr(uint32_t sa, uint32_t sa_len, struct sockaddr* addr,
               uint32_t* addrlen)
{
  uint16_t family = 0;
  if (*addrlen == 0) {
    return;
  }
  family = addr->sa_family;
  switch (family) {
    case AF_INET: {
      // assert(*addrlen >= sizeof(struct sockaddr_in));
      struct sockaddr_in* p = (struct sockaddr_in*)addr;
      // XXX: Check offset
      emscripten_set_value((void*)&p->sin_family, sa, 0, sizeof(uint16_t));
      emscripten_set_value((void*)&p->sin_port, sa, 2, sizeof(uint16_t));
      emscripten_set_value((void*)&p->sin_addr, sa, 4, sizeof(uint32_t));

      emscripten_set_value((void*)addrlen, sa_len, 0, sizeof(uint32_t));
      break;
    }
    case AF_INET6: {
      // assert(*addrlen >= sizeof(struct sockaddr_in6));
      struct sockaddr_in6* p = (struct sockaddr_in6*)addr;
      // XXX: Check offset
      emscripten_set_value((void*)&p->sin6_family, sa, 0, sizeof(uint16_t));
      emscripten_set_value((void*)&p->sin6_port, sa, 2, sizeof(uint16_t));
      emscripten_set_value((void*)&p->sin6_flowinfo, sa, 4, sizeof(uint32_t));
      emscripten_set_value((void*)&p->sin6_addr, sa, 8, sizeof(uint32_t) * 4);
      emscripten_set_value((void*)&p->sin6_scope_id, sa, 16, sizeof(uint32_t));

      emscripten_set_value((void*)addrlen, sa_len, 0, sizeof(uint32_t));
      break;
    }
    default:
      // Unsupported family.
      assert(0);
  }
}

static void
read_sockaddr(struct sockaddr_storage* info, uint32_t sa, uint32_t sa_len)
{
  uint16_t family;
  uint16_t port;
  emscripten_get_value((void*)&family, sa, 0, sizeof(uint16_t));
  emscripten_get_value((void*)&port, sa, 2, sizeof(uint16_t));
  // Convert em_family to family fo c socket.
  family = get_af_family(family);
  switch (family) {
    case AF_INET: {
      assert(sa_len == sizeof(struct sockaddr_in));
      struct sockaddr_in* p = (struct sockaddr_in*)info;
      p->sin_family = family;
      p->sin_port = port;
      emscripten_get_value((void*)&p->sin_addr, sa, 4, sizeof(uint32_t));
      // *addr = __inet_ntop4_raw(*addr);
      break;
    }
    case AF_INET6: {
      assert(sa_len == sizeof(struct sockaddr_in6));
      struct sockaddr_in6* p = (struct sockaddr_in6*)info;
      p->sin6_family = family;
      p->sin6_port = port;
      emscripten_get_value((void*)&p->sin6_addr, sa, 8, sizeof(uint32_t) * 4);
      // *addr = __inet_ntop6_raw(addr);
      break;
    }
    default:
      // Unsupported family
      assert(0);
  }
}

static struct sockaddr_storage*
emscripten_get_socket_address(uint32_t addrp, uint32_t addrlen, int allow_null)
{
  struct sockaddr_storage* info;
  if (allow_null && addrp == 0) {
    return NULL;
  }
  info = calloc(1, sizeof(struct sockaddr_storage));
  read_sockaddr(info, addrp, addrlen);

  return info;
}

static void
write_msghdr(struct msghdr* msg, uint32_t base)
{
  uint32_t namep;
  uint32_t namelenp;
  uint32_t iovp;
  uint32_t controlp;
  uint32_t controllenp;
  uint32_t controllen;

  // Write sock name.
  emscripten_get_value(&namep, base, 0, sizeof(uint32_t));
  namelenp = base + 4;
  write_sockaddr(namep, namelenp, (struct sockaddr*)msg->msg_name,
                 &msg->msg_namelen);
  // Write iov.
  emscripten_get_value(&iovp, base, 8, sizeof(uint32_t));
  write_iov(iovp, msg->msg_iovlen, msg->msg_iov);
  // Write control.
  emscripten_get_value(&controlp, base, 16, sizeof(uint32_t));
  controllenp = base + 20;
  emscripten_get_value(&controllen, controllenp, 0, sizeof(uint32_t));
  assert(controllen >= msg->msg_controllen);
  emscripten_set_value(msg->msg_control, controlp, 0, msg->msg_controllen);
  emscripten_set_value(&msg->msg_controllen, controllenp, 0, sizeof(uint32_t));
  // Write flags.
  emscripten_set_value(&msg->msg_flags, base, 24, sizeof(uint32_t));
}

__attribute__((unused)) static void
read_msghdr(struct msghdr* msg, uint32_t base)
{
  uint32_t namep;
  uint32_t namelen;
  uint32_t iovp;
  uint32_t iovlen;
  uint32_t controlp;
  uint32_t controllen;

  // Read sock name.
  emscripten_get_value(&namep, base, 0, sizeof(uint32_t));
  emscripten_get_value(&namelen, base, 4, sizeof(uint32_t));
  if (namelen > 0) {
    msg->msg_name = calloc(1, namelen);
    read_sockaddr((struct sockaddr_storage*)msg->msg_name, namep, namelen);
  }
  msg->msg_namelen = namelen;
  // Read iov.
  emscripten_get_value(&iovp, base, 8, sizeof(uint32_t));
  emscripten_get_value(&iovlen, base, 12, sizeof(uint32_t));
  if (iovlen > 0) {
    msg->msg_iov = malloc(sizeof(struct iovec) * iovlen);
    read_iov(msg->msg_iov, iovp, iovlen);
  }
  msg->msg_iovlen = iovlen;
  // Read control.
  emscripten_get_value(&controlp, base, 16, sizeof(uint32_t));
  emscripten_get_value(&controllen, base, 20, sizeof(uint32_t));
  if (controllen > 0) {
    msg->msg_control = malloc(controllen);
    emscripten_get_value(msg->msg_control, controlp, 0, controllen);
  }
  msg->msg_controllen = controllen;
  // Read flags.
  emscripten_get_value(&msg->msg_flags, base, 24, sizeof(uint32_t));
}

uint32_t
check_fd(uint32_t fd, uint32_t low, uint32_t high, uint32_t val)
{
  return (fd < 32 ? (low & val) : (high & val));
}

// XXX: TEST-NEEDED
__attribute__((unused)) static void
read_flock(struct flock* lock, uint32_t base)
{
  emscripten_get_value(&lock->l_type, base, 0, sizeof(uint16_t));
  emscripten_get_value(&lock->l_whence, base, 2, sizeof(uint16_t));
  emscripten_get_value(&lock->l_start, base, 4, sizeof(uint32_t));
  emscripten_get_value(&lock->l_len, base, 8, sizeof(uint32_t));
  emscripten_get_value(&lock->l_pid, base, 12, sizeof(uint32_t));
}

// Emscripten syscalls support

// __syscall1: exit

// unlink
// Status: DONE.
uint32_t
emscripten____syscall10(uint32_t which, uint32_t varargs)
{
  assert(which == 10);
  char* path = emscripten_get_string(&varargs);
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 10] %u, %u\n", which, varargs);
#endif
  return check_ret(sys_unlink(path));
}

// __syscall100: fstatfs
// __syscall101: oiperm

// socketcall
// Status: DONE.
uint32_t
emscripten____syscall102(uint32_t which, uint32_t varargs)
{
  assert(which == 102);
  uint32_t call = emscripten_get(&varargs);
  uint32_t socketvararg = emscripten_get(&varargs);
  int ret;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall102] call: %u, socketvararg: %u\n", call, socketvararg);
#endif
  switch (call) {
    case 1: { // socket
      uint32_t domain = emscripten_get(&socketvararg);
      uint32_t type = emscripten_get(&socketvararg);
      uint32_t protocol = emscripten_get(&socketvararg);
      domain = get_af_family(domain);
      ret = sys_socket(domain, type, protocol);
      // assert(ret < 64); // XXX: Emscripten check
      break;
    }
    case 2: { // bind
      uint32_t fd = emscripten_get(&socketvararg);
      uint32_t addrp = emscripten_get(&socketvararg);
      uint32_t addrlen = emscripten_get(&socketvararg);
      struct sockaddr_storage* info =
        emscripten_get_socket_address(addrp, addrlen, 0);
      assert(info);
      ret = sys_bind(fd, (struct sockaddr*)info, addrlen);
      break;
    }
    case 3: { // connect
      uint32_t fd = emscripten_get(&socketvararg);
      uint32_t addrp = emscripten_get(&socketvararg);
      uint32_t addrlen = emscripten_get(&socketvararg);
      struct sockaddr_storage* info =
        emscripten_get_socket_address(addrp, addrlen, 0);
      assert(info);
      ret = sys_connect(fd, (struct sockaddr*)info, addrlen);
      break;
    }
    case 4: { // listen
      uint32_t fd = emscripten_get(&socketvararg);
      uint32_t backlog = emscripten_get(&socketvararg);
      ret = sys_listen(fd, backlog);
      break;
    }
    case 5: { // accept
      uint32_t fd = emscripten_get(&socketvararg);
      uint32_t addrp = emscripten_get(&socketvararg);
      uint32_t addrlenp = emscripten_get(&socketvararg);
      struct sockaddr addr;
      uint32_t addrlen = sizeof(addr);
      ret = sys_accept(fd, &addr, &addrlen);
      if (ret < 0) {
        break;
      }
      if (addrp) {
        write_sockaddr(addrp, addrlenp, &addr, &addrlen);
      }
      break;
    }
    case 6: { // getsockname
      uint32_t fd = emscripten_get(&socketvararg);
      uint32_t addrp = emscripten_get(&socketvararg);
      uint32_t addrlenp = emscripten_get(&socketvararg);
      struct sockaddr addr;
      uint32_t addrlen = sizeof(addr);
      ret = sys_getsockname(fd, &addr, &addrlen);
      if (ret < 0) {
        break;
      }
      assert(addrp);
      write_sockaddr(addrp, addrlenp, &addr, &addrlen);
      break;
    }
    case 7: { // getpeername
      uint32_t fd = emscripten_get(&socketvararg);
      uint32_t addrp = emscripten_get(&socketvararg);
      uint32_t addrlenp = emscripten_get(&socketvararg);
      struct sockaddr addr;
      uint32_t addrlen = sizeof(addr);
      ret = sys_getpeername(fd, &addr, &addrlen);
      if (ret < 0) {
        break;
      }
      assert(addrp);
      write_sockaddr(addrp, addrlenp, &addr, &addrlen);
      break;
    }
    case 11: { // sento
      uint32_t fd = emscripten_get(&socketvararg);
      uint32_t message = emscripten_get(&socketvararg);
      uint32_t length = emscripten_get(&socketvararg);
      uint32_t flags = emscripten_get(&socketvararg);
      uint32_t addrp = emscripten_get(&socketvararg);
      uint32_t addrlen = emscripten_get(&socketvararg);
      struct sockaddr_storage* dest =
        emscripten_get_socket_address(addrp, addrlen, 1);
      void* buf = malloc(length);
      emscripten_get_value(buf, message, 0, length);
      if (!dest) {
        // send, no address provided.
        ret = sys_send(fd, (void*)buf, length, flags);
      } else {
        ret = sys_sendto(fd, (void*)buf, length, flags, (struct sockaddr*)dest,
                         addrlen);
      }
      free(buf);
      break;
    }
    case 12: { // recvfrom
      uint32_t fd = emscripten_get(&socketvararg);
      uint32_t bufp = emscripten_get(&socketvararg);
      uint32_t len = emscripten_get(&socketvararg);
      uint32_t flags = emscripten_get(&socketvararg);
      uint32_t addrp = emscripten_get(&socketvararg);
      uint32_t addrlenp = emscripten_get(&socketvararg);
      char* buf = calloc(1, len);
      struct sockaddr addr;
      uint32_t addrlen;
      emscripten_get_value((void*)&addrlen, addrlenp, 0, sizeof(uint32_t));
      if (!addrp) {
        ret = sys_recv(fd, (void*)buf, len, flags);
      } else {
        ret = sys_recvfrom(fd, (void*)buf, len, flags, &addr, &addrlen);
        write_sockaddr(addrp, addrlenp, &addr, &addrlen);
      }
      // Copy the received data back to wasm memory.
      emscripten_set_value(buf, bufp, 0, len);
      free(buf);
      break;
    }
    case 13: { // shutdown
      uint32_t fd = emscripten_get(&socketvararg);
      uint32_t how = emscripten_get(&socketvararg);
      ret = sys_shutdown(fd, how);
      break;
    }
    case 14: { // setsockopt
#if __linux__
      uint32_t fd = emscripten_get(&socketvararg);
      uint32_t level = emscripten_get(&socketvararg);
      uint32_t optname = emscripten_get(&socketvararg);
      uint32_t optp = emscripten_get(&socketvararg);
      uint32_t optlen = emscripten_get(&socketvararg);
      char* opt = malloc(optlen);
      emscripten_get_value((void*)opt, optp, 0, optlen);
      ret = sys_setsockopt(fd, level, optname, (void*)opt, optlen);
      free(opt);
#else
      ret = 0;
#endif
      break;
    }
    case 15: { // getsockopt
      uint32_t fd = emscripten_get(&socketvararg);
      uint32_t level = emscripten_get(&socketvararg);
      uint32_t optname = emscripten_get(&socketvararg);
      uint32_t optp = emscripten_get(&socketvararg);
      uint32_t optlenp = emscripten_get(&socketvararg);
      size_t opt;
      uint32_t optlen;
      ret = sys_getsockopt(fd, level, optname, (void*)&opt, &optlen);
      assert(optlen <= sizeof(opt));
      emscripten_set_value((void*)&opt, optp, 0, optlen);
      emscripten_set_value((void*)&optlen, optlenp, 0, sizeof(optlen));
      break;
    }
    case 16: { // sendmsg
      uint32_t fd = emscripten_get(&socketvararg);
      uint32_t msg = emscripten_get(&socketvararg);
      uint32_t flags = emscripten_get(&socketvararg);
      struct msghdr msghdr;
      memset(&msghdr, 0, sizeof(struct msghdr));
      read_msghdr(&msghdr, msg);
      ret = sys_sendmsg(fd, &msghdr, flags);
      if (msghdr.msg_name) {
        free(msghdr.msg_name);
      }
      if (msghdr.msg_iov) {
#if __linux__
        size_t i;
#else
        int i;
#endif
        for (i = 0; i < msghdr.msg_iovlen; i++) {
          free(msghdr.msg_iov[i].iov_base);
        }
        free(msghdr.msg_iov);
      }
      if (msghdr.msg_control) {
        free(msghdr.msg_control);
      }
      break;
    }
    case 17: { // recvmsg
      uint32_t fd = emscripten_get(&socketvararg);
      uint32_t msg = emscripten_get(&socketvararg);
      uint32_t flags = emscripten_get(&socketvararg);
      struct msghdr msghdr;
      uint32_t namep;
      uint32_t iovp;
      memset((void*)&msghdr, 0, sizeof(struct msghdr));
      // Initialize name.
      emscripten_get_value(&namep, msg, 0, sizeof(uint32_t));
      if (namep != 0) {
        struct sockaddr_storage addr;
        emscripten_get_value(&msghdr.msg_namelen, msg, 4, sizeof(uint32_t));
        msghdr.msg_name = (void*)&addr;
      }
      // Initialize iov.
      emscripten_get_value(&msghdr.msg_iovlen, msg, 12, sizeof(uint32_t));
      assert(msghdr.msg_iovlen > 0);
      msghdr.msg_iov = malloc(sizeof(struct iovec) * msghdr.msg_iovlen);
      emscripten_get_value(&iovp, msg, 8, sizeof(uint32_t));
      init_iov(msghdr.msg_iov, iovp, msghdr.msg_iovlen);
      // Initialize control.
      emscripten_get_value(&msghdr.msg_controllen, msg, 20, sizeof(uint32_t));
      msghdr.msg_control = malloc(msghdr.msg_controllen);
      ret = sys_recvmsg(fd, &msghdr, flags);
      if (ret >= 0) {
        write_msghdr(&msghdr, msg);
      }
      break;
    }
    case 18: { // accept4
      uint32_t fd = emscripten_get(&socketvararg);
      uint32_t addrp = emscripten_get(&socketvararg);
      uint32_t addrlenp = emscripten_get(&socketvararg);
      uint32_t flags = emscripten_get(&socketvararg);
      struct sockaddr addr;
      uint32_t addrlen = sizeof(addr);
#if __linux__
      ret = sys_accept4(fd, &addr, &addrlen, flags);
#else
      (void)flags;
      ret = sys_accept(fd, &addr, &addrlen);
#endif
      // ret = sys_accept(fd, &addr, &addrlen);
      if (ret < 0) {
        break;
      }
      if (addrp) {
        write_sockaddr(addrp, addrlenp, &addr, &addrlen);
      }
      break;
    }
    default: {
      // Unspported socketcall
      sgxwasm_log("call: %u\n", call);
      assert(0);
    }
  }
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 102] socket, return %d\n", ret);
#endif
  return check_ret(ret);
}

// __syscall103: syslog
// __syscall104: setitimer
// __syscall105: getwtitimer
// __syscall106: stat
// __syscall107: lstat
// __syscall108: fstat
// __syscall109: olduname

// __syscall11: execve

// __syscall110: iopl
// __syscall111: vhangup
// __syscall112: idle
// __syscall113: vm86old
// __syscall114: wait4

// fsync
// Status: TEST-NEEDED.
uint32_t
emscripten____syscall118(uint32_t which, uint32_t varargs)
{
  assert(which == 118);
  uint32_t fd = emscripten_get(&varargs);
  uint32_t ret = fsync(fd);
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 118] fsync %u, %u\n", which, varargs);
#endif
  return check_ret(ret);
}

// chdir
// Status: TEST-NEEDED.
uint32_t
emscripten____syscall12(uint32_t which, uint32_t varargs)
{
  assert(which == 12);
  char* path = emscripten_get_string(&varargs);
  assert(!path);
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 12] %u, %u\n", which, varargs);
#endif
  return check_ret(sys_chdir(path));
}

// __syscall121: setdomainname

// uname
// Status: DONE.
uint32_t
emscripten____syscall122(uint32_t which, uint32_t varargs)
{
  assert(which == 122);
  uint32_t buf = emscripten_get(&varargs);
  assert(buf);
  const char* sysname = "Emscripten";
  const char* nodename = "emscripten";
  const char* release = "1.0";
  const char* version = "#1";
  const char* machine = "x86-JS";
  // XXX: This is a default size set by emscripten.
  const uint32_t buf_size = 65;
  uint32_t offset = 0;

  // Hardcoded value.
  emscripten_set_value((void*)sysname, buf, offset, strlen(sysname));
  offset += buf_size;
  emscripten_set_value((void*)nodename, buf, offset, strlen(nodename));
  offset += buf_size;
  emscripten_set_value((void*)release, buf, offset, strlen(release));
  offset += buf_size;
  emscripten_set_value((void*)version, buf, offset, strlen(version));
  offset += buf_size;
  emscripten_set_value((void*)machine, buf, offset, strlen(machine));
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 122] %u, %u\n", which, varargs);
#endif
  return 0;
}

// __syscall125: mprotect

// __syscall13: time

// __syscall132: getpid
// __syscall133: fchdir

// __syscall14: mknod

// llseek
// Status: DONE.
uint32_t
emscripten____syscall140(uint32_t which, uint32_t varargs)
{
  assert(which == 140);

  uint32_t fd = emscripten_get(&varargs);
  uint32_t offset_high = emscripten_get(&varargs);
  uint32_t offset_low = emscripten_get(&varargs);
  uint32_t resultp = emscripten_get(&varargs);
  uint32_t whence = emscripten_get(&varargs);
  uint32_t result;
  // offset_high is unused.
  (void)offset_high;
  result = sys_lseek(fd, offset_low, whence);
  emscripten_set_value((void*)&result, resultp, 0, sizeof(uint32_t));
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 140] %u, %u\n", which, varargs);
#endif
  return 0;
}

// newselect
// Status: DONE.
uint32_t
emscripten____syscall142(uint32_t which, uint32_t varargs)
{
  assert(which == 142);
  uint32_t nfds = emscripten_get(&varargs);
  uint32_t readfdsp = emscripten_get(&varargs);
  uint32_t writefdsp = emscripten_get(&varargs);
  uint32_t exceptfdsp = emscripten_get(&varargs);
  uint32_t timeoutp = emscripten_get(&varargs);
  long ret = 0;

  assert(nfds <= 64);      // fd sets have 64 bits
  assert(exceptfdsp == 0); // exceptfds not supported.
#if __linux__
  fd_set* readfds = NULL;
  fd_set* writefds = NULL;
  fd_set* exceptfds = NULL;
#else
  struct fd_set* readfds = NULL;
  struct fd_set* writefds = NULL;
  struct fd_set* exceptfds = NULL;
#endif

  struct timeval timeout;

  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  if (readfdsp != 0) {
#if __linux__
    readfds = malloc(sizeof(fd_set));
    memset(readfds, 0, sizeof(fd_set));
#else
    readfds = malloc(sizeof(struct fd_set));
    memset(readfds, 0, sizeof(struct fd_set));
#endif
    emscripten_get_value((void*)readfds, readfdsp, 0, sizeof(uint64_t));
  }
  if (writefds != 0) {
#if __linux__
    writefds = malloc(sizeof(fd_set));
    memset(writefds, 0, sizeof(fd_set));
#else
    writefds = malloc(sizeof(struct fd_set));
    memset(writefds, 0, sizeof(struct fd_set));
#endif
    emscripten_get_value((void*)&writefds, writefdsp, 0, sizeof(uint64_t));
  }
  if (exceptfds != 0) {
#if __linux__
    exceptfds = malloc(sizeof(fd_set));
    memset(exceptfds, 0, sizeof(fd_set));
#else
    exceptfds = malloc(sizeof(struct fd_set));
    memset(exceptfds, 0, sizeof(struct fd_set));
#endif
    emscripten_get_value((void*)&exceptfds, exceptfdsp, 0, sizeof(uint64_t));
  }
  if (timeoutp != 0) {
    emscripten_get_value((void*)&timeout.tv_sec, timeoutp, 0, sizeof(uint32_t));
    emscripten_get_value((void*)&timeout.tv_usec, timeoutp, 4,
                         sizeof(uint32_t));
  }

  ret = select(nfds, readfds, writefds, exceptfds, &timeout);

  if (readfdsp != 0 && readfds != NULL) {
    emscripten_set_value(readfds, readfdsp, 0, sizeof(uint64_t));
  }
  if (writefdsp != 0 && writefds != NULL) {
    emscripten_set_value(writefds, writefdsp, 0, sizeof(uint64_t));
  }
  if (exceptfdsp != 0 && exceptfds != NULL) {
    emscripten_set_value(exceptfds, exceptfdsp, 0, sizeof(uint64_t));
  }
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 142] %u, %u\n", which, varargs);
#endif
  return check_ret(ret);
}

// __syscall144: msync

// readv
// Status: DONE.
uint32_t
emscripten____syscall145(uint32_t which, uint32_t varargs)
{
  assert(which == 145);
#if 1
  long rret = 0;
  uint32_t fd = emscripten_get(&varargs);
  uint32_t iovp = emscripten_get(&varargs);
  uint32_t iovcnt = emscripten_get(&varargs);
  struct iovec* iov = NULL;
  iov = malloc(sizeof(struct iovec) * iovcnt);
  if (iov == NULL)
    goto error;
  init_iov(iov, iovp, iovcnt);
  rret = sys_readv(fd, iov, iovcnt);
  if (rret >= 0) {
    write_iov(iovp, iovcnt, iov);
  }
  free(iov);
error:
#endif
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 145] readv(%u, %u, %u)\n", fd, iovp, iovcnt);
#endif
  return check_ret(rret);
  // return 0;
}

// writev
// Status: DONE.
uint32_t
emscripten____syscall146(uint32_t which, uint32_t varargs)
{
  assert(which == 146);
  long rret = 0;
  uint32_t fd = emscripten_get(&varargs);
  uint32_t iovp = emscripten_get(&varargs);
  uint32_t iovcnt = emscripten_get(&varargs);
  struct iovec* iov = NULL;

  iov = malloc(sizeof(struct iovec) * iovcnt);
  if (iov == NULL)
    goto error;
  read_iov(iov, iovp, iovcnt);
  rret = sys_writev(fd, iov, iovcnt);
  if (rret)
    goto error;
  free(iov);
error:
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 146] writev %u, %u\n", which, varargs);
#endif

  // dump_ssa();
  // i = rret / 0;
  // dump_ssa();
  // dump_exit_info();

  return check_ret(rret);
}

// chmod
// Status: TEST-NEDDED.
uint32_t
emscripten____syscall15(uint32_t which, uint32_t varargs)
{
  assert(which == 15);
  char* path = emscripten_get_string(&varargs);
  assert(path != NULL);
  uint32_t mode = emscripten_get(&varargs);

  chmod(path, mode);
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 15] %u, %u\n", which, varargs);
#endif
  return 0;
}

// __syscall150: mlock
// __syscall151: munlock
// __syscall152: mlockall
// __syscall153: munlockall

// __syscall16: lchown

// __syscall163: mremap

// poll
// Status: DONE.
uint32_t
emscripten____syscall168(uint32_t which, uint32_t varargs)
{
  assert(which == 168);
  uint32_t fdsp = emscripten_get(&varargs);
  uint32_t nfds = emscripten_get(&varargs);
  uint32_t timeout = emscripten_get(&varargs);
  struct pollfd* fds;
  uint32_t i;
  long ret;

  fds = malloc(nfds * sizeof(struct pollfd));
  for (i = 0; i < nfds; i++) {
    struct pollfd* fd = &fds[i];
    size_t offset = fdsp + i * sizeof(struct pollfd);
    emscripten_get_value((void*)&fd->fd, offset, 0, sizeof(uint32_t));
    emscripten_get_value((void*)&fd->events, offset, 4, sizeof(uint16_t));
    emscripten_get_value((void*)&fd->revents, offset, 6, sizeof(uint16_t));
  }

  ret = poll(fds, (nfds_t)nfds, (int)timeout);

  for (i = 0; i < nfds; i++) {
    struct pollfd* fd = &fds[i];
    size_t offset = fdsp + i * sizeof(struct pollfd);
    emscripten_set_value((void*)&fd->fd, offset, 0, sizeof(uint32_t));
    emscripten_set_value((void*)&fd->events, offset, 4, sizeof(uint16_t));
    emscripten_set_value((void*)&fd->revents, offset, 6, sizeof(uint16_t));
  }
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 168] %u, %u\n", which, varargs);
#endif
  return check_ret(ret);
}

// __syscall17: SYS_break

// __syscall178: rt_sigqueueinfo

// __syscall18: SYS_oldstat

// pread64
uint32_t
emscripten____syscall180(uint32_t which, uint32_t varargs)
{
  assert(which == 180);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 180] %u, %u\n", which, varargs);
#endif
  return 0;
}

// pwrite64
uint32_t
emscripten____syscall181(uint32_t which, uint32_t varargs)
{
  assert(which == 181);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 181] %u, %u\n", which, varargs);
#endif
  return 0;
}

// __syscall183: getcwd
uint32_t
emscripten____syscall183(uint32_t which, uint32_t varargs)
{
  assert(which == 183);
  uint32_t bufp = emscripten_get(&varargs);
  uint32_t size = emscripten_get(&varargs);
  char* buf = malloc(size);
  char* ret;
  assert(size != 0 && buf != NULL);
  ret = getcwd(buf, size);
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 183] getcwd(%u, %u), ret: %s\n", bufp, size, ret);
#endif
  if (ret != NULL) {
    emscripten_set_value(buf, bufp, 0, size);
    free(buf);
    return bufp;
  }
  return 0;
}

// __syscall19: lseek

// ugetrlimit
// Status: TEST-NEEDED.
uint32_t
emscripten____syscall191(uint32_t which, uint32_t varargs)
{
  assert(which == 191);
  uint32_t resource = emscripten_get(&varargs);
  uint32_t rlim = emscripten_get(&varargs);
  const int unlimit = -1;
  (void)resource;
  emscripten_set_value((void*)&unlimit, rlim, 0, sizeof(uint32_t));
  emscripten_set_value((void*)&unlimit, rlim, 4, sizeof(uint32_t));
  emscripten_set_value((void*)&unlimit, rlim, 8, sizeof(uint32_t));
  emscripten_set_value((void*)&unlimit, rlim, 12, sizeof(uint32_t));
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 191] %u, %u\n", which, varargs);
#endif
  return 0;
}

// mmap2
uint32_t
emscripten____syscall192(uint32_t which, uint32_t varargs)
{
  assert(which == 192);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 192] %u, %u\n", which, varargs);
#endif
  return 0;
}

// __syscall193: truncate64

// ftruncate64
// Status: TEST-NEEDED.
uint32_t
emscripten____syscall194(uint32_t which, uint32_t varargs)
{
  assert(which == 194);
  uint32_t fd = emscripten_get(&varargs);
  uint32_t length = emscripten_get(&varargs);
  ftruncate(fd, length);
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 194] %u, %u\n", which, varargs);
#endif
  return 0;
}

// stat64
// Status: DONE.
// TODO: Check definitation of struct stat.
uint32_t
emscripten____syscall195(uint32_t which, uint32_t varargs)
{
  assert(which == 195);
  char* path = emscripten_get_string(&varargs);
  uint32_t bufp = emscripten_get(&varargs);
  struct stat statbuf;
  long ret;

  memset(&statbuf, 0, sizeof(struct stat));
  ret = stat(path, &statbuf);

#if 0
  sgxwasm_log(
    "[195]\nst_dev: %u\nst_ino: %llu\nst_nlink: %u\nst_mode: %u\nst_uid: %u\nst_gid: %u\nst_rdev: %d\n"
    "st_size: %lld\nst_blksize: %d\nst_blocks: %lld\n",
    statbuf.st_dev,
    statbuf.st_ino,
    statbuf.st_nlink,
    statbuf.st_mode,
    statbuf.st_uid,
    statbuf.st_gid,
    statbuf.st_rdev,
    statbuf.st_size,
    statbuf.st_blksize,
    statbuf.st_blocks);
  sgxwasm_log(
    "st_atim: %lu\nst_atime_nsec: "
    "%lu\nst_mtim: %lu\nst_mtime_nsec: %lu\nst_ctim: %lu\nst_ctime_nsec: %lu\n",
    statbuf.st_atimespec.tv_sec,
    statbuf.st_atimespec.tv_nsec,
    statbuf.st_mtimespec.tv_sec,
    statbuf.st_mtimespec.tv_nsec,
    statbuf.st_ctimespec.tv_sec,
    statbuf.st_ctimespec.tv_nsec);
#endif
#if 0
#if __linux__
  sgxwasm_log(
    "[195]\nst_dev: %lu\nst_ino: %lu\nst_nlink: %lu\nst_mode: %lu\nst_uid: %lu\nst_gid: %lu\nst_rdev: %lu\n"
    "st_size: %lu\nst_blksize: %lu\nst_blocks: %lu\n",
    (uint64_t)&statbuf.st_dev - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_ino - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_nlink - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_mode - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_uid - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_gid - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_rdev - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_size - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_blksize - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_blocks - (uint64_t)&statbuf);
#else
  sgxwasm_log(
    "[195]\nst_dev: %llu\nst_ino: %llu\nst_nlink: %llu\nst_mode: %llu\nst_uid: %llu\nst_gid: %llu\nst_rdev: %llu\n"
    "st_size: %llu\nst_blksize: %llu\nst_blocks: %llu\n",
    (uint64_t)&statbuf.st_dev - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_ino - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_nlink - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_mode - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_uid - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_gid - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_rdev - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_size - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_blksize - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_blocks - (uint64_t)&statbuf);
#endif
#if __linux__
  sgxwasm_log(
    "st_atim: %lu\nst_atime_nsec: "
    "%lu\nst_mtim: %lu\nst_mtime_nsec: %lu\nst_ctim: %lu\nst_ctime_nsec: %lu\n",
    (uint64_t)&statbuf.st_atim.tv_sec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_atim.tv_nsec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_mtim.tv_sec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_mtim.tv_nsec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_ctim.tv_sec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_ctim.tv_nsec - (uint64_t)&statbuf);
#else
  sgxwasm_log(
    "st_atim: %llu\nst_atime_nsec: "
    "%llu\nst_mtim: %llu\nst_mtime_nsec: %llu\nst_ctim: %llu\nst_ctime_nsec: %llu\n",
    (uint64_t)&statbuf.st_atimespec.tv_sec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_atimespec.tv_nsec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_mtimespec.tv_sec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_mtimespec.tv_nsec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_ctimespec.tv_sec - (uint64_t)&statbuf,
    (uint64_t)&statbuf.st_ctimespec.tv_nsec - (uint64_t)&statbuf);
#endif
#endif

  write_stat(bufp, &statbuf);

#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 195] stat(%s), return: %ld\n", path, ret);
#endif
  return check_ret(ret);
}

// lstat64
// Status: DONE.
uint32_t
emscripten____syscall196(uint32_t which, uint32_t varargs)
{
  assert(which == 196);
  char* path = emscripten_get_string(&varargs);
  uint32_t bufp = emscripten_get(&varargs);
  struct stat statbuf;
  long ret;

  memset(&statbuf, 0, sizeof(struct stat));
  ret = lstat(path, &statbuf);
  write_stat(bufp, &statbuf);
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 196] %u, %u\n", which, varargs);
#endif
  return check_ret(ret);
}

// fstat64
// Status: DONE.
uint32_t
emscripten____syscall197(uint32_t which, uint32_t varargs)
{
  assert(which == 197);
  uint32_t fd = emscripten_get(&varargs);
  uint32_t bufp = emscripten_get(&varargs);
  struct stat statbuf;
  long ret;

  memset(&statbuf, 0, sizeof(struct stat));
  ret = fstat(fd, &statbuf);
  write_stat(bufp, &statbuf);
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 197] fstat - fd: %u, bufp: %u, return: %ld\n", fd, bufp,
              ret);
#endif
  return check_ret(ret);
}

// __syscall198: lchown32

// getuid32
// Status: DONE.
uint32_t
emscripten____syscall199(uint32_t which, uint32_t varargs)
{
  assert(which == 199);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 199] getuid %u, %u\n", which, varargs);
#endif
  return 0;
}

// __syscall2: fork

// getpid
// Status: DONE.
uint32_t
emscripten____syscall20(uint32_t which, uint32_t varargs)
{
  assert(which == 20);
  uint32_t ret = getpid();
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 20] getpid, ret: %u\n", ret);
#endif
  return ret;
}

// getgid
// Status: DONE.
uint32_t
emscripten____syscall200(uint32_t which, uint32_t varargs)
{
  assert(which == 200);
  uint32_t ret = getgid();
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 200] getgid, return: %u\n", ret);
#endif
  return ret;
}
// geteuid32
// Status: DONE.
uint32_t
emscripten____syscall201(uint32_t which, uint32_t varargs)
{
  assert(which == 201);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 201] %u, %u\n", which, varargs);
#endif
  return geteuid();
}

// getegid
// Status: DONE.
uint32_t
emscripten____syscall202(uint32_t which, uint32_t varargs)
{
  assert(which == 202);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 202] %u, %u\n", which, varargs);
#endif
  return getegid();
}

// __syscall207: fchown32

// chown32
uint32_t
emscripten____syscall212(uint32_t which, uint32_t varargs)
{
  assert(which == 212);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 212] %u, %u\n", which, varargs);
#endif
  return 0;
}

// __syscall203: setreuid32
// __syscall204: setregid32
// __syscall213: setuid32
// __syscall214: setgid32

// __syscall205: getgroups32

// __syscall208: setresuid32
// __syscall210: setresgid32

// __syscall209: getresuid
// __syscall211: getresgid32

// __syscall218: mincore
// __syscall219: madvise

// getdents64
uint32_t
emscripten____syscall220(uint32_t which, uint32_t varargs)
{
  assert(which == 220);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 220] %u, %u\n", which, varargs);
#endif
  return 0;
}

enum
{
  EM_F_GETFD = 1,
  EM_F_SETFD = 2,
  EM_F_GETFL = 3,
  EM_F_SETFL = 4,
  EM_F_GETLK = 12,
  EM_F_SETLK = 13
};

// fcntl64
uint32_t
emscripten____syscall221(uint32_t which, uint32_t varargs)
{
  assert(which == 221);
  uint32_t fd = emscripten_get(&varargs);
  uint32_t cmd = emscripten_get(&varargs);
  int ret;

  switch (cmd) {
    case EM_F_SETFD: {
      uint32_t arg = emscripten_get(&varargs);
      ret = fcntl(fd, F_SETFD, arg);
#if DEBUG_EM_CALLS
      sgxwasm_log("[syscall 221] fcntl F_SETFD fd: %u, arg: %u, return: %d\n",
                  fd, arg, ret);
#endif
      break;
    }
    case EM_F_GETFD: {
      ret = fcntl(fd, F_GETFD);
#if DEBUG_EM_CALLS
      sgxwasm_log("[syscall 221] fcntl F_GETFD fd: %u, return: %d\n", fd, ret);
#endif
      break;
    }
    case EM_F_SETFL: {
      uint32_t arg = emscripten_get(&varargs) ^ 0x8000;
      ret = fcntl(fd, F_SETFL, arg);
// emscripten_set_value(&rv, arg, 0, sizeof(uint32_t));
#if DEBUG_EM_CALLS
      sgxwasm_log("[syscall 221] fcntl F_SETFL fd: %u, arg: %u, return: %d\n",
                  fd, arg, ret);
#endif
      break;
    }
    case EM_F_GETFL: {
      ret = fcntl(fd, F_GETFL);
#if DEBUG_EM_CALLS
      sgxwasm_log("[syscall 221] fcntl F_GETFL fd: %u, return: %d\n", fd, ret);
#endif
      break;
    }
    case EM_F_SETLK: { // XXX: NOT SUPPORTED
      uint32_t arg = emscripten_get(&varargs);
      struct flock lock;
      //read_flock(&lock, arg);
      //ret = fcntl(fd, F_SETLK, &lock);
      (void) arg;
      (void) lock;
      ret = 0;
#if DEBUG_EM_CALLS
      sgxwasm_log("[syscall 221] fcntl flock: type %u, whence %u, start %zu, len %zu\n",
                  lock.l_type, lock.l_whence, lock.l_start, lock.l_len);
      sgxwasm_log("[syscall 221] fcntl F_SETLK fd: %u, arg: %u, return: %d\n",
                  fd, arg, ret);
#endif
      break;
    }
    case EM_F_GETLK: { // XXX: TEST-NEEDED
      uint32_t arg = emscripten_get(&varargs);
      struct flock lock;
      const uint32_t rv = F_ULOCK;
      ret = fcntl(fd, F_GETLK, &lock);
      emscripten_set_value((void*)&rv, arg, 0, sizeof(uint16_t));
#if DEBUG_EM_CALLS
      sgxwasm_log("[syscall 221] fcntl F_GETLK fd: %u, arg: %u, return: %d\n",
                  fd, arg, ret);
#endif
      break;
    }
    default: {
      ret = 0;
      sgxwasm_log("Unsupported cmd: %d\n", cmd);
    }
  }
  return (uint32_t)ret;
}

// __syscall265: clock_nanosleep

// statfs64
uint32_t
emscripten____syscall268(uint32_t which, uint32_t varargs)
{
  assert(which == 268);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 268] %u, %u\n", which, varargs);
#endif
  return 0;
}

// __syscall269: fstatfs64

// fadvise64_64
uint32_t
emscripten____syscall272(uint32_t which, uint32_t varargs)
{
  assert(which == 272);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 272] %u, %u\n", which, varargs);
#endif
  return 0;
}

// openat
uint32_t
emscripten____syscall295(uint32_t which, uint32_t varargs)
{
  assert(which == 295);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 295] %u, %u\n", which, varargs);
#endif
  return 0;
}

// __syscall296: mkdirat
// __syscall297: mknodat
// __syscall298: fchownat
// __syscall299: futimesat

// read
// Status: DONE.
uint32_t
emscripten____syscall3(uint32_t which, uint32_t varargs)
{
  assert(which == 3);
  long ret;
  uint32_t fd = emscripten_get(&varargs);
  uint32_t bufp = emscripten_get(&varargs);
  uint32_t count = emscripten_get(&varargs);
  char* buf;

#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 3] read fd: %u, bufp: %u, count: %u\n", fd, bufp,
              count);
#endif

  buf = calloc(1, count);
  ret = sys_read(fd, (void*)buf, count);
  if (buf == NULL) {
    return check_ret(ret);
  }
  emscripten_set_value(buf, bufp, 0, count);
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 3] read -\n%s (%u), ret: %ld\n", buf, count, ret);
#endif

  if (buf != NULL) {
    free(buf);
  }
  return check_ret(ret);
}

// fstatat64
uint32_t
emscripten____syscall300(uint32_t which, uint32_t varargs)
{
  assert(which == 300);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 300] %u, %u\n", which, varargs);
#endif
  return 0;
}

// __syscall301: unlinkat
// __syscall302: renameat
// __syscall303: linkat
// __syscall304: symlinkat
// __syscall305: readlinkat
// __syscall306: fchmodat
// __syscall307: faccessat
// __syscall308: pselect

// __syscall320: utimensat
// __syscall324: fallocate

uint32_t
emscripten____syscall33(uint32_t which, uint32_t varargs)
{
  assert(which == 33);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 33] %u, %u\n", which, varargs);
#endif
  return 0;
}

// __syscall330: dup3
// __syscall331: pipe2
// __syscall333: preaedv

// pwritev
uint32_t
emscripten____syscall334(uint32_t which, uint32_t varargs)
{
  assert(which == 334);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 334] %u, %u\n", which, varargs);
#endif
  return 0;
}

// __syscall337: recvmmsg

// prlimit64
uint32_t
emscripten____syscall340(uint32_t which, uint32_t varargs)
{
  assert(which == 340);
  uint32_t pid = emscripten_get(&varargs);
  uint32_t resource = emscripten_get(&varargs);
  uint32_t new_limit = emscripten_get(&varargs);
  uint32_t old_limit = emscripten_get(&varargs);
  const int unlimit = -1;
  if (old_limit) {
    emscripten_set_value((void*)&unlimit, old_limit, 0, sizeof(uint32_t));
    emscripten_set_value((void*)&unlimit, old_limit, 4, sizeof(uint32_t));
    emscripten_set_value((void*)&unlimit, old_limit, 8, sizeof(uint32_t));
    emscripten_set_value((void*)&unlimit, old_limit, 12, sizeof(uint32_t));
  }
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 340] prlimit %u, %u\n", which, varargs);
#endif
  return 0;
}

// __syscall345: sendmmsg

// rename
uint32_t
emscripten____syscall38(uint32_t which, uint32_t varargs)
{
  assert(which == 38);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 38] %u, %u\n", which, varargs);
#endif
  return 0;
}

// mkdir
uint32_t
emscripten____syscall39(uint32_t which, uint32_t varargs)
{
  assert(which == 39);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 39] %u, %u\n", which, varargs);
#endif
  return 0;
}

// write
// Status: TEST-NEEDED.
uint32_t
emscripten____syscall4(uint32_t which, uint32_t varargs)
{
  assert(which == 4);
  long ret;
  uint32_t fd = emscripten_get(&varargs);
  uint32_t bufp = emscripten_get(&varargs);
  uint32_t count = emscripten_get(&varargs);
  char* buf;

  buf = calloc(1, count);
  emscripten_get_value(buf, bufp, 0, count);

  ret = sys_write(fd, (void*)buf, count);
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 4] write(%u, %u, %u), return: %u\n", fd, bufp, count,
              ret);
#endif
  return check_ret(ret);
}

uint32_t
emscripten____syscall40(uint32_t which, uint32_t varargs)
{
  assert(which == 40);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 40] %u, %u\n", which, varargs);
#endif
  return 0;
}

// getdents
uint32_t
emscripten____syscall41(uint32_t which, uint32_t varargs)
{
  assert(which == 41);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 41] %u, %u\n", which, varargs);
#endif
  return 0;
}

// pipe
uint32_t
emscripten____syscall42(uint32_t which, uint32_t varargs)
{
  assert(which == 42);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 42] %u, %u\n", which, varargs);
#endif
  return 0;
}

// open
uint32_t
emscripten____syscall5(uint32_t which, uint32_t varargs)
{
  assert(which == 5);
  int fd;
  char* pathname = emscripten_get_string(&varargs);
  assert(pathname != NULL);
  uint32_t flags = emscripten_get(&varargs);
  uint32_t mode = emscripten_get(&varargs);

  fd = sys_open(pathname, flags, mode);
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 5] open(%s, %d, %u) ret: %d\n", pathname, flags, mode,
              fd);
#endif
  return fd;
}

// ioctl
uint32_t
emscripten____syscall54(uint32_t which, uint32_t varargs)
{
  assert(which == 54);
  uint32_t fd = emscripten_get(&varargs);
  uint32_t request = emscripten_get(&varargs);
  uint32_t ret = 0;

  switch (request) {
    case 21531: { // FIONREAD
      uint32_t argp = emscripten_get(&varargs);
      int arg;
      ret = ioctl(fd, request, &arg);
      if (argp != 0) {
        emscripten_set_value(&arg, argp, 0, sizeof(int));
      }
#if DEBUG_EM_CALLS
      sgxwasm_log("[syscall 54] ioctl: %u, FIONREAD, %u, %d, ret: %d\n", fd,
                  argp, arg, ret);
#endif
      break;
    }
    default: {
#if DEBUG_EM_CALLS
      sgxwasm_log("[syscall 54] ioctl %u, %u\n", fd, request);
#endif
      break;
    }
  }

#if DEBUG_EM_CALLS
// sgxwasm_log("[syscall 54] ioctl %u, %x\n", fd, request);
#endif
  return ret;
}

// close
// Status: DONE.
uint32_t
emscripten____syscall6(uint32_t which, uint32_t varargs)
{
  assert(which == 6);
  int fd = emscripten_get(&varargs);
  sys_close(fd);
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 6] close(%u)\n", fd);
#endif
  return 0;
}

// umask
uint32_t
emscripten____syscall60(uint32_t which, uint32_t varargs)
{
  assert(which == 60);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 60] %u, %u\n", which, varargs);
#endif
  return 0;
}

// dup2
uint32_t
emscripten____syscall63(uint32_t which, uint32_t varargs)
{
  assert(which == 63);
  uint32_t oldfd = emscripten_get(&varargs);
  uint32_t newfd = emscripten_get(&varargs);
  uint32_t ret;
  if (oldfd == newfd) {
    ret = newfd;
  } else {
    ret = dup2(oldfd, newfd);
  }
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 63] dup2 oldfd: %u, newfd: %u, ret: %u\n", oldfd, newfd,
              ret);
#endif
  return ret;
}

// getppid
uint32_t
emscripten____syscall64(uint32_t which, uint32_t varargs)
{
  assert(which == 64);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 64] %u, %u\n", which, varargs);
#endif
  return 0;
}

// setsid
uint32_t
emscripten____syscall66(uint32_t which, uint32_t varargs)
{
  assert(which == 66);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 66] %u, %u\n", which, varargs);
#endif
  return 0;
}

// setrlimit
uint32_t
emscripten____syscall75(uint32_t which, uint32_t varargs)
{
  assert(which == 75);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 75] %u, %u\n", which, varargs);
#endif
  return 0;
}

// readlink
uint32_t
emscripten____syscall85(uint32_t which, uint32_t varargs)
{
  assert(which == 85);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 85] %u, %u\n", which, varargs);
#endif
  return 0;
}

// munmap
uint32_t
emscripten____syscall91(uint32_t which, uint32_t varargs)
{
  assert(which == 91);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 91] %u, %u\n", which, varargs);
#endif
  return 0;
}

uint32_t
emscripten____syscall94(uint32_t which, uint32_t varargs)
{
  assert(which == 94);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 94] %u, %u\n", which, varargs);
#endif
  return 0;
}

// setpriority
uint32_t
emscripten____syscall97(uint32_t which, uint32_t varargs)
{
  assert(which == 97);
  (void)varargs;
#if DEBUG_EM_CALLS
  sgxwasm_log("[syscall 97] %u, %u\n", which, varargs);
#endif
  return 0;
}

// End of emscripten syscalls support.

void
emscripten____unlock(uint32_t arg)
{
  (void)arg;
#if DEBUG_EM_CALLS
  sgxwasm_log("[___unlock]: arg - %d\n", arg);
#endif
}

uint32_t
emscripten__clock()
{
  uint32_t ret;

  ret = (uint32_t)clock();
  return ret;
}

void
emscripten____wait(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
  sgxwasm_log("[___wait] arg1: %u, arg2: %u, arg3: %u, arg4: %u\n", arg1, arg2,
              arg3, arg4);
}

uint32_t
emscripten__emscripten_memcpy_big(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
  // sgxwasm_log("[___syscall6]: arg - %d, arg - %d, arg - %d\n", arg1, arg2,
  // arg3);
  // sgxwasm_log("[debug] 3-args: arg - %u, arg - %u, arg - %u\n", arg1, arg2,
  // arg3);
  (void)arg1;
  (void)arg2;
  (void)arg3;
  return 0;
}

void
emscripten__exit(uint32_t arg)
{
  sgxwasm_log("[_exit] arg: %u\n", arg);
}

void
emscripten___exit(uint32_t arg)
{
  sgxwasm_log("[__exit] arg: %u\n", arg);
}

uint32_t
emscripten__gettimeofday(uint32_t arg1, uint32_t arg2)
{
  //sgxwasm_log("[_gettimeofday] arg1: %u, arg2: %u\n", arg1, arg2);
  uint32_t ptr = emscripten_get(&arg1);
  struct timespec tp;
  tp.tv_sec = 0;
  tp.tv_nsec = 0;
  //clock_gettime(CLOCK_REALTIME, &tp);
  emscripten_set_value((void *)&tp.tv_sec, ptr, 0, sizeof(uint32_t));
  emscripten_set_value((void *)&tp.tv_nsec, ptr, 4, sizeof(uint32_t));
  //sgxwasm_log("[_gettimeofday] sec: %lu, nsec: %lu\n", tp.tv_sec, tp.tv_nsec);

  return 0;
}

double
emscripten__llvm_cos_f64(double x)
{
  // sgxwasm_log("[_llvm_cos_f64] arg - %lf\n", arg);
  return cos(x);
}

double
emscripten__llvm_sin_f64(double x)
{
  // sgxwasm_log("[_llvm_sin_f64] arg - %lf\n", arg);
  return sin(x);
}

void
emscripten_abort(uint32_t arg)
{
  sgxwasm_log("[abort]: arg - %d\n", arg);
}

void
emscripten__abort()
{
  sgxwasm_log("[_abort]\n");
}

uint32_t
emscripten__chroot(uint32_t arg)
{
  sgxwasm_log("[_chroot] arg: %d\n", arg);
  return 0;
}

uint32_t
emscripten__fork()
{
  sgxwasm_log("[_fork]\n");
  return 0;
}

void
emscripten__endgrent()
{
  sgxwasm_log("[_endgrent]\n");
}

uint32_t
emscripten__execl(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
  sgxwasm_log("[_execl] arg1: %u, arg2: %u, arg3: %u\n", arg1, arg2, arg3);

  return 0;
}

void
emscripten____buildEnvironment(uint32_t arg)
{
  sgxwasm_log("[___buildEnvironment]: arg - %d\n", arg);
}

uint32_t
emscripten__clock_gettime(uint32_t clk_id, uint32_t tp)
{
  sgxwasm_log("[__clock_gettime] arg1: %u, arg2: %u\n", clk_id, tp);
  return 0;
}

uint32_t
emscripten____clock_gettime(uint32_t clk_id, uint32_t tp)
{
  sgxwasm_log("[___clock_gettime] arg1: %u, arg2: %u\n", clk_id, tp);
  return 0;
}

double
emscripten_pow(double x, double y)
{
  // sgxwasm_log("[pow] arg1 - %lf, arg2 - %lf\n", x, y);
  return pow(x, y);
}

double
emscripten_exp(double x)
{
  // sgxwasm_log("[pow] arg - %lf\n", x);
  return exp(x);
}

uint32_t
emscripten__gai_strerror(uint32_t arg)
{
  sgxwasm_log("[_gai_strerror]: arg - %d\n", arg);
  return 0;
}

uint32_t
emscripten__getaddrinfo(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                        uint32_t arg4)
{
  sgxwasm_log("[_getaddrinfo] arg1: %u, arg2: %u, arg3: %u, arg4: %u\n", arg1,
              arg2, arg3, arg4);
  return 0;
}

uint32_t
emscripten__getenv(uint32_t arg)
{
  // sgxwasm_log("[_getenv]: arg - %d\n", arg);
  return 0;
}

uint32_t
emscripten__getgrent()
{
  sgxwasm_log("[_getgrent]\n");
  return 0;
}

uint32_t
emscripten__getgrgid(uint32_t arg)
{
  sgxwasm_log("[_getgrgid]: arg - %d\n", arg);
  return 0;
}

uint32_t
emscripten__getgrnam(uint32_t arg)
{
  sgxwasm_log("[_getgrnam]: arg - %d\n", arg);
  return 0;
}

uint32_t
emscripten__gethostbyaddr(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
  sgxwasm_log("[_gethostbyaddr] arg1: %u, arg2: %u, arg3: %u\n", arg1, arg2,
              arg3);

  return 0;
}

uint32_t
emscripten__getpwnam(uint32_t arg)
{
  sgxwasm_log("[_getpwnam]: arg - %d\n", arg);
  return 0;
}

uint32_t
emscripten__inet_addr(uint32_t cp)
{
  char* str = emscripten_get_string_at(cp);
  uint32_t addr;
  addr = inet_addr(str);
#if DEBUG_EM_CALLS
  sgxwasm_log("[_inet_addr]: str %s, ret: %u\n", str, addr);
#endif
  return addr;
}

uint32_t
emscripten__initgroups(uint32_t arg1, uint32_t arg2)
{
  sgxwasm_log("[_initgroups] arg1: %u, arg2: %u\n", arg1, arg2);

  return 0;
}

void
emscripten__llvm_stackrestore(uint32_t arg)
{
  (void)arg;
  // sgxwasm_log("[_llvm_stackrestore]: arg - %d\n", arg);
}

uint32_t
emscripten__llvm_stacksave()
{
  // sgxwasm_log("[_llvm_stacksave]\n");
  return 0;
}

static void
read_tm(uint32_t tmp, struct tm* tmptr)
{
  emscripten_get_value((void*)&tmptr->tm_sec, tmp, 0, sizeof(uint32_t));
  emscripten_get_value((void*)&tmptr->tm_min, tmp, 4, sizeof(uint32_t));
  emscripten_get_value((void*)&tmptr->tm_hour, tmp, 8, sizeof(uint32_t));
  emscripten_get_value((void*)&tmptr->tm_mday, tmp, 12, sizeof(uint32_t));
  emscripten_get_value((void*)&tmptr->tm_mon, tmp, 16, sizeof(uint32_t));
  emscripten_get_value((void*)&tmptr->tm_year, tmp, 20, sizeof(uint32_t));
  emscripten_get_value((void*)&tmptr->tm_wday, tmp, 24, sizeof(uint32_t));
  emscripten_get_value((void*)&tmptr->tm_yday, tmp, 28, sizeof(uint32_t));
  emscripten_get_value((void*)&tmptr->tm_gmtoff, tmp, 32, sizeof(uint32_t));
  emscripten_get_value((void*)&tmptr->tm_isdst, tmp, 36, sizeof(uint32_t));
  emscripten_get_value((void*)&tmptr->tm_zone, tmp, 40, sizeof(uint32_t));
}

static void
write_tm(uint32_t tmp, struct tm* tmptr)
{
  emscripten_set_value((void*)&tmptr->tm_sec, tmp, 0, sizeof(uint32_t));
  emscripten_set_value((void*)&tmptr->tm_min, tmp, 4, sizeof(uint32_t));
  emscripten_set_value((void*)&tmptr->tm_hour, tmp, 8, sizeof(uint32_t));
  emscripten_set_value((void*)&tmptr->tm_mday, tmp, 12, sizeof(uint32_t));
  emscripten_set_value((void*)&tmptr->tm_mon, tmp, 16, sizeof(uint32_t));
  emscripten_set_value((void*)&tmptr->tm_year, tmp, 20, sizeof(uint32_t));
  emscripten_set_value((void*)&tmptr->tm_wday, tmp, 24, sizeof(uint32_t));
  emscripten_set_value((void*)&tmptr->tm_yday, tmp, 28, sizeof(uint32_t));
  emscripten_set_value((void*)&tmptr->tm_gmtoff, tmp, 32, sizeof(uint32_t));
  emscripten_set_value((void*)&tmptr->tm_isdst, tmp, 36, sizeof(uint32_t));
  emscripten_set_value((void*)&tmptr->tm_zone, tmp, 40, sizeof(uint32_t));
}

uint32_t
emscripten__localtime(uint32_t arg)
{
  assert(emscripten_stack_alloc != NULL);
  uint32_t tmp_tm = emscripten_stack_alloc(sizeof(struct tm));
  uint32_t timer = emscripten_get(&arg);
  time_t date = timer * 1000;
  struct tm* tmptr = localtime(&date);
  // sgxwasm_log("[_localtime] date: %ld\n", date);
  write_tm(tmp_tm, tmptr);
  return tmp_tm;
}

uint32_t
emscripten__localtime_r(uint32_t arg1, uint32_t arg2)
{
  sgxwasm_log("[_localtime_r] arg1: %u, arg2: %u\n", arg1, arg2);

  return 0;
}

uint32_t
emscripten__nanosleep(uint32_t arg1, uint32_t arg2)
{
  sgxwasm_log("[_nanosleep] arg1: %u, arg2: %u\n", arg1, arg2);

  return 0;
}

uint32_t
emscripten__gmtime(uint32_t arg)
{
  assert(emscripten_stack_alloc != NULL);
  uint32_t tmp_tm = emscripten_stack_alloc(sizeof(struct tm));
  uint32_t timer = emscripten_get(&arg);
  time_t date = timer * 1000;
  struct tm* tmptr = localtime(&date);
#if DEBUG_EM_CALLS
  sgxwasm_log("[_gmtime] date: %ld\n", date);
#endif
  write_tm(tmp_tm, tmptr);
  return tmp_tm;
}

uint32_t
emscripten__gmtime_r(uint32_t arg1, uint32_t arg2)
{
  sgxwasm_log("[_gmtime_r] arg1: %u, arg2: %u\n", arg1, arg2);

  return 0;
}

uint32_t
emscripten__strftime(uint32_t s, uint32_t maxsize, uint32_t format, uint32_t tm)
{
  uint32_t ret = 0;
  struct tm date;
  char* f = emscripten_get_string_at(format);
  char* buf = malloc(maxsize);
  read_tm(tm, &date);

  ret = strftime(buf, maxsize, f, &date);
#if DEBUG_EM_CALLS
  sgxwasm_log("[_strftime] s: %u, maxsize: %u, format: %s, tm: %u, ret: %u\n",
              s, maxsize, f, tm, ret);
#endif

  if (ret != 0) {
    assert(s != 0 && ret <= maxsize);
    emscripten_set_value((void*)buf, s, 0, ret);
  }

  free(buf);
  return ret;
}

uint32_t
emscripten__mktime(uint32_t arg)
{
  sgxwasm_log("[_mktime]: arg - %d\n", arg);
  return 0;
}

uint32_t
emscripten__pathconf(uint32_t arg1, uint32_t arg2)
{
  sgxwasm_log("[_pathconf] arg1: %u, arg2: %u\n", arg1, arg2);

  return 0;
}

uint32_t
emscripten__prctl(uint32_t arg1, uint32_t arg2)
{
  sgxwasm_log("[_prctl] arg1: %u, arg2: %u\n", arg1, arg2);

  return 0;
}

uint32_t
emscripten__pthread_setcancelstate(uint32_t arg1, uint32_t arg2)
{
  // sgxwasm_log("[_pthread_setcancelstate] arg1: %u, arg2: %u\n", arg1, arg2);
  return 0;
}

void
emscripten__setgrent()
{
  sgxwasm_log("[_setgrent]\n");
}

uint32_t
emscripten__setgroups(uint32_t arg1, uint32_t arg2)
{
  sgxwasm_log("[_setgroups] arg1: %u, arg2: %u\n", arg1, arg2);

  return 0;
}

uint32_t
emscripten__signal(uint32_t arg1, uint32_t arg2)
{
  sgxwasm_log("[_signal] arg1: %u, arg2: %u\n", arg1, arg2);

  return 0;
}

uint32_t
emscripten__strptime(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
  sgxwasm_log("[_strptime] arg1: %u, arg2: %u, arg3: %u\n", arg1, arg2, arg3);

  return 0;
}

uint32_t
emscripten__time(uint32_t arg)
{
  uint32_t ret = (time(NULL) / 1000) | 0;
  (void)arg;
#if DEBUG_EM_CALLS
  sgxwasm_log("[_time]: ret - %u\n", ret);
#endif
  return ret;
}

uint32_t
emscripten__utimes(uint32_t arg1, uint32_t arg2)
{
  (void)arg1;
  (void)arg2;
#if DEBUG_EM_CALLS
  sgxwasm_log("[_utimes] arg1: %u, arg2: %u\n", arg1, arg2);
#endif
  return 0;
}

void
emscripten__tzset()
{
  sgxwasm_log("[_tzset]\n");
}

uint32_t
emscripten__waitpid(uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
  sgxwasm_log("[_waitpid] arg1: %u, arg2: %u, arg3: %u\n", arg1, arg2, arg3);

  return 0;
}
