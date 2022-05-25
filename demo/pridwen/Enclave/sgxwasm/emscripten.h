#ifndef __SGXWASM__EMSCRIPTEN_H__
#define __SGXWASM__EMSCRIPTEN_H__

#include <sgxwasm/runtime.h>
#include <sgxwasm/sys.h>
#include <sgxwasm/util.h>

struct EmscriptenGlobals
{
  uint32_t memoryBase;
  uint32_t tempDoublePtr;
  uint32_t DYNAMICTOP_PTR;
  uint32_t STACKTOP;
  uint32_t STACK_MAX;
};

struct EmscriptenImports
{
  struct Function* errno_location;
  struct Function* malloc;
  struct Function* free;
};

struct EmscriptenContext
{
  struct EmscriptenGlobals globals;
  struct EmscriptenImports imports;
  size_t total_memory;
  uint64_t mem_base;
  size_t max_size;
  char** environ;
  int buildEnvironmentCalled;
};

#define SGXWASM_TRAP_OFFSET 0x100
#define SGXWASM_IS_TRAP_ERROR(ret) ((ret) >= SGXWASM_TRAP_OFFSET)
#define SGXWASM_DECODE_TRAP_ERROR(ret) ((ret)-SGXWASM_TRAP_OFFSET)
#define SGXWASM_ENCODE_TRAP_ERROR(ret) ((ret) + SGXWASM_TRAP_OFFSET)

void
emscripten_context_init(struct EmscriptenContext*);
struct EmscriptenGlobals*
emscripten_context_derive_memory_globals(struct EmscriptenContext*, uint32_t);

int
emscripten_context_set_imports(struct EmscriptenContext*,
                               struct Function*,
                               struct Function*,
                               struct Function*,
                               char**);
struct EmscriptenContext*
emscripten_get_context(struct Module*);
void
emscripten_cleanup(struct Module*);
void emscripten_setMemBase(uint64_t, uint64_t);
uint64_t
emscripten_getMemBase();
int
emscripten_setDynamicBase(struct EmscriptenContext*);
void
emscripten_setModuleRef(struct Module *);
int
emscripten_invoke_main(struct Function*, struct Function*, int, char**);
int
emscripten_build_environment(struct Function*);
void
emscripten_start_func();
void
memory_preloading();

#define CTYPE_VALTYPE_I32 uint32_t
#define CTYPE_VALTYPE_I64 uint64_t
#define CTYPE_VALTYPE_F32 float
#define CTYPE_VALTYPE_F64 double
#define CTYPE_VALTYPE_NULL void
#define CTYPE(val) CTYPE_##val

#define __PARAM(to, n, t) CTYPE(t)

#define DEFINE_WASM_FUNCTION(_name, _output, _n, ...)                          \
  CTYPE(_output)                                                               \
  emscripten_##_name(__KMAP(_n, __PARAM, ##__VA_ARGS__));

#define DEFINE_WASM_FUNCTIONS(V)                                               \
  V(enlargeMemory, VALTYPE_I32, 0)                                             \
  V(getTotalMemory, VALTYPE_I32, 0)                                            \
  V(abortOnCannotGrowMemory, VALTYPE_I32, 0)                                   \
  V(abortStackOverflow, VALTYPE_NULL, 1, VALTYPE_I32)                          \
  V(nullFunc_i, VALTYPE_NULL, 1, VALTYPE_I32)                                  \
  V(nullFunc_ii, VALTYPE_NULL, 1, VALTYPE_I32)                                 \
  V(nullFunc_iii, VALTYPE_NULL, 1, VALTYPE_I32)                                \
  V(nullFunc_iiii, VALTYPE_NULL, 1, VALTYPE_I32)                               \
  V(nullFunc_iiiii, VALTYPE_NULL, 1, VALTYPE_I32)                              \
  V(nullFunc_iiiiii, VALTYPE_NULL, 1, VALTYPE_I32)                             \
  V(nullFunc_iiiiiii, VALTYPE_NULL, 1, VALTYPE_I32)                            \
  V(nullFunc_iiiij, VALTYPE_NULL, 1, VALTYPE_I32)                              \
  V(nullFunc_iij, VALTYPE_NULL, 1, VALTYPE_I32)                                \
  V(nullFunc_iiji, VALTYPE_NULL, 1, VALTYPE_I32)                               \
  V(nullFunc_iijii, VALTYPE_NULL, 1, VALTYPE_I32)                              \
  V(nullFunc_v, VALTYPE_NULL, 1, VALTYPE_I32)                                  \
  V(nullFunc_vi, VALTYPE_NULL, 1, VALTYPE_I32)                                 \
  V(nullFunc_vii, VALTYPE_NULL, 1, VALTYPE_I32)                                \
  V(nullFunc_viii, VALTYPE_NULL, 1, VALTYPE_I32)                               \
  V(nullFunc_viiii, VALTYPE_NULL, 1, VALTYPE_I32)                              \
  V(nullFunc_viiiii, VALTYPE_NULL, 1, VALTYPE_I32)                             \
  V(nullFunc_viiiiii, VALTYPE_NULL, 1, VALTYPE_I32)                            \
  V(nullFunc_viiiiiii, VALTYPE_NULL, 1, VALTYPE_I32)                           \
  V(nullFunc_viiiij, VALTYPE_NULL, 1, VALTYPE_I32)                              \
  V(nullFunc_viij, VALTYPE_NULL, 1, VALTYPE_I32)                              \
  V(___assert_fail,                                                            \
    VALTYPE_NULL,                                                              \
    4,                                                                         \
    VALTYPE_I32,                                                               \
    VALTYPE_I32,                                                               \
    VALTYPE_I32,                                                               \
    VALTYPE_I32)                                                               \
  V(___lock, VALTYPE_NULL, 1, VALTYPE_I32)                                     \
  V(___setErrNo, VALTYPE_NULL, 1, VALTYPE_I32)                                 \
  V(___syscall10, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall102, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall118, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall12, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall122, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall140, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall142, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall145, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall146, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall15, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall168, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall180, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall181, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall183, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall191, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall192, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall194, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall195, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall196, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall197, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall199, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall20, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall200, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall201, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall202, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall212, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall220, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall221, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall268, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall272, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall295, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall3, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                     \
  V(___syscall300, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall33, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                     \
  V(___syscall334, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall340, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(___syscall38, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall39, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall4, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                     \
  V(___syscall40, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                     \
  V(___syscall41, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall42, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall5, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                     \
  V(___syscall54, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall6, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                     \
  V(___syscall60, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall63, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall64, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall66, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall75, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall85, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall91, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall94, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___syscall97, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(___unlock, VALTYPE_NULL, 1, VALTYPE_I32)                                   \
  V(___wait,                                                                   \
    VALTYPE_NULL,                                                              \
    4,                                                                         \
    VALTYPE_I32,                                                               \
    VALTYPE_I32,                                                               \
    VALTYPE_I32,                                                               \
    VALTYPE_I32)                                                               \
  V(_clock, VALTYPE_I32, 0)                                                    \
  V(_emscripten_memcpy_big,                                                    \
    VALTYPE_I32,                                                               \
    3,                                                                         \
    VALTYPE_I32,                                                               \
    VALTYPE_I32,                                                               \
    VALTYPE_I32)                                                               \
  V(_endgrent, VALTYPE_NULL, 0)                                                \
  V(_execl, VALTYPE_I32, 3, VALTYPE_I32, VALTYPE_I32, VALTYPE_I32)             \
  V(_fork, VALTYPE_I32, 0)                                                     \
  V(_exit, VALTYPE_NULL, 1, VALTYPE_I32)                                       \
  V(__exit, VALTYPE_NULL, 1, VALTYPE_I32)                                      \
  V(_gettimeofday, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                   \
  V(_clock_gettime, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                  \
  V(___clock_gettime, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                \
  V(___map_file, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                     \
  V(_llvm_cos_f64, VALTYPE_F64, 1, VALTYPE_F64)                                \
  V(_llvm_sin_f64, VALTYPE_F64, 1, VALTYPE_F64)                                \
  V(abort, VALTYPE_NULL, 1, VALTYPE_I32)                                       \
  V(_abort, VALTYPE_NULL, 0)                                                   \
  V(_chroot, VALTYPE_I32, 1, VALTYPE_I32)                                      \
  V(___buildEnvironment, VALTYPE_NULL, 1, VALTYPE_I32)                         \
  V(pow, VALTYPE_F64, 2, VALTYPE_F64, VALTYPE_F64)                             \
  V(exp, VALTYPE_F64, 1, VALTYPE_F64)                                          \
  V(_gai_strerror, VALTYPE_I32, 1, VALTYPE_I32)                                \
  V(_getaddrinfo,                                                              \
    VALTYPE_I32,                                                               \
    4,                                                                         \
    VALTYPE_I32,                                                               \
    VALTYPE_I32,                                                               \
    VALTYPE_I32,                                                               \
    VALTYPE_I32)                                                               \
  V(_getenv, VALTYPE_I32, 1, VALTYPE_I32)                                      \
  V(_getgrent, VALTYPE_I32, 0)                                                 \
  V(_getgrgid, VALTYPE_I32, 1, VALTYPE_I32)                                    \
  V(_getgrnam, VALTYPE_I32, 1, VALTYPE_I32)                                    \
  V(_gethostbyaddr, VALTYPE_I32, 3, VALTYPE_I32, VALTYPE_I32, VALTYPE_I32)     \
  V(_getpwnam, VALTYPE_I32, 1, VALTYPE_I32)                                    \
  V(_gmtime, VALTYPE_I32, 1, VALTYPE_I32)                                      \
  V(_gmtime_r, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                       \
  V(_inet_addr, VALTYPE_I32, 1, VALTYPE_I32)                                   \
  V(_initgroups, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                     \
  V(_inet_addr, VALTYPE_I32, 1, VALTYPE_I32)                                   \
  V(_initgroups, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                     \
  V(_llvm_stackrestore, VALTYPE_NULL, 1, VALTYPE_I32)                          \
  V(_llvm_stacksave, VALTYPE_I32, 0)                                           \
  V(_localtime, VALTYPE_I32, 1, VALTYPE_I32)                                   \
  V(_nanosleep, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                      \
  V(_localtime_r, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                    \
  V(_mktime, VALTYPE_I32, 1, VALTYPE_I32)                                      \
  V(_pathconf, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                       \
  V(_prctl, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                          \
  V(_pthread_setcancelstate, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)         \
  V(_setgrent, VALTYPE_NULL, 0)                                                \
  V(_setgroups, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                      \
  V(_signal, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                         \
  V(_strftime,                                                                 \
    VALTYPE_I32,                                                               \
    4,                                                                         \
    VALTYPE_I32,                                                               \
    VALTYPE_I32,                                                               \
    VALTYPE_I32,                                                               \
    VALTYPE_I32)                                                               \
  V(_strptime, VALTYPE_I32, 3, VALTYPE_I32, VALTYPE_I32, VALTYPE_I32)          \
  V(_time, VALTYPE_I32, 1, VALTYPE_I32)                                        \
  V(_utimes, VALTYPE_I32, 2, VALTYPE_I32, VALTYPE_I32)                         \
  V(_tzset, VALTYPE_NULL, 0)                                                   \
  V(_waitpid, VALTYPE_I32, 3, VALTYPE_I32, VALTYPE_I32, VALTYPE_I32)

DEFINE_WASM_FUNCTIONS(DEFINE_WASM_FUNCTION)

#undef DEFINE_WASM_FUNCTION
//#undef DEFINE_WASM_FUNCTIONS

#undef __PARAM
#undef CTYPE
#undef CTYPE_VALTYPE_I32
#undef CTYPE_VALTYPE_I64
#undef CTYPE_VALTYPE_F32
#undef CTYPE_VALTYPE_F64
#undef CTYPE_VALTYPE_NULL

#endif