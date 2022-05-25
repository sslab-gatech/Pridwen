#include <wasmjit/assembler.h>

size_t
dump_output(struct SizedBuffer* output, size_t start)
{
  size_t i;
  for (i = start; i < output->n_elts; i++) {
    uint8_t byte = (uint8_t)output->elts[i];
    if (byte < 16) {
      printf("0%x ", byte);
      continue;
    }
    printf("%x ", byte);
  }
  printf("\n");

  return i;
}

void
dump_fp(uint8_t* val, uint8_t byte)
{
  int i;
  printf("----dump_fp----\n");
  for (i = (int)byte - 1; i >= 0; i--) {
    uint8_t b = val[i];
    if (b == 0) {
      printf("00 ");
      continue;
    }
    printf("%x ", b);
  }
  printf("\n----------------\n");
}

void
dump_bit(reglist_t list)
{
  int i;
  reglist_t mask = 1 << 31;
  printf("dump_bit:");
  for (i = 0; i < 32; i++) {
    if (mask & list) {
      printf(" 1");
    } else {
      printf(" 0");
    }
    mask = mask >> 1;
  }
  printf("\n");
}

void test(int lv, int rv)
{
  if (lv == rv) {
    printf("pass\n");
  } else {
    printf("fail, expected: %d, result: %d\n", lv, rv);
  }
}

void
dump_output_to_file(struct SizedBuffer* output)
{
  FILE* f;
  f = fopen("code.bin", "wb");
  printf("write to file...\n");
  fwrite(output->elts, output->n_elts, 1, f);
  fclose(f);
}

int
main()
{
  struct SizedBuffer outputv = { 0, NULL };
  struct SizedBuffer* output = &outputv;
  size_t i;

  reglist_t used_registers = 0;
  uint32_t register_use_count[RegisterNum] = { 0 };
  reglist_t last_spilled_regs = 0;
  reglist_t gp_list = get_cache_reg_list(GP_REG);
  reglist_t fp_list = get_cache_reg_list(FP_REG);

  printf("test register\n");

  printf("has(gp_list, GP_RAX)......");
  test(has(gp_list, GP_RAX),1);

  clear(&gp_list, GP_RAX);
  printf("clear(gp_list, GP_RAX)......");
  test(has(gp_list, GP_RAX), 0);

  set(&gp_list, GP_RAX);
  printf("set(gp_list, GP_RAX)......");
  test(has(gp_list, GP_RAX), 1);

  printf("has(gp_list, GP_R15)......");
  test(has(gp_list, GP_R15), 1);

  printf("has(gp_list, FP_XMM0)......");
  test(has(gp_list, FP_XMM0), 0);

  reglist_t list = 0;
  printf("is_empty(list)......");
  test(is_empty(list), 1);

  // Randomly initialize the register list.
  set(&list, FP_XMM3);
  set(&list, FP_XMM12);
  set(&list, FP_XMM8);
  set(&list, GP_RCX);
  set(&list, GP_R9);
  set(&list, GP_RDI);

  gp_list = 0x282;
  fp_list = 0x1108 << 16;
  printf("get_gp_list(list)......");
  test(get_gp_list(list), gp_list);
  printf("get_fp_list(list)......");
  test(get_fp_list(list), fp_list);

  printf("get_first_reg_set(gp_list)......");
  test(get_first_reg_set(get_gp_list(list)), GP_RCX);
  printf("get_last_reg_set(gp_list)......");
  test(get_last_reg_set(get_gp_list(list)), GP_R9);

  printf("get_first_reg_set(fp_list)......");
  test(get_first_reg_set(get_fp_list(list)), FP_XMM3);
  printf("get_last_reg_set(fp_list)......");
  test(get_last_reg_set(get_fp_list(list)), FP_XMM12);

  printf("has_unused_register_with_class(list, gp, null)......");
  test(has_unused_register_with_class(list, GP_REG, EmptyRegList), 1);
  gp_list = get_cache_reg_list(GP_REG);
  printf("has_unused_register_with_class(list, gp, gp_list)......");
  test(has_unused_register_with_class(list, GP_REG, gp_list), 0);
  printf("has_unused_register_with_class(gp_list, gp, null)......");
  test(has_unused_register_with_class(gp_list, GP_REG, EmptyRegList), 0);
  printf("has_unused_register_with_class(gp_list, fp, null)......");
  test(has_unused_register_with_class(gp_list, FP_REG, EmptyRegList), 1);
  fp_list = get_cache_reg_list(FP_REG);
  printf("has_unused_register_with_class(list, fp, fp_list)......");
  test(has_unused_register_with_class(list, FP_REG, fp_list), 0);
  printf("has_unused_register_with_class(fp_list, fp, null)......");
  test(has_unused_register_with_class(fp_list, FP_REG, EmptyRegList), 0);
  printf("has_unused_register_with_class(fp_list, gp, null)......");
  test(has_unused_register_with_class(fp_list, GP_REG, EmptyRegList), 1);  

  printf("is_used(used_registers, register_use_count, GP_RCX)......");
  test(is_used(used_registers, register_use_count, GP_RCX), 0);
  inc_used(&used_registers, register_use_count, GP_RCX);
  printf("inc_used(used_registers, register_use_count, GP_RCX)......");
  test(is_used(used_registers, register_use_count, GP_RCX), 1);
  printf("dec_used(used_registers, register_use_count, GP_RCX)......");
  dec_used(&used_registers, register_use_count, GP_RCX); 
  test(is_used(used_registers, register_use_count, GP_RCX), 0);
  printf("get_use_count(register_use_count, GP_RCX)......");
  test(get_use_count(register_use_count, GP_RCX), 0);
  printf("...............................................");
  inc_used(&used_registers, register_use_count, GP_RCX);
  inc_used(&used_registers, register_use_count, GP_RCX);
  test(get_use_count(register_use_count, GP_RCX), 2);
  printf("clear_used(register_use_count, GP_RCX)......");
  clear_used(&used_registers, register_use_count, GP_RCX);
  test(get_use_count(register_use_count, GP_RCX), 0);
  printf("............................................");
  test(is_used(used_registers, register_use_count, GP_RCX), 0);

  dump_bit(AllocableRegList);
  test(has(AllocableRegList, GP_RCX), 1);
  printf("............................");
  test(has(AllocableRegList, GP_RBP), 1);
  printf("............................");
  test(has(AllocableRegList, GP_RSP), 1);
  printf("............................");
  test(has(AllocableRegList, GP_R10), 1);
  printf("............................");
  test(has(AllocableRegList, GP_R11), 1);
  printf("............................");
  test(has(AllocableRegList, GP_R13), 1);
  printf("............................");
  test(has(AllocableRegList, FP_XMM15), 1);
  printf("............................");
  test(has(AllocableRegList, FP_XMM14), 1);  

  return 0;
}