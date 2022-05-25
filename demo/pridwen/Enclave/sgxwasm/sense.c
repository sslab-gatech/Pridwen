#include <sgxwasm/sense.h>
#if __SGX__
#include "sgx_trts_exception.h"
#endif

#if __SGX__
// Hard-coded offset
// Check the value of _binary_sgxwasm_code_start symbol.
/*#define OFFSET_TO_START 0x2a2a83
static uint64_t sgxwasm_code_pointer =
  (int64_t)__SGXWASM_CODE_BASE - OFFSET_TO_START;*/
static uint64_t sgxwasm_code_pointer = (int64_t)__SGXWASM_CODE_BASE;

// NOTE: Modify the offset if size of stack or heap is changed.
// Current HEAP:  0x5000000
// Current Stack: 0x40000
// Memory layout:
//    CODE SECTION    (r-xs)
//        ...
//  SGXWASM_CODE_BASE (rwxs)
//        HEAP        (rw-s)
//      guard page
//        STACK       (rw-s)
//      guard page
//         TCS        (rw-s)
//         SSA        (rw-s)
#if !__COLOC_TEST__
//#define SSA_OFFSET 0x7316000
#else
//#define SSA_OFFSET 0x731a000 /* Co-location test */
#endif
//#define SSA_OFFSET 0x7312000
//#define SSA_OFFSET 0x7313000 /* Large program */
//#define SSA_OFFSET 0x7314000
//#define SSA_OFFSET 0x7316000
//#define SSA_OFFSET 0x7318000
//#define SSA_OFFSET 0x731a000 /* Co-location test */
#define SSA_OFFSET 0x731b000

#define TCS_NUM 4
#define TCS_OFFSET 0x74000
static uint64_t ssa_offset[TCS_NUM];

/*
 EXITINFO {
    bit 0 - 7: vector
    bit 8 - 10: exit_type
    bit 11 - 30: reserved
    bit 31: valid
 }
 */
//#define EXITINFO_OFFSET 4072
// 4072 points to EXITINFO
#define EXITINFO_OFFSET 4073
#endif

static uint8_t tmp_exit_type;

int check_tsx();

#if __SGX__
void
dump_ssa(uint64_t offset)
{
  uint64_t ssa = sgxwasm_code_pointer + offset;
  printf("[dump_ssa] offset: %lx\n", offset);
  for (int i = 0; i < 8192; i += 8) {
    uint64_t bytes = *(uint64_t*)(ssa + i);
    printf("%lx (%d): %lx\n", ssa + i, i, bytes);
  }
}

int
sensing_exception_handler(sgx_exception_info_t* info)
{
  // info->cpu_context.rip += 3; // idiv = 2 or3 bytes
  return -1;
  // return 0;
}
#if !__HARDCODE_TCS__
static uint8_t expected_exittype = 0;
int
init_exception_handler(sgx_exception_info_t* info)
{
  if (info->exception_vector == 0) {
    info->cpu_context.rip += 2; // idiv = 2 bytes
    expected_exittype = info->exception_type;
  }
  return -1;
}
#endif
#endif

void
init_system_sensing(struct SystemConfig* config)
{
#if __SGX__ && __COLOC_TEST__
  sgxwasm_code_pointer &= ~0xffffff;

#if 0 // Only works when number of TCS is 1.
  uint64_t ssa = sgxwasm_code_pointer + SSA_OFFSET;

  config->exittype_addr = sgxwasm_code_pointer + SSA_OFFSET + EXITINFO_OFFSET;
   Initialize the exittype.
  *(uint8_t*)config->exittype_addr = VARYS_MAGIC;
#endif
  //printf("sgxwasm_base: %lx, target_base: 0x%lx\n",
  //       (int64_t)__SGXWASM_CODE_BASE, (int64_t)sgxwasm_code_pointer);

  // Hanlde the case of multiple TCS.
  for (int i = 0; i < TCS_NUM; i++) {
    ssa_offset[i] = sgxwasm_code_pointer + SSA_OFFSET + EXITINFO_OFFSET + TCS_OFFSET * i;
    *(uint8_t*)ssa_offset[i] = VARYS_MAGIC;
  }

#if DEBUG_SYS_CONFIG
  printf("sgxwasm_base: %lx, target_base: 0x%lx\n",
         (int64_t)__SGXWASM_CODE_BASE, (int64_t)sgxwasm_code_pointer);
  // XXX: Not working
  for (int i = 0; i < TCS_NUM; i++) {
    // dump_ssa(ssa_offset[i]);
  }
#endif
#if !__HARDCODE_TCS__
  sgx_register_exception_handler(1, init_exception_handler);
  int ret = ret / 0;
  sgx_unregister_exception_handler(init_exception_handler);
#endif
#if DEBUG_SYS_CONFIG
  for (int i = 0; i < TCS_NUM; i++) {
    // XXX: Not working
    // dump_ssa(ssa_offset[i]);
  }
  for (int i = 0; i < TCS_NUM; i++) {
    printf("tcs #%d (%lx): %u\n", i, ssa_offset[i], *(uint8_t*)ssa_offset[i]);
  }
#endif
#if !__HARDCODE_TCS__
  int found = 0;
  for (int i = 0; i < TCS_NUM; i++) {
    if (*(uint8_t*)ssa_offset[i] == expected_exittype) {
      config->exittype_addr = ssa_offset[i];
      *(uint8_t*)config->exittype_addr = VARYS_MAGIC;
      found = 1;
      break;
    }
  }
  assert(found);
#else
  config->exittype_addr = ssa_offset[3];
  *(uint8_t*)config->exittype_addr = VARYS_MAGIC;
#endif
  sgx_register_exception_handler(1, sensing_exception_handler);
#else
  config->exittype_addr = (uint64_t)&tmp_exit_type;
#endif
}

void
system_sensing(struct SystemConfig* config)
{
  int ret = check_tsx();
  // int ret = 1;
  config->tsx_support = ret;
}

void
finalize_system_sensing()
{
#if __SGX__
  sgx_unregister_exception_handler(sensing_exception_handler);
#endif
}

void
dump_system_config(struct SystemConfig* config)
{
#if __DEMO__
  printf("[PRIDWEN] Intel TSX: ");
  if (config->tsx_support) {
    printf("enabled");
  } else {
    printf("disabled");
  }
  printf("\n");
#else
  printf("[system config]\n");
  if (config->exittype_addr == 0) {
#if __linux__
    printf("exittype_addr: %lx\n", config->exittype_addr);
#else
    printf("exittype_addr: %llx\n", config->exittype_addr);

#endif
  } else {
#if __linux__
    printf("exittype_addr: %lx (value: %u)\n", config->exittype_addr,
           *(uint8_t*)config->exittype_addr);
#else
    printf("exittype_addr: %llx (value: %u)\n", config->exittype_addr,
           *(uint8_t*)config->exittype_addr);
#endif
  }
  printf("tsx_support: %d\n", config->tsx_support);
#endif
}
