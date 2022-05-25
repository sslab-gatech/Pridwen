#include <sgxwasm/sense.h>
#if __SGX__
#include "sgx_trts_exception.h"
#endif

#if __SGX__
// Hard-coded offset
static uint64_t sgxwasm_code_pointer = (int64_t)__SGXWASM_CODE_BASE - 0x4c80;
//#define SSA_OFFSET 0x3779000

// NOTE: Modify the offset if size of stack or heap is changed.
#define SSA_OFFSET 0x3879000

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
#else
static uint8_t tmp_exit_type;
#endif

int
check_tsx();

#if __SGX__
void
dump_ssa()
{
  uint64_t ssa = sgxwasm_code_pointer + SSA_OFFSET;
  printf("[dump_ssa]\n");
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

#if DEBUG_SYS_CONFIG
int
init_exception_handler(sgx_exception_info_t* info)
{
  if (info->exception_vector == 0) {
    info->cpu_context.rip += 2; // idiv = 2 bytes
  }
  return -1;
}
#endif
#endif

void
init_system_sensing(struct SystemConfig* config)
{
#if __SGX__
  uint64_t ssa = sgxwasm_code_pointer + SSA_OFFSET;

#if DEBUG_SYS_CONFIG
  dump_ssa();
  sgx_register_exception_handler(1, init_exception_handler);
  ret = ret / 0;
  sgx_unregister_exception_handler(init_exception_handler);
  dump_ssa();
#endif

  config->exittype_addr = sgxwasm_code_pointer + SSA_OFFSET + EXITINFO_OFFSET;
  // Initialize the exittype.
  *(uint8_t*)config->exittype_addr = VARYS_MAGIC;

  sgx_register_exception_handler(1, sensing_exception_handler);
#else
  config->exittype_addr = (uint64_t)&tmp_exit_type;
#endif
}

void
system_sensing(struct SystemConfig* config)
{
  // XXX: This is not working for some reason...
  int ret = check_tsx();
  //int ret = 1;
  printf("check_tsx: %d\n", ret);
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
    printf("exittype_addr: %lx (value: %u)\n",
           config->exittype_addr,
           *(uint8_t*)config->exittype_addr);
#else
    printf("exittype_addr: %llx (value: %u)\n",
           config->exittype_addr,
           *(uint8_t*)config->exittype_addr);
#endif
  }
  printf("tsx_support: %d\n", config->tsx_support);
#endif
}
