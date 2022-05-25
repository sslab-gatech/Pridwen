#include <wasmjit/assembler.h>

uint64_t
test_prepare_stack_frame(struct SizedBuffer* output)
{
  uint64_t offset;
  size_t memref_idx;

  // push %rbp
  emit_pushq_r(output, GP_RBP);

  // movq %rbp, %rsp
  emit_mov_rr(output, GP_RBP, GP_RSP, VALTYPE_I64);

  // subq %rsp, $0
  emit_subq_sp_32(output, 0);

  offset = pc(output);
  offset -= 4;

  return offset;
}

void
test_patch_stack_frame(uint64_t addr, uint32_t size)
{
  size_t i;
  char* mem = (char*)addr;
  for (i = 0; i < 4; i++) {
    mem[i] = size & 0xff;
    size = size >> 8;
  }
}

size_t
dump_output_to_file(struct SizedBuffer* output)
{
  FILE* f;
  f = fopen("code.bin", "wb");
  printf("write to file...\n");
  fwrite(output->data, output->size, 1, f);
  fclose(f);

  return output->size;
}

size_t
dump_output(struct SizedBuffer* output, size_t start)
{
  size_t i;
  for (i = start; i < output->size; i++) {
    uint8_t byte = (uint8_t)output->data[i];
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

int
main()
{
  struct SizedBuffer outputv = { 0, 0, NULL };
  struct SizedBuffer* output = &outputv;
  size_t i = 0;
  double double_fp;
  float fp;
  uint64_t pc_offset_stack_frame;

  printf("test assembler\n");

#if 0 // Test stack frame patching.
    printf("prepare_stack_frame\n");
    pc_offset_stack_frame = test_prepare_stack_frame(output);
    dump_output(output, 0);

    printf("patch_stack_frame\n");
    test_patch_stack_frame(pc_offset_stack_frame, 0x1234);
    dump_output(output, 0);
#endif

  printf("emit_shl_r(RDI, I32)\n");
  emit_shl_r(output, GP_RDI, VALTYPE_I32);
  printf("emit_shr_r(RDI, I32)\n");
  emit_shr_r(output, GP_RDI, VALTYPE_I32);
  printf("emit_sar_r(RDI, I32)\n");
  emit_sar_r(output, GP_RDI, VALTYPE_I32);
  printf("emit_shl_r(RDI, I64)\n");
  emit_shl_r(output, GP_RDI, VALTYPE_I64);
  printf("emit_shr_r(RDI, I64)\n");
  emit_shr_r(output, GP_RDI, VALTYPE_I64);
  printf("emit_sar_r(RDI, I64)\n");
  emit_sar_r(output, GP_RDI, VALTYPE_I64);
  i = dump_output(output, i);

  printf("emit_float_set_cond(NE, RAX, XMM0, XMM9, F32)\n");
  emit_float_set_cond(output, COND_NE, GP_RAX, FP_XMM0, FP_XMM9, VALTYPE_F32);
  i = dump_output(output, i);

  struct Operand op;
  printf("leaq %%rax, OP\n");
  build_operand(&op, GP_RSI, GP_R9, SCALE_1, 0);
  emit_lea_rm(output, GP_RCX, &op, VALTYPE_I64);
  i = dump_output(output, i);

  printf("leal %%rax, OP\n");
  build_operand(&op, GP_RSI, GP_R9, SCALE_1, 0);
  emit_lea_rm(output, GP_RCX, &op, VALTYPE_I32);
  i = dump_output(output, i);
  {
    label_t label = { 0, 0 };
    printf("leaq %%r15, RIP + 5\n");
    build_operand(&op, REG_UNKNOWN, REG_UNKNOWN, SCALE_NONE, 5);
    emit_lea_rm(output, GP_R15, &op, VALTYPE_I64);
    emit_jmp_label(output, &label, Far);
    emit(output, 0x90);
    i = dump_output(output, i);
  }

  printf("addq %%rax, %%rbx\n");
  emit_add_rr(output, GP_RAX, GP_RBX, VALTYPE_I64);
  i = dump_output(output, i);

  printf("addq %%r9, %%rdi\n");
  emit_add_rr(output, GP_R9, GP_RDI, VALTYPE_I64);
  i = dump_output(output, i);

  printf("addl %%eax, %%ebx\n");
  emit_add_rr(output, GP_RAX, GP_RBX, VALTYPE_I32);
  i = dump_output(output, i);

  printf("addl %%r9d, %%edi\n");
  emit_add_rr(output, GP_R9, GP_RDI, VALTYPE_I32);
  i = dump_output(output, i);

  printf("subq %%rax, %%rbx\n");
  emit_sub_rr(output, GP_RAX, GP_RBX, VALTYPE_I64);
  i = dump_output(output, i);

  printf("subq %%r9, %%rdi\n");
  emit_sub_rr(output, GP_R9, GP_RDI, VALTYPE_I64);
  i = dump_output(output, i);

  printf("subl %%eax, %%ebx\n");
  emit_sub_rr(output, GP_RAX, GP_RBX, VALTYPE_I32);
  i = dump_output(output, i);

  printf("subl %%r9d, %%edi\n");
  emit_sub_rr(output, GP_R9, GP_RDI, VALTYPE_I32);
  i = dump_output(output, i);
  dump_output_to_file(output);

  printf("imulq %%rax, %%rbx\n");
  emit_imul_rr(output, GP_RAX, GP_RBX, VALTYPE_I64);
  i = dump_output(output, i);

  printf("imulq %%r9, %%rdi\n");
  emit_imul_rr(output, GP_R9, GP_RDI, VALTYPE_I64);
  i = dump_output(output, i);

  printf("imull %%eax, %%ebx\n");
  emit_imul_rr(output, GP_RAX, GP_RBX, VALTYPE_I32);
  i = dump_output(output, i);

  printf("imull %%r9d, %%edi\n");
  emit_imul_rr(output, GP_R9, GP_RDI, VALTYPE_I32);
  i = dump_output(output, i);
  dump_output_to_file(output);

  printf("andq %%rax, %%rbx\n");
  emit_and_rr(output, GP_RAX, GP_RBX, VALTYPE_I64);
  i = dump_output(output, i);

  printf("andq %%r9, %%rdi\n");
  emit_and_rr(output, GP_R9, GP_RDI, VALTYPE_I64);
  i = dump_output(output, i);

  printf("andl %%eax, %%ebx\n");
  emit_and_rr(output, GP_RAX, GP_RBX, VALTYPE_I32);
  i = dump_output(output, i);

  printf("andl %%r9d, %%edi\n");
  emit_and_rr(output, GP_R9, GP_RDI, VALTYPE_I32);
  i = dump_output(output, i);
  dump_output_to_file(output);

  printf("orq %%rax, %%rbx\n");
  emit_or_rr(output, GP_RAX, GP_RBX, VALTYPE_I64);
  i = dump_output(output, i);

  printf("orq %%r9, %%rdi\n");
  emit_or_rr(output, GP_R9, GP_RDI, VALTYPE_I64);
  i = dump_output(output, i);

  printf("orl %%eax, %%ebx\n");
  emit_or_rr(output, GP_RAX, GP_RBX, VALTYPE_I32);
  i = dump_output(output, i);

  printf("orl %%r9d, %%edi\n");
  emit_or_rr(output, GP_R9, GP_RDI, VALTYPE_I32);
  i = dump_output(output, i);
  dump_output_to_file(output);

  printf("%movq %%rax, 0x0\n");
  printf("0x%llx: ", pc(output));
  emit_movq_ri(output, GP_RAX, 0x0);
  i = dump_output(output, 0);

  printf("movq %%rax, 0x1234\n");
  printf("0x%llx: ", pc(output));
  emit_movq_ri(output, GP_RAX, 0x1234);
  i = dump_output(output, i);

  printf("movq %%rax, 0xffffffffffff\n");
  printf("0x%llx: ", pc(output));
  emit_movq_ri(output, GP_RAX, 0xffffffffffff);
  i = dump_output(output, i);

  printf("movq %%r9, 0x1234\n");
  printf("0x%llx: ", pc(output));
  emit_movq_ri(output, GP_R9, 0x1234);
  i = dump_output(output, i);

  printf("movl %%ecx, 0x0\n");
  emit_movl_ri(output, GP_RCX, 0x0);
  i = dump_output(output, i);

  printf("movl %%eax, 0x1234\n");
  emit_movl_ri(output, GP_RCX, 0x1234);
  i = dump_output(output, i);

  printf("xorq %%rax, %%rbx\n");
  emit_xor_rr(output, GP_RAX, GP_RBX, VALTYPE_I64);
  i = dump_output(output, i);

  printf("xorq %%r9, %%rdi\n");
  emit_xor_rr(output, GP_R9, GP_RDI, VALTYPE_I64);
  i = dump_output(output, i);

  printf("xorl %%eax, %%ebx\n");
  emit_xor_rr(output, GP_RAX, GP_RBX, VALTYPE_I32);
  i = dump_output(output, i);

  printf("xorl %%r9d, %%edi\n");
  emit_xor_rr(output, GP_R9, GP_RDI, VALTYPE_I32);
  i = dump_output(output, i);

  printf("cmpq %%rax, %%rbx\n");
  emit_cmp_rr(output, GP_RAX, GP_RBX, VALTYPE_I64);
  i = dump_output(output, i);

  printf("cmpq %%r9, %%rdi\n");
  emit_cmp_rr(output, GP_R9, GP_RDI, VALTYPE_I64);
  i = dump_output(output, i);

  printf("cmpl %%eax, %%ebx\n");
  emit_cmp_rr(output, GP_RAX, GP_RBX, VALTYPE_I32);
  i = dump_output(output, i);

  printf("cmpl %%r9d, %%edi\n");
  emit_cmp_rr(output, GP_R9, GP_RDI, VALTYPE_I32);
  i = dump_output(output, i);

  printf("testq %%rax, %%rax\n");
  emit_test_rr(output, GP_RAX, GP_RAX, VALTYPE_I64);
  i = dump_output(output, i);

  printf("testq %%r9, %%r9\n");
  emit_test_rr(output, GP_R9, GP_R9, VALTYPE_I64);
  i = dump_output(output, i);

  printf("testl %%eax, %%eax\n");
  emit_test_rr(output, GP_RAX, GP_RAX, VALTYPE_I32);
  i = dump_output(output, i);

  printf("testl %%r9d, %%r9d\n");
  emit_test_rr(output, GP_R9, GP_R9, VALTYPE_I32);
  i = dump_output(output, i);

  printf("xorps %%xmm0, %%xmm0\n");
  emit_xorps_rr(output, FP_XMM0, FP_XMM0);
  i = dump_output(output, i);

  printf("xorps %%xmm8, %%xmm8\n");
  emit_xorps_rr(output, FP_XMM8, FP_XMM8);
  i = dump_output(output, i);

  printf("xorpd %%xmm0, %%xmm0\n");
  emit_xorpd_rr(output, FP_XMM0, FP_XMM0);
  i = dump_output(output, i);

  printf("xorpd %%xmm8, %%xmm8\n");
  emit_xorpd_rr(output, FP_XMM8, FP_XMM8);
  i = dump_output(output, i);

  printf("LoadConstant(rdx, 0, i32)\n");
  LoadConstant(output, GP_RDX, 0, VALTYPE_I32);
  i = dump_output(output, i);

  printf("LoadConstant(r9, 0, i32)\n");
  LoadConstant(output, GP_R9, 0, VALTYPE_I32);
  i = dump_output(output, i);

  printf("LoadConstant(rdx, 1234, i32)\n");
  LoadConstant(output, GP_RDX, 1234, VALTYPE_I32);
  i = dump_output(output, i);

  printf("LoadConstant(r9, 1234, i32)\n");
  LoadConstant(output, GP_R9, 1234, VALTYPE_I32);
  i = dump_output(output, i);

  printf("LoadConstant(rdx, 0, i64)\n");
  LoadConstant(output, GP_RDX, 0, VALTYPE_I64);
  i = dump_output(output, i);

  printf("LoadConstant(r9, 0, i64)\n");
  LoadConstant(output, GP_R9, 0, VALTYPE_I64);
  i = dump_output(output, i);

  printf("LoadConstant(rdx, 1234, i64)\n");
  LoadConstant(output, GP_RDX, 1234, VALTYPE_I64);
  i = dump_output(output, i);

  printf("LoadConstant(r9, 1234, i64)\n");
  LoadConstant(output, GP_R9, 1234, VALTYPE_I64);
  i = dump_output(output, i);

  printf("LoadConstant(rdx, 0xffffffffffff, i64)\n");
  LoadConstant(output, GP_RDX, 0xffffffffffff, VALTYPE_I64);
  i = dump_output(output, i);

  printf("LoadConstant(r9, 0xffffffffffff, i64)\n");
  LoadConstant(output, GP_R9, 0xffffffffffff, VALTYPE_I64);
  i = dump_output(output, i);

  printf("LoadConstant(xmm0, 0, f32)\n");
  LoadConstant(output, FP_XMM0, 0, VALTYPE_F32);
  i = dump_output(output, i);

  printf("LoadConstant(xmm8, 0, f32)\n");
  LoadConstant(output, FP_XMM8, 0, VALTYPE_F32);
  i = dump_output(output, i);

  printf("LoadConstant(xmm0, 1.0, f32)\n");
  LoadConstant(output, FP_XMM0, f32_encoding(1.0), VALTYPE_F32);
  i = dump_output(output, i);

  printf("LoadConstant(xmm8, 1.0, f32)\n");
  LoadConstant(output, FP_XMM8, f32_encoding(1.0), VALTYPE_F32);
  i = dump_output(output, i);

  printf("LoadConstant(xmm0, 1.2345, f32)\n");
  LoadConstant(output, FP_XMM0, f32_encoding(1.2345), VALTYPE_F32);
  i = dump_output(output, i);

  printf("LoadConstant(xmm8, 1.2345, f32)\n");
  LoadConstant(output, FP_XMM8, f32_encoding(1.2345), VALTYPE_F32);
  i = dump_output(output, i);

  printf("LoadConstant(xmm0, 0, f64)\n");
  LoadConstant(output, FP_XMM0, 0, VALTYPE_F64);
  i = dump_output(output, i);

  printf("LoadConstant(xmm8, 0, f64)\n");
  LoadConstant(output, FP_XMM8, 0, VALTYPE_F64);
  i = dump_output(output, i);

  printf("LoadConstant(xmm0, 1.0, f64)\n");
  LoadConstant(output, FP_XMM0, f64_encoding(1.0), VALTYPE_F64);
  i = dump_output(output, i);

  printf("LoadConstant(xmm8, 1.0, f64)\n");
  LoadConstant(output, FP_XMM8, f64_encoding(1.0), VALTYPE_F64);
  i = dump_output(output, i);

  printf("LoadConstant(xmm0, 1.2345, f64)\n");
  LoadConstant(output, FP_XMM0, f64_encoding(1.2345), VALTYPE_F64);
  i = dump_output(output, i);

  printf("LoadConstant(xmm8, 1.2345, f64)\n");
  LoadConstant(output, FP_XMM8, f64_encoding(1.2345), VALTYPE_F64);
  i = dump_output(output, i);

  printf("Fill(RAX, 10, I32);\n");
  Fill(output, GP_RAX, 10, VALTYPE_I32);
  i = dump_output(output, i);

  printf("Fill(R9, 20, I32);\n");
  Fill(output, GP_R9, 20, VALTYPE_I32);
  i = dump_output(output, i);

  printf("Fill(RAX, 300, I32);\n");
  Fill(output, GP_RAX, 300, VALTYPE_I32);
  i = dump_output(output, i);

  printf("Fill(RAX, 10, I64);\n");
  Fill(output, GP_RAX, 10, VALTYPE_I64);
  i = dump_output(output, i);

  printf("Fill(R9, 20, I64);\n");
  Fill(output, GP_R9, 20, VALTYPE_I64);
  i = dump_output(output, i);

  printf("Fill(RAX, 300, I64);\n");
  Fill(output, GP_RAX, 300, VALTYPE_I64);
  i = dump_output(output, i);

  printf("Fill(XMM0, 10, F32);\n");
  Fill(output, FP_XMM0, 10, VALTYPE_F32);
  i = dump_output(output, i);

  printf("Fill(XMM8, 20, F32);\n");
  Fill(output, FP_XMM8, 20, VALTYPE_F32);
  i = dump_output(output, i);

  printf("Fill(XMM0, 300, F32);\n");
  Fill(output, FP_XMM0, 300, VALTYPE_F32);
  i = dump_output(output, i);

  printf("Fill(XMM0, 10, F64);\n");
  Fill(output, FP_XMM0, 10, VALTYPE_F64);
  i = dump_output(output, i);

  printf("Fill(XMM8, 20, F64);\n");
  Fill(output, FP_XMM8, 20, VALTYPE_F64);
  i = dump_output(output, i);

  printf("Fill(XMM0, 300, F64);\n");
  Fill(output, FP_XMM0, 300, VALTYPE_F64);
  i = dump_output(output, i);

  printf("Spill(25, RAX, 0, I32);\n");
  Spill(output, NULL, 23, GP_RAX, 0, VALTYPE_I32);
  i = dump_output(output, i);

  printf("Spill(25, R9, 0, I32);\n");
  Spill(output, NULL, 23, GP_R9, 0, VALTYPE_I32);
  i = dump_output(output, i);

  printf("Spill(25, RAX, 0, I64);\n");
  Spill(output, NULL, 23, GP_RAX, 0, VALTYPE_I64);
  i = dump_output(output, i);

  printf("Spill(25, R9, 0, I64);\n");
  Spill(output, NULL, 23, GP_R9, 0, VALTYPE_I64);
  i = dump_output(output, i);

  printf("Spill(25, XMM0, 0, F32);\n");
  Spill(output, NULL, 23, FP_XMM0, 0, VALTYPE_F32);
  i = dump_output(output, i);

  printf("Spill(25, XMM8, 0, F32);\n");
  Spill(output, NULL, 23, FP_XMM8, 0, VALTYPE_F32);
  i = dump_output(output, i);

  printf("Spill(25, XMM0, 0, F64);\n");
  Spill(output, NULL, 23, FP_XMM0, 0, VALTYPE_F64);
  i = dump_output(output, i);

  printf("Spill(25, XMM8, 0, F64);\n");
  Spill(output, NULL, 23, FP_XMM8, 0, VALTYPE_F64);
  i = dump_output(output, i);

  printf("Spill(25, NONE, 123, I32);\n");
  Spill(output, NULL, 23, REG_UNKNOWN, 123, VALTYPE_I32);
  i = dump_output(output, i);

  printf("Spill(25, NONE, -12345, I64);\n");
  Spill(output, NULL, 23, REG_UNKNOWN, -12345, VALTYPE_I64);
  i = dump_output(output, i);

  printf("Spill(25, NONE, 2147483648, I64);\n");
  Spill(output, NULL, 23, REG_UNKNOWN, 2147483648, VALTYPE_I64);
  i = dump_output(output, i);

  printf("Spill(25, NONE, 4294967296, I64);\n");
  Spill(output, NULL, 23, REG_UNKNOWN, 4294967296, VALTYPE_I64);
  i = dump_output(output, i);

  printf("Move(RAX, RBX, I32);\n");
  Move(output, GP_RAX, GP_RBX, VALTYPE_I32);
  i = dump_output(output, i);

  printf("Move(RAX, R9, I32);\n");
  Move(output, GP_RAX, GP_R9, VALTYPE_I32);
  i = dump_output(output, i);

  printf("Move(R9, R10, I32);\n");
  Move(output, GP_R9, GP_R10, VALTYPE_I32);
  i = dump_output(output, i);

  printf("Move(RAX, RBX, I64);\n");
  Move(output, GP_RAX, GP_RBX, VALTYPE_I64);
  i = dump_output(output, i);

  printf("Move(RAX, R9, I64);\n");
  Move(output, GP_RAX, GP_R9, VALTYPE_I64);
  i = dump_output(output, i);

  printf("Move(R9, R10, I64);\n");
  Move(output, GP_R9, GP_R10, VALTYPE_I64);
  i = dump_output(output, i);

  printf("Move(XMM0, XMM1, F32);\n");
  Move(output, FP_XMM0, FP_XMM1, VALTYPE_F32);
  i = dump_output(output, i);

  printf("Move(XMM0, XMM8, F32);\n");
  Move(output, FP_XMM0, FP_XMM8, VALTYPE_F32);
  i = dump_output(output, i);

  printf("Move(XMM8, XMM9, F32);\n");
  Move(output, FP_XMM8, FP_XMM9, VALTYPE_F32);
  i = dump_output(output, i);

  printf("Move(XMM0, XMM1, F64);\n");
  Move(output, FP_XMM0, FP_XMM1, VALTYPE_F64);
  i = dump_output(output, i);

  printf("Move(XMM0, XMM8, F64);\n");
  Move(output, FP_XMM0, FP_XMM8, VALTYPE_F64);
  i = dump_output(output, i);

  printf("Move(XMM8, XMM9, F64);\n");
  Move(output, FP_XMM8, FP_XMM9, VALTYPE_F64);
  i = dump_output(output, i);

  printf("emit_test_ri(RDI, 1, I32)\n");
  emit_test_ri(output, GP_RDI, 1, VALTYPE_I32);
  i = dump_output(output, i);

  printf("emit_test_ri(RDI, 1, I64)\n");
  emit_test_ri(output, GP_RDI, 1, VALTYPE_I64);
  i = dump_output(output, i);

  printf("emit_float_min_or_max(XMM0, XMM5, XMM9, min, F32)\n");
  emit_float_min_or_max(
    output, FP_XMM0, FP_XMM5, FP_XMM9, FLOAT_MIN, VALTYPE_F32);
  i = dump_output(output, i);

  printf("emit_float_min_or_max(XMM0, XMM5, XMM9, min, F64)\n");
  emit_float_min_or_max(
    output, FP_XMM0, FP_XMM5, FP_XMM9, FLOAT_MIN, VALTYPE_F64);
  i = dump_output(output, i);

  printf("mov r14, rax\n");
  printf("mov rax, r14\n");
  emit_mov_rr(output, GP_R14, GP_RAX, VALTYPE_I64);
  emit_mov_rr(output, GP_RAX, GP_R14, VALTYPE_I64);
  i = dump_output(output, i);
  {
    label_t label = { 0, 0 };
    bind_label(output, &label, pc_offset(output));
    printf("xbegin 0x0\n");
    printf("xend\n");
    emit_xbegin(output, &label);
    emit_xend(output);
    i = dump_output(output, i);
  }

  InitStack(output, 10, 48);
  i = dump_output(output, i);

#if 0
    double_fp = 0.0;
    printf("double_fp: %lf: %u\n", double_fp, test_double_fp(double_fp));
    double_fp = 1.0;
    printf("double_fp: %lf: %u\n", double_fp, test_double_fp(double_fp));
    double_fp = 0.5;
    printf("double_fp: %lf: %u\n", double_fp, test_double_fp(double_fp));
    double_fp = 1.0 / 3;
    printf("double_fp: %lf: %u\n", double_fp, test_double_fp(double_fp));
    double_fp = 12345.0;
    printf("double_fp: %lf: %u\n", double_fp, test_double_fp(double_fp));
    double_fp = 1.2345;
    printf("double_fp: %lf: %u\n", double_fp, test_double_fp(double_fp));

    fp = 0.0;
    printf("fp: %f: %u\n", fp, test_fp(fp));
    fp = 1.0;
    printf("fp: %f: %u\n", fp, test_fp(fp));
    fp = 0.5;
    printf("fp: %f: %u\n", fp, test_fp(fp));
    fp = 1.0 / 3;
    printf("fp: %f: %u\n", fp, test_fp(fp));
    fp = 12345.0;
    printf("fp: %f: %u\n", fp, test_fp(fp));
    fp = 1.2345;
    printf("fp: %f: %u\n", fp, test_fp(fp));
#endif

  return 0;
}
