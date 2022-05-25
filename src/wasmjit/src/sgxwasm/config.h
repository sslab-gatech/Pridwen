#ifndef __SGXWASM__CONFIG_H__
#define __SGXWASM__CONFIG_H__

// Default code size is 32 MB
#define CodeSize 33554432
#define UnitSize 64

#ifndef SGXWASM_SPEC_TEST
#define SGXWASM_SPEC_TEST 0
#endif

#ifndef MEMORY_TRACE
#define MEMORY_TRACE 0
#endif

#ifndef DEBUG_INSTANTIATE
#define DEBUG_INSTANTIATE 0
#endif

#ifndef DEBUG_RELOCATE
#define DEBUG_RELOCATE 0
#endif

// Configurations for compilation.

// FIXME: Not working correcly now.
#ifndef __OPT_INIT_STACK__
#define __OPT_INIT_STACK__ 1
#endif

// Configurations for passes.

#ifndef __SGX__
#define __SGX__ 0
#endif

#ifndef __PASS__
#define __PASS__ 1
#endif

#ifndef __TEST__
#define __TEST__ 0
#endif

#ifndef __CFG__
#define __CFG__ 1
#endif

#ifndef __DEBUG_CFG__
#define __DEBUG_CFG__ 1
#endif

#ifndef __CASLR__
#define __CASLR__ 1
#endif

#ifndef __DEBUG_CASLR__
#define __DEBUG_CASLR__ 1
#endif

#ifndef FIX_SIZE_ASLR
#define FIX_SIZE_ASLR 1
#endif

#ifndef __TSGX__
#define __TSGX__ 0
#endif

#ifndef __DEBUG_TSGX__
#define __DEBUG_TSGX__ 0
#endif

#ifndef FIX_SIZE_UNIT
#define FIX_SIZE_UNIT 0
#endif

#ifndef __DEBUG_TSGX_SPLIT__
#define __DEBUG_TSGX_SPLIT__ 0
#endif

#ifndef TSX_SUPPORT
#define TSX_SUPPORT 1
#endif

#ifndef __ASLR__
#define __ASLR__ 0
#endif

#ifndef __DEBUG_ASLR__
#define __DEBUG_ASLR__ 0
#endif

#ifndef __DEBUG_PASS__
#define __DEBUG_PASS__ 0
#endif

#endif