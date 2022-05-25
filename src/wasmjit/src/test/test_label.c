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
dump_output_to_file(struct SizedBuffer* output)
{
  FILE *f;
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
  double double_fp;
  float fp;
  uint64_t pc_offset_stack_frame;

  printf("test label\n");

  printf("Case: jmp before bind.\n");
  label_t label = {0, 0};
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit_jmp_label(output, &label, Far);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  bind(output, &label, pc_offset(output));
  emit(output, 0xcc);
  i = dump_output(output, 0);

  printf("Case: jmp after bind.\n");
  if (!output_buf_reset(output)) {
    printf("reset output buffer failed!\n");
    return 0;
  }
  label.pos = 0;
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  bind(output, &label, pc_offset(output));
  emit(output, 0xcc);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit_jmp_label(output, &label, Far);
  i = dump_output(output, 0);

  printf("Case: multiple jmp before bind.\n");
  if (!output_buf_reset(output)) {
    printf("reset output buffer failed!\n");
    return 0;
  }
  label.pos = 0;
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit_jmp_label(output, &label, Far);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit_jmp_label(output, &label, Far);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit_jmp_label(output, &label, Far);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  bind(output, &label, pc_offset(output));
  emit(output, 0xcc);
  i = dump_output(output, 0);

  printf("Case: cond_jmp before bind.\n");
  if (!output_buf_reset(output)) {
    printf("reset output buffer failed!\n");
    return 0;
  }
  label.pos = 0;
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit_cond_jump_rr(output, COND_EQ, &label, VALTYPE_I32, GP_RAX, REG_UNKNOWN);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  bind(output, &label, pc_offset(output));
  emit(output, 0xcc);
  i = dump_output(output, 0);

  printf("Case: multiple cond_jmp before bind.\n");
  if (!output_buf_reset(output)) {
    printf("reset output buffer failed!\n");
    return 0;
  }
  label.pos = 0;
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit_cond_jump_rr(output, COND_EQ, &label, VALTYPE_I32, GP_RAX, REG_UNKNOWN);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit_cond_jump_rr(output, COND_EQ, &label, VALTYPE_I32, GP_RAX, REG_UNKNOWN);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit_cond_jump_rr(output, COND_EQ, &label, VALTYPE_I32, GP_RAX, REG_UNKNOWN);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  bind(output, &label, pc_offset(output));
  emit(output, 0xcc);
  i = dump_output(output, 0);

  printf("Case: cond_jmp after bind.\n");
  if (!output_buf_reset(output)) {
    printf("reset output buffer failed!\n");
    return 0;
  }
  label.pos = 0;
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  bind(output, &label, pc_offset(output));
  emit(output, 0xcc);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit(output, 0x90);
  emit_cond_jump_rr(output, COND_EQ, &label, VALTYPE_I32, GP_RAX, REG_UNKNOWN);
  i = dump_output(output, 0);

  dump_output_to_file(output);

  return 0;
}