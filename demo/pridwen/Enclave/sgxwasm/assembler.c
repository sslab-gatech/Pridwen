#include <sgxwasm/assembler.h>
#include <stdarg.h>

#define is_byte_register(reg) (reg <= 3)
#define low_bits(reg) (reg & 0x7)
#define high_bit(reg) (reg >> 3)

#ifndef DEBUG_ASSEMBLE
#define DEBUG_ASSEMBLE 0
#endif

enum
{
  IMM_1_BYTE = 1,
  IMM_2_BYTE = 2,
  IMM_4_BYTE = 4,
  IMM_8_BYTE = 8,
};

static uint8_t
get_num_bytes(uint64_t imm)
{
  uint8_t num = 0;
  while (imm != 0) {
    imm = imm >> 8;
    num++;
  }
  if (num <= IMM_1_BYTE) {
    return IMM_1_BYTE;
  }
  if (num <= IMM_2_BYTE) {
    return IMM_2_BYTE;
  }
  if (num <= IMM_4_BYTE) {
    return IMM_4_BYTE;
  }
  return IMM_8_BYTE;
}

void
dump_bytes(uint8_t* val, uint8_t byte)
{
  int i;
  // uint8_t *a = (uint8_t *)&val;
  for (i = (int)byte - 1; i >= 0; i--) {
    uint8_t b = (uint8_t)val[i];
    if (b == 0) {
      printf("00 ");
      continue;
    }
    printf("%x ", b);
  }
  printf("\n");
}

uint32_t
test_double_fp(double fp)
{
  uint64_t encoding;
  uint32_t nlz, ntz, pop;

  dump_bytes((uint8_t*)&fp, IMM_8_BYTE);
  memcpy(&encoding, &fp, sizeof(uint64_t));
  nlz = count_leading_zeros(encoding, IMM_8_BYTE * 8);
  ntz = count_trailing_zeros(encoding, IMM_8_BYTE);
  pop = count_population(encoding, IMM_8_BYTE);

  return nlz + ntz + pop;
}

uint32_t
test_fp(float fp)
{
  uint64_t encoding;
  uint32_t nlz, ntz, pop;

  dump_bytes((uint8_t*)&fp, IMM_4_BYTE);
  memset(&encoding, 0, sizeof(uint64_t));
  memcpy(&encoding, &fp, sizeof(uint32_t));
  nlz = count_leading_zeros(encoding, IMM_4_BYTE * 8);
  ntz = count_trailing_zeros(encoding, IMM_4_BYTE);
  pop = count_population(encoding, IMM_4_BYTE);

  return nlz + ntz + pop;
}

// -----------------------------------------------------------------------------
// Implementation of Label

// Returns the position of bound or linked labels. Cannot be used
// for unused labels.
int32_t
label_pos(label_t* label)
{
  if (label->pos < 0)
    return -label->pos - 1;
  if (label->pos > 0)
    return label->pos - 1;
  assert(0); // Unreachable.
}

int32_t
near_link_pos(label_t* label)
{
  return label->near_link_pos - 1;
}

void
unuse(label_t* label)
{
  label->pos = 0;
}

void
unuse_near(label_t* label)
{
  label->near_link_pos = 0;
}

int
is_bound(label_t* label)
{
  return label->pos < 0;
}

int
is_unused(label_t* label)
{
  return label->pos == 0 && label->near_link_pos == 0;
}

int
is_linked(label_t* label)
{
  return label->pos > 0;
}

int
is_near_linked(label_t* label)
{
  return label->near_link_pos > 0;
}

void
bind_to(label_t* label, int pos)
{
  label->pos = -pos - 1;
}

void
link_to(label_t* label, int pos, distance_t distance)
{
  if (distance == Near) {
    label->near_link_pos = pos + 1;
    assert(is_near_linked(label));
  } else {
    label->pos = pos + 1;
    assert(is_linked);
  }
}

void
bind_label(struct SizedBuffer* output, label_t* label, int pos)
{
  assert(!is_bound(label)); // Label may only be bound once.
  assert(0 <= pos && pos <= (int)pc_offset(output)); // Position must be valid.
  if (is_linked(label)) {
    // Patch the address.
    int current = label_pos(label);
    int next = long_at(output, current);
    while (next != current) {
      // TODO: Handle absolute address.

      // Relative address.
      int imm32 = pos - (current + JmpLongSize);
      int* addr = (int*)pc_at(output, current);
      *addr = imm32;

      current = next;
      next = long_at(output, next);
    }
    // Fix up the last address on the linked list.
    // Relative address.
    int imm32 = pos - (current + JmpLongSize);
    int* addr = (int*)pc_at(output, current);
    *addr = imm32;
  }

  // TODO: Optimization.
  while (is_near_linked(label)) {
    int fixup_ops = near_link_pos(label);
    int8_t offset_to_next = byte_at(output, fixup_ops);
    assert(offset_to_next <= 0);
    int disp = pos - (fixup_ops + sizeof(int8_t));
    assert(is_int8(disp));
    set_byte_at(output, fixup_ops, disp);
    if (offset_to_next < 0) {
      link_to(label, fixup_ops + offset_to_next, Near);
    } else {
      unuse_near(label);
    }
  }

  bind_to(label, pos);
}

// End of Label

void
emit(struct SizedBuffer* output, int8_t byte)
{
  if (!output_buf(output, &byte, 1)) {
    return;
  }
}

void
emit_imm(struct SizedBuffer* output, int64_t val, uint8_t byte)
{
  size_t i;
  for (i = 0; i < byte; i++) {
    emit(output, val & 0xff);
    val = val >> 8;
  }
}

void
emit_rex_rr(struct SizedBuffer* output, sgxwasm_register_t reg,
            sgxwasm_register_t rm_reg, sgxwasm_valtype_t type, uint8_t optional)
{
  assert(type == VALTYPE_I64 || type == VALTYPE_I32 || type == VALTYPE_F64 ||
         type == VALTYPE_F32);
  if (reg >= FPGroupOffset)
    reg -= FPGroupOffset;
  if (rm_reg >= FPGroupOffset)
    rm_reg -= FPGroupOffset;
  if (type == VALTYPE_I64 || type == VALTYPE_F64) {
    emit(output, 0x48 | high_bit(reg) << 2 | high_bit(rm_reg));
  } else {
    uint8_t rex_bits = high_bit(reg) << 2 | high_bit(rm_reg);
    if (optional && rex_bits == 0) {
      return;
    }
    // Alway output for the non-optional case.
    emit(output, 0x40 | rex_bits);
  }
}

void
emit_rex_rm(struct SizedBuffer* output, sgxwasm_register_t reg,
            struct Operand* op, sgxwasm_valtype_t type, uint8_t optional)
{
  assert(type == VALTYPE_I64 || type == VALTYPE_I32 || type == VALTYPE_F64 ||
         type == VALTYPE_F32);
  if (reg >= FPGroupOffset)
    reg -= FPGroupOffset;
  if (type == VALTYPE_I64 || type == VALTYPE_F64) {
    emit(output, 0x48 | high_bit(reg) << 2 | op->rex);
  } else {
    uint8_t rex_bits = high_bit(reg) << 2 | op->rex;
    if (optional && rex_bits == 0) {
      return;
    }
    // Alway output for the non-optional case.
    emit(output, 0x40 | rex_bits);
  }
}

void
emit_rex_r(struct SizedBuffer* output, sgxwasm_register_t rm_reg,
           sgxwasm_valtype_t type, uint8_t optional)
{
  assert(type == VALTYPE_I64 || type == VALTYPE_I32 || type == VALTYPE_F64 ||
         type == VALTYPE_F32);
  if (type == VALTYPE_I64 || type == VALTYPE_F64) {
    emit(output, 0x48 | high_bit(rm_reg));
  } else {
    uint8_t rex_bits = high_bit(rm_reg);
    if (optional && rex_bits == 0) {
      return;
    }
    // Alway output for the non-optional case.
    emit(output, 0x40 | high_bit(rm_reg));
  }
}

void
emit_rex_m(struct SizedBuffer* output, struct Operand* op,
           sgxwasm_valtype_t type, uint8_t optional)
{
  if (type == VALTYPE_I64 || type == VALTYPE_F64) {
    emit(output, 0x48 | op->rex);
  } else {
    uint8_t rex_bits = op->rex;
    if (optional && rex_bits == 0) {
      return;
    }
    emit(output, 0x40 | rex_bits);
  }
}

// -----------------------------------------------------------------------------
// Implementation of Operand

static void
set_modrm(struct Operand* operand, int mod, sgxwasm_register_t rm_reg)
{
  operand->buf[0] = mod << 6 | low_bits(rm_reg);
  // Set REX.B to the high bit of rm.code().
  operand->rex |= high_bit(rm_reg);
}

static void
set_sib(struct Operand* operand, scale_factor_t scale, sgxwasm_register_t index,
        sgxwasm_register_t base)
{
  assert(operand->len == 1);
  // Use SIB with no index register only for base rsp or r12. Otherwise we
  // would skip the SIB byte entirely.
  assert(index != GP_RSP || base == GP_RSP || base == GP_R12);
  operand->buf[1] = (scale << 6) | (low_bits(index) << 3) | low_bits(base);
  operand->rex |= high_bit(index) << 1 | high_bit(base);
  operand->len = 2;
}

static void
set_disp8(struct Operand* operand, int disp)
{
  assert(is_int8(disp));
  assert(operand->len == 1 || operand->len == 2);
  operand->buf[operand->len] = (uint8_t)disp;
  operand->len += sizeof(int8_t);
}

static void
set_disp32(struct Operand* operand, int disp)
{
  assert(operand->len == 1 || operand->len == 2);
  int32_t* p = (int32_t*)(&operand->buf[operand->len]);
  *p = disp;
  operand->len += sizeof(int32_t);
}

__attribute__((unused)) static void
set_disp64(struct Operand* operand, int64_t disp)
{
  assert(operand->len == 1);
  int64_t* p = (int64_t*)(&operand->buf[operand->len]);
  *p = disp;
  operand->len += sizeof(int64_t);
}

void
build_operand(struct Operand* operand, sgxwasm_register_t base,
              sgxwasm_register_t index, scale_factor_t scale, int32_t disp)
{
  // Initialize.
  operand->rex = 0;
  operand->len = 1;

  // Case of [base + disp/r]
  if (base != REG_UNKNOWN && index == REG_UNKNOWN && scale == SCALE_NONE) {
    if (base == GP_RSP || base == GP_R12) {
      // SIB byte is needed to encode (rsp + offset) or (r12 + offset).
      set_sib(operand, SCALE_1, GP_RSP, base);
    }

    if (disp == 0 && base != GP_RBP && base != GP_R13) {
      set_modrm(operand, 0, base);
    } else if (is_int8(disp)) {
      set_modrm(operand, 1, base);
      set_disp8(operand, disp);
    } else {
      set_modrm(operand, 2, base);
      set_disp32(operand, disp);
    }
    return;
  }

  // Case of [base + index*scale + disp/r]
  if (base != REG_UNKNOWN && index != REG_UNKNOWN && scale != SCALE_NONE) {
    assert(index != GP_RSP);
    set_sib(operand, scale, index, base);
    if (disp == 0 && base != GP_RBP && base != GP_R13) {
      // This call to set_modrm doesn't overwrite the REX.B (or REX.X) bits
      // possibly set by set_sib.
      set_modrm(operand, 0, GP_RSP);
    } else if (is_int8(disp)) {
      set_modrm(operand, 1, GP_RSP);
      set_disp8(operand, disp);
    } else {
      set_modrm(operand, 2, GP_RSP);
      set_disp32(operand, disp);
    }
    return;
  }

  // Case of [index*scale + disp/r]
  if (base == REG_UNKNOWN && index != REG_UNKNOWN && scale != SCALE_NONE) {
    assert(index != GP_RSP);
    set_modrm(operand, 0, GP_RSP);
    set_sib(operand, scale, index, GP_RBP);
    set_disp32(operand, disp);
    return;
  }

  // Case of RIP-relative addressing.
  if (base == REG_UNKNOWN && index == REG_UNKNOWN) {
    // mod = 00, r/m = 101
    set_modrm(operand, 0, GP_RBP);
    set_disp32(operand, disp);
  }
}

void
emit_operand_cm(struct SizedBuffer* output, int code, struct Operand* adr)
{
  assert(is_uint3(code));
  unsigned length = adr->len;
  assert(length > 0);

  // Emit updated ModR/M byte containing the given register.
  assert((adr->buf[0] & 0x38) == 0);
  emit(output, adr->buf[0] | code << 3);

  // Recognize RIP relative addressing.
  if (adr->buf[0] == 5) {
    // TODO: handle RIP relative addressing later.
    // label_t label = {0, 0};
    unsigned i;
    for (i = 1; i < length; i++)
      emit(output, adr->buf[i]);
  } else {
    // Emit the rest of the encoded operand.
    unsigned i;
    for (i = 1; i < length; i++)
      emit(output, adr->buf[i]);
  }
}

void
emit_operand_rm(struct SizedBuffer* output, sgxwasm_register_t reg,
                struct Operand* adr)
{
  emit_operand_cm(output, low_bits(reg), adr);
}

void
emit_operand_cr(struct SizedBuffer* output, int code, sgxwasm_register_t rm_reg)
{
  assert(is_uint3(code));
  emit(output, 0xc0 | code << 3 | low_bits(rm_reg));
}

// End of Operand

void
emit_modrm_rr(struct SizedBuffer* output, sgxwasm_register_t reg,
              sgxwasm_register_t rm_reg)
{
  if (reg >= FPGroupOffset)
    reg -= FPGroupOffset;
  if (rm_reg >= FPGroupOffset)
    rm_reg -= FPGroupOffset;
  emit(output, 0xc0 | low_bits(reg) << 3 | low_bits(rm_reg));
}

void
emit_modrm_cr(struct SizedBuffer* output, int8_t opcode,
              sgxwasm_register_t rm_reg)
{
  if (rm_reg >= FPGroupOffset)
    rm_reg -= FPGroupOffset;
  emit(output, 0xc0 | low_bits(opcode) << 3 | low_bits(rm_reg));
}

void
emit_sse_operand_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                    sgxwasm_register_t src)
{
  if (dst >= FPGroupOffset)
    dst -= FPGroupOffset;
  if (src >= FPGroupOffset)
    src -= FPGroupOffset;
  emit(output, 0xc0 | (low_bits(dst) << 3) | low_bits(src));
}

void
emit_sse_operand_rm(struct SizedBuffer* output, sgxwasm_register_t dst,
                    struct Operand* adr)
{
  if (dst >= FPGroupOffset)
    dst -= FPGroupOffset;
  emit_operand_rm(output, dst, adr);
}

int
emit_pushq_r(struct SizedBuffer* output, sgxwasm_register_t src)
{
  emit_rex_r(output, src, VALTYPE_I32, 1);
  emit(output, 0x50 | low_bits(src));
  return 1;
}

int
emit_pushq_m(struct SizedBuffer* output, struct Operand* src)
{
  emit_rex_m(output, src, VALTYPE_I32, 1);
  emit(output, 0xff);
  emit_operand_cm(output, 6, src);
  return 1;
}

int
emit_pushq_i(struct SizedBuffer* output, uint32_t value)
{
  if (is_int8(value)) {
    emit(output, 0x6a);
    emit(output, value);
  } else {
    emit(output, 0x68);
    emit_imm(output, value, IMM_4_BYTE);
  }
  return 1;
}

int
emit_popq_r(struct SizedBuffer* output, sgxwasm_register_t src)
{
  emit_rex_r(output, src, VALTYPE_I32, 1);
  emit(output, 0x58 | low_bits(src));
  return 1;
}

int
emit_ret(struct SizedBuffer* output, int imm16)
{
  assert(is_uint16(imm16));
  if (imm16 == 0) {
    emit(output, 0xc3);
  } else {
    emit(output, 0xc2);
    emit(output, imm16 & 0xff);
    emit(output, (imm16) >> 8 & 0xff);
  }
  return 1;
}

int
emit_neq_r(struct SizedBuffer* output, sgxwasm_register_t dst,
           sgxwasm_valtype_t type)
{
  emit_rex_r(output, dst, type, 1);
  emit(output, 0xf7);
  emit_modrm_cr(output, 0x3, dst);
  return 1;
}

// Only handle 32- and 64-bit operands.
void
arithmetic_op_rr(struct SizedBuffer* output, int8_t opcode,
                 sgxwasm_register_t reg, sgxwasm_register_t rm_reg,
                 sgxwasm_valtype_t type)
{
  if (low_bits(rm_reg) == 4) { // Case of RSB, forcing SIB byte.
    // Swap reg and rm_reg and change opcode operand order.
    emit_rex_rr(output, rm_reg, reg, type, 1);
    emit(output, opcode ^ 0x02);
    emit_modrm_rr(output, rm_reg, reg);
  } else {
    emit_rex_rr(output, reg, rm_reg, type, 1);
    emit(output, opcode);
    emit_modrm_rr(output, reg, rm_reg);
  }
}

void
arithmetic_op_ri(struct SizedBuffer* output, int8_t opcode,
                 sgxwasm_register_t dst, int64_t src, sgxwasm_valtype_t type)
{
  emit_rex_r(output, dst, type, 1);
  // Workaround: avoid emit 8 byte of negative number
  // when the operands are only 32 bit.
  uint8_t byte = get_num_bytes(src);
  if (byte > IMM_4_BYTE && type == VALTYPE_I32) {
    byte = IMM_4_BYTE;
  }
  if (is_int8(src)) {
    emit(output, 0x83);
    emit_modrm_cr(output, opcode, dst);
    emit(output, (uint8_t)src);
  } else if (dst == GP_RAX) {
    emit(output, 0x05 | (opcode << 3));
    emit_imm(output, src, IMM_4_BYTE);
  } else {
    emit(output, 0x81);
    emit_modrm_cr(output, opcode, dst);
    emit_imm(output, src, IMM_4_BYTE);
  }
}

void
arithmetic_op_ri_8(struct SizedBuffer* output, int8_t opcode,
                   sgxwasm_register_t dst, int8_t src)
{
  if (!is_byte_register(dst)) {
    emit_rex_r(output, dst, VALTYPE_I32, 0);
  }
  emit(output, 0x80);
  emit_modrm_cr(output, opcode, dst);
  emit(output, src);
}

int
emit_and_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
            sgxwasm_register_t src, sgxwasm_valtype_t type)
{
  arithmetic_op_rr(output, 0x23, dst, src, type);
  return 1;
}

int
emit_and_ri(struct SizedBuffer* output, sgxwasm_register_t dst, int64_t src,
            sgxwasm_valtype_t type)
{
  arithmetic_op_ri(output, 0x4, dst, src, type);
  return 1;
}

int
emit_or_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
           sgxwasm_register_t src, sgxwasm_valtype_t type)
{
  arithmetic_op_rr(output, 0x0b, dst, src, type);
  return 1;
}

int
emit_or_ri(struct SizedBuffer* output, sgxwasm_register_t dst, int64_t src,
           sgxwasm_valtype_t type)
{
  arithmetic_op_ri(output, 0x1, dst, src, type);
  return 1;
}

int
emit_xor_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
            sgxwasm_register_t src, sgxwasm_valtype_t type)
{
  if (type == VALTYPE_I64 && dst == src) {
    // 32 bit operations zero the top 32 bits of 64 bit registers. Therefore
    // there is no need to make this a 64 bit operation.
    arithmetic_op_rr(output, 0x33, dst, src, VALTYPE_I32);

  } else {
    arithmetic_op_rr(output, 0x33, dst, src, type);
  }
  return 1;
}

int
emit_xor_ri(struct SizedBuffer* output, sgxwasm_register_t dst, int64_t src,
            sgxwasm_valtype_t type)
{
  arithmetic_op_ri(output, 0x6, dst, src, type);
  return 1;
}

int
emit_add_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
            sgxwasm_register_t src, sgxwasm_valtype_t type)
{
  arithmetic_op_rr(output, 0x03, dst, src, type);
  return 1;
}

int
emit_add_ri(struct SizedBuffer* output, sgxwasm_register_t dst, int64_t src,
            sgxwasm_valtype_t type)
{
  arithmetic_op_ri(output, 0x00, dst, src, type);
  return 1;
}

int
emit_sub_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
            sgxwasm_register_t src, sgxwasm_valtype_t type)
{
  arithmetic_op_rr(output, 0x2b, dst, src, type);
  return 1;
}

int
emit_sub_ri(struct SizedBuffer* output, sgxwasm_register_t dst, int64_t src,
            sgxwasm_valtype_t type)
{
  arithmetic_op_ri(output, 0x05, dst, src, type);
  return 1;
}

int
emit_imul_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
             sgxwasm_register_t src, sgxwasm_valtype_t type)
{
  emit_rex_rr(output, dst, src, type, 1);
  emit(output, 0x0f);
  emit(output, 0xaf);
  emit_modrm_rr(output, dst, src);
  return 1;
}

int
emit_cmp_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
            sgxwasm_register_t src, sgxwasm_valtype_t type)
{
  arithmetic_op_rr(output, 0x3b, dst, src, type);
  return 1;
}

int
emit_cmp_ri(struct SizedBuffer* output, sgxwasm_register_t dst, int64_t src,
            sgxwasm_valtype_t type)
{
  arithmetic_op_ri(output, 0x7, dst, src, type);
  return 1;
}

int
emit_cmpb_ri(struct SizedBuffer* output, sgxwasm_register_t dst, int8_t src)
{
  arithmetic_op_ri_8(output, 0x7, dst, src);
  return 1;
}

int
emit_cdq(struct SizedBuffer* output)
{
  emit(output, 0x99);
  return 1;
}

int
emit_cqo(struct SizedBuffer* output)
{
  // REX prefix
  emit(output, 0x48);
  emit(output, 0x99);
  return 1;
}

int
emit_idiv_r(struct SizedBuffer* output, sgxwasm_register_t src,
            sgxwasm_valtype_t type)
{
  emit_rex_r(output, src, type, 1);
  emit(output, 0xf7);
  emit_modrm_cr(output, 0x7, src);
  return 1;
}

int
emit_div_r(struct SizedBuffer* output, sgxwasm_register_t src,
           sgxwasm_valtype_t type)
{
  emit_rex_r(output, src, type, 1);
  emit(output, 0xf7);
  emit_modrm_cr(output, 0x6, src);
  return 1;
}

int
emit_div_or_rem(struct SizedBuffer* output, sgxwasm_register_t dst,
                sgxwasm_register_t lhs, sgxwasm_register_t rhs,
                label_t* trap_div_by_zero, label_t* trap_div_unrepresentable,
                sgxwasm_valtype_t type, int_div_or_rem_t div_or_rem,
                val_signed_or_unsigned_t signed_or_unsigned)
{
  int count = 0;
  uint8_t needs_unrepresentable_check =
    signed_or_unsigned == VAL_SIGNED && div_or_rem == INT_DIV;
  uint8_t special_case_minus_1 =
    signed_or_unsigned == VAL_SIGNED && div_or_rem == INT_REM;
  assert(needs_unrepresentable_check == (trap_div_unrepresentable != NULL));

  // Make sure rax and rdx are no used.
  if (rhs == GP_RAX || rhs == GP_RDX) {
    count += emit_mov_rr(output, ScratchGP, rhs, type);
    rhs = ScratchGP;
  }

  count += emit_test_rr(output, rhs, rhs, type);
  count += emit_jcc(output, COND_EQ, trap_div_by_zero, Far);

  label_t done = { 0, 0 };
  if (needs_unrepresentable_check) {
    // Check for {kMinInt / -1}. This is unrepresentable.
    label_t do_div = { 0, 0 };
    count += emit_cmp_ri(output, rhs, -1, type);
    count += emit_jcc(output, COND_NE, &do_div, Far);
    // {lhs} is min int if {lhs - 1} overflows.
    count += emit_cmp_ri(output, lhs, 1, type);
    count += emit_jcc(output, COND_OVERFLOW, trap_div_unrepresentable, Far);
    bind_label(output, &do_div, pc_offset(output));
  } else if (special_case_minus_1) {
    // {lhs % -1} is always 0 (needs to be special cased because {kMinInt / -1}
    // cannot be computed).
    label_t do_rem = { 0, 0 };
    count += emit_cmp_ri(output, rhs, -1, type);
    count += emit_jcc(output, COND_NE, &do_rem, Far);
    count += emit_xor_rr(output, dst, dst, type);
    count += emit_jmp_label(output, &done, Far);
    bind_label(output, &do_rem, pc_offset(output));
  }

  // Now move {lhs} into {eax}, then zero-extend or sign-extend into {edx}, then
  // do the division.
  if (lhs != GP_RAX) {
    count += emit_mov_rr(output, GP_RAX, lhs, type);
  }
  if (type == VALTYPE_I32 && signed_or_unsigned == VAL_SIGNED) { // i32
    count += emit_cdq(output);
    count += emit_idiv_r(output, rhs, VALTYPE_I32);
  } else if (type == VALTYPE_I32 && signed_or_unsigned == VAL_UNSIGNED) { // u32
    count += emit_xor_rr(output, GP_RDX, GP_RDX, VALTYPE_I32);
    count += emit_div_r(output, rhs, VALTYPE_I32);
  } else if (type == VALTYPE_I64 && signed_or_unsigned == VAL_SIGNED) { // i64
    count += emit_cqo(output);
    count += emit_idiv_r(output, rhs, VALTYPE_I64);
  } else { // u64
    assert(type == VALTYPE_I64 && signed_or_unsigned == VAL_UNSIGNED);
    count += emit_xor_rr(output, GP_RDX, GP_RDX, VALTYPE_I64);
    count += emit_div_r(output, rhs, VALTYPE_I64);
  }

  // Move back the result (in {eax} or {edx}) into the {dst} register.
  sgxwasm_register_t result_reg = div_or_rem == INT_DIV ? GP_RAX : GP_RDX;
  if (dst != result_reg) {
    count += emit_mov_rr(output, dst, result_reg, type);
  }

  if (special_case_minus_1) {
    bind_label(output, &done, pc_offset(output));
  }

  return count;
}

int
emit_setcc(struct SizedBuffer* output, condition_t cc, sgxwasm_register_t reg)
{
  assert(is_uint4(cc));
  if (!is_byte_register(reg)) {
    // Register is not one of al, bl, cl, dl.  Its encoding needs REX.
    emit_rex_r(output, reg, VALTYPE_I32, 0);
  }
  emit(output, 0x0f);
  emit(output, 0x90 | cc);
  emit_modrm_cr(output, 0x0, reg);
  return 1;
}

int
emit_set_cond(struct SizedBuffer* output, condition_t cond,
              sgxwasm_register_t dst, sgxwasm_register_t lhs,
              sgxwasm_register_t rhs, sgxwasm_valtype_t type)
{
  int count = 0;
  count += emit_cmp_rr(output, lhs, rhs, type);
  count += emit_setcc(output, cond, dst);
  count += emit_movzxb_rr(output, dst, dst);
  return count;
}

// Bit operations

int
emit_btr_ri(struct SizedBuffer* output, sgxwasm_register_t dst, int8_t imm)
{
  emit_rex_r(output, dst, VALTYPE_I64, 1);
  emit(output, 0x0f);
  emit(output, 0xba);
  emit_modrm_cr(output, 0x6, dst);
  emit(output, imm);
  return 1;
}

int
emit_bsr_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
            sgxwasm_register_t src, sgxwasm_valtype_t type)
{
  emit_rex_rr(output, dst, src, type, 1);
  emit(output, 0x0f);
  emit(output, 0xbd);
  emit_modrm_rr(output, dst, src);
  return 1;
}

int
emit_bsf_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
            sgxwasm_register_t src, sgxwasm_valtype_t type)
{
  emit_rex_rr(output, dst, src, type, 1);
  emit(output, 0x0f);
  emit(output, 0xbc);
  emit_modrm_rr(output, dst, src);
  return 1;
}

// Shift operations

void
shift_r(struct SizedBuffer* output, sgxwasm_register_t dst,
        shift_code_t subcode, sgxwasm_valtype_t type)
{
  emit_rex_r(output, dst, type, 1);
  emit(output, 0xd3);
  emit_operand_cr(output, subcode, dst);
}

void
shift_ri(struct SizedBuffer* output, sgxwasm_register_t dst,
         int8_t shift_amount, shift_code_t subcode, sgxwasm_valtype_t type)
{
  if (shift_amount == 1) {
    emit_rex_r(output, dst, type, 1);
    emit(output, 0xd1);
    emit_modrm_cr(output, subcode, dst);
  } else {
    emit_rex_r(output, dst, type, 1);
    emit(output, 0xc1);
    emit_modrm_cr(output, subcode, dst);
    emit(output, shift_amount);
  }
}

int
emit_shr_r(struct SizedBuffer* output, sgxwasm_register_t dst,
           sgxwasm_valtype_t type)
{
  shift_r(output, dst, SHIFT_SHR, type);
  return 1;
}

int
emit_shr_ri(struct SizedBuffer* output, sgxwasm_register_t dst,
            int8_t shift_amount, sgxwasm_valtype_t type)
{
  shift_ri(output, dst, shift_amount, SHIFT_SHR, type);
  return 1;
}

int
emit_shl_r(struct SizedBuffer* output, sgxwasm_register_t dst,
           sgxwasm_valtype_t type)
{
  shift_r(output, dst, SHIFT_SHL, type);
  return 1;
}

int
emit_shl_ri(struct SizedBuffer* output, sgxwasm_register_t dst,
            int8_t shift_amount, sgxwasm_valtype_t type)
{
  shift_ri(output, dst, shift_amount, SHIFT_SHL, type);
  return 1;
}

int
emit_sar_r(struct SizedBuffer* output, sgxwasm_register_t dst,
           sgxwasm_valtype_t type)
{
  shift_r(output, dst, SHIFT_SAR, type);
  return 1;
}

int
emit_sar_ri(struct SizedBuffer* output, sgxwasm_register_t dst,
            int8_t shift_amount, sgxwasm_valtype_t type)
{
  shift_ri(output, dst, shift_amount, SHIFT_SAR, type);
  return 1;
}

int
emit_ror_r(struct SizedBuffer* output, sgxwasm_register_t dst,
           sgxwasm_valtype_t type)
{
  shift_r(output, dst, SHIFT_ROR, type);
  return 1;
}

int
emit_ror_ri(struct SizedBuffer* output, sgxwasm_register_t dst,
            int8_t shift_amount, sgxwasm_valtype_t type)
{
  shift_ri(output, dst, shift_amount, SHIFT_ROR, type);
  return 1;
}

int
emit_rol_r(struct SizedBuffer* output, sgxwasm_register_t dst,
           sgxwasm_valtype_t type)
{
  shift_r(output, dst, SHIFT_ROL, type);
  return 1;
}

int
emit_rol_ri(struct SizedBuffer* output, sgxwasm_register_t dst,
            int8_t shift_amount, sgxwasm_valtype_t type)
{
  shift_ri(output, dst, shift_amount, SHIFT_ROL, type);
  return 1;
}

int
emit_test_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
             sgxwasm_register_t src, sgxwasm_valtype_t type)
{
  if (low_bits(src) == 4) {
    sgxwasm_register_t tmp = src;
    src = dst;
    dst = tmp;
  }
  emit_rex_rr(output, dst, src, type, 1);
  emit(output, 0x85);
  emit_modrm_rr(output, dst, src);
  return 1;
}

int
emit_test_ri(struct SizedBuffer* output, sgxwasm_register_t reg, int64_t mask,
             sgxwasm_valtype_t type)
{
  uint8_t byte = 0;
  if (is_uint8(mask)) {
    byte = sizeof(int8_t);
  } else if (is_uint16(mask)) {
    byte = sizeof(int16_t);
  }
  uint8_t half_word = byte == sizeof(int16_t);
  if (half_word) {
    emit(output, 0x66);
    type = VALTYPE_I32;
  }
  uint8_t byte_operand = byte == sizeof(int8_t);
  if (byte_operand) {
    type = VALTYPE_I32;
    if (!is_byte_register(reg)) {
      emit_rex_r(output, reg, VALTYPE_I32, 0);
    }
  } else {
    emit_rex_r(output, reg, type, 1);
  }
  if (reg == GP_RAX) {
    emit(output, byte_operand ? 0xa8 : 0xa9);
  } else {
    emit(output, byte_operand ? 0xf6 : 0xf7);
    emit_modrm_cr(output, 0x0, reg);
  }
  if (byte_operand) {
    emit(output, (uint8_t)mask);
  } else if (half_word) {
    emit_imm(output, mask, IMM_2_BYTE);
  } else {
    emit_imm(output, mask, get_num_bytes(mask));
  }
  return 1;
}

int
emit_lea_rm(struct SizedBuffer* output, sgxwasm_register_t dst,
            struct Operand* src, sgxwasm_valtype_t type)
{
  emit_rex_rm(output, dst, src, type, 1);
  emit(output, 0x8d);
  emit_operand_rm(output, dst, src);
  return 1;
}

int
emit_call_r(struct SizedBuffer* output, sgxwasm_valtype_t addr)
{
  emit_rex_r(output, addr, VALTYPE_I32, 1);
  emit(output, 0xff);
  emit_modrm_cr(output, 0x2, addr);
  return 1;
}

// Moves.

int
emit_movl_ri(struct SizedBuffer* output, sgxwasm_register_t dst, int32_t imm)
{
  emit_rex_r(output, dst, VALTYPE_I32, 1);
  emit(output, 0xb8 + low_bits(dst));
  emit_imm(output, (int64_t)imm, IMM_4_BYTE);
  return 1;
}

int
emit_movq_ri(struct SizedBuffer* output, sgxwasm_register_t dst, int64_t imm)
{

  uint8_t byte;
  byte = get_num_bytes((uint64_t)imm);
  emit_rex_r(output, dst, VALTYPE_I64, 1);
  if (byte == IMM_8_BYTE) {
    emit(output, 0xb8 + low_bits(dst));
    emit_imm(output, imm, IMM_8_BYTE);
  } else {
    emit(output, 0xc7);
    emit_modrm_cr(output, 0x0, dst);
    emit_imm(output, imm, IMM_4_BYTE);
  }
  return 1;
}

int
emit_mov_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
            sgxwasm_register_t src, sgxwasm_valtype_t type)
{
  assert(dst < FPGroupOffset && src < FPGroupOffset);
  if (low_bits(src) == 4) {
    emit_rex_rr(output, src, dst, type, 1);
    emit(output, 0x89);
    emit_modrm_rr(output, src, dst);
  } else {
    emit_rex_rr(output, dst, src, type, 1);
    emit(output, 0x8b);
    emit_modrm_rr(output, dst, src);
  }
  return 1;
}

int
emit_mov_rm(struct SizedBuffer* output, sgxwasm_register_t dst,
            struct Operand* src, sgxwasm_valtype_t type)
{
  emit_rex_rm(output, dst, src, type, 1);
  emit(output, 0x8b);
  emit_operand_rm(output, dst, src);
  return 1;
}

int
emit_mov_mr(struct SizedBuffer* output, struct Operand* dst,
            sgxwasm_register_t src, sgxwasm_valtype_t type)
{
  emit_rex_rm(output, src, dst, type, 1);
  emit(output, 0x89);
  emit_operand_rm(output, src, dst);
  return 1;
}

int
emit_mov_mi(struct SizedBuffer* output, struct Operand* dst, int32_t imm,
            sgxwasm_valtype_t type)
{
  emit_rex_m(output, dst, type, 1);
  emit(output, 0xc7);
  emit_operand_cm(output, 0x0, dst);
  emit_imm(output, (int64_t)imm, IMM_4_BYTE);
  return 1;
}

int
emit_movb_mr(struct SizedBuffer* output, struct Operand* dst,
             sgxwasm_register_t src)
{
  if (!is_byte_register(src)) {
    emit_rex_rm(output, src, dst, VALTYPE_I32, 0);
  } else {
    emit_rex_rm(output, src, dst, VALTYPE_I32, 1);
  }
  emit(output, 0x88);
  emit_operand_rm(output, src, dst);
  return 1;
}

int
emit_movb_rm(struct SizedBuffer* output, sgxwasm_register_t dst,
             struct Operand* src)
{
  if (!is_byte_register(dst)) {
    emit_rex_rm(output, dst, src, VALTYPE_I32, 0);
  } else {
    emit_rex_rm(output, dst, src, VALTYPE_I32, 1);
  }
  emit(output, 0x8a);
  emit_operand_rm(output, dst, src);
  return 1;
}

int
emit_movw_mr(struct SizedBuffer* output, struct Operand* dst,
             sgxwasm_register_t src)
{
  emit(output, 0x66);
  emit_rex_rm(output, src, dst, VALTYPE_I32, 1);
  emit(output, 0x89);
  emit_operand_rm(output, src, dst);
  return 1;
}

int
emit_movzxb_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
               sgxwasm_register_t src)
{
  if (!is_byte_register(src)) {
    emit_rex_rr(output, dst, src, VALTYPE_I32, 0);
  } else {
    emit_rex_rr(output, dst, src, VALTYPE_I32, 1);
  }
  emit(output, 0x0f);
  emit(output, 0xb6);
  emit_modrm_rr(output, dst, src);
  return 1;
}

int
emit_movzxb_rm(struct SizedBuffer* output, sgxwasm_register_t dst,
               struct Operand* src)
{
  emit_rex_rm(output, dst, src, VALTYPE_I32, 1);
  emit(output, 0x0f);
  emit(output, 0xb6);
  emit_operand_rm(output, dst, src);
  return 1;
}

int
emit_movzxw_rm(struct SizedBuffer* output, sgxwasm_register_t dst,
               struct Operand* src)
{
  emit_rex_rm(output, dst, src, VALTYPE_I32, 1);
  emit(output, 0x0f);
  emit(output, 0xb7);
  emit_operand_rm(output, dst, src);
  return 1;
}

int
emit_movsxb_rm(struct SizedBuffer* output, sgxwasm_register_t dst,
               struct Operand* src, sgxwasm_valtype_t type)
{
  emit_rex_rm(output, dst, src, type, 1);
  emit(output, 0x0f);
  emit(output, 0xbe);
  emit_operand_rm(output, dst, src);
  return 1;
}

int
emit_movsxw_rm(struct SizedBuffer* output, sgxwasm_register_t dst,
               struct Operand* src, sgxwasm_valtype_t type)
{
  emit_rex_rm(output, dst, src, type, 1);
  emit(output, 0x0f);
  emit(output, 0xbf);
  emit_operand_rm(output, dst, src);
  return 1;
}

int
emit_movsxlq_rm(struct SizedBuffer* output, sgxwasm_register_t dst,
                struct Operand* src)
{
  emit_rex_rm(output, dst, src, VALTYPE_I64, 1);
  emit(output, 0x63);
  emit_operand_rm(output, dst, src);
  return 1;
}

int
emit_movsxlq_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                sgxwasm_register_t src)
{
  emit_rex_rr(output, dst, src, VALTYPE_I64, 1);
  emit(output, 0x63);
  emit_modrm_rr(output, dst, src);
  return 1;
}

int
emit_subq_sp_32(struct SizedBuffer* output, uint32_t imm)
{
  emit_rex_r(output, GP_RAX, VALTYPE_I64, 1); // emit 0x48
  emit(output, 0x81);
  emit_modrm_rr(output, 0x5, GP_RSP);
  emit_imm(output, (int64_t)imm, IMM_4_BYTE);
  return 1;
}

// SSE operations.

void
sse2_instr_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src, uint8_t prefix, uint8_t escape,
              uint8_t opcode)
{
  emit(output, prefix);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, escape);
  emit(output, opcode);
  emit_sse_operand_rr(output, dst, src);
}

int
emit_ucomiss_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                sgxwasm_register_t src)
{
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x2e);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_ucomisd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                sgxwasm_register_t src)
{
  emit(output, 0x66);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x2e);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_andps_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src)
{
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x54);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_andpd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src)
{
  emit(output, 0x66);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x54);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_xorps_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src)
{
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x57);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_xorpd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src)
{
  emit(output, 0x66);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x57);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_pcmpeqd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                sgxwasm_register_t src)
{
  sse2_instr_rr(output, dst, src, 0x66, 0x0f, 0x76);
  return 1;
}

int
emit_pslld_ri(struct SizedBuffer* output, sgxwasm_register_t dst, uint8_t imm)
{
  emit(output, 0x66);
  emit_rex_r(output, dst, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x72);
  emit_sse_operand_rr(output, GP_RSI, dst); // rsi == 6
  emit(output, imm);
  return 1;
}

int
emit_psrld_ri(struct SizedBuffer* output, sgxwasm_register_t dst, uint8_t imm)
{
  emit(output, 0x66);
  emit_rex_r(output, dst, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x72);
  emit_sse_operand_rr(output, GP_RDX, dst); // rdx == 2
  emit(output, imm);
  return 1;
}

int
emit_psllq_ri(struct SizedBuffer* output, sgxwasm_register_t dst, uint8_t imm)
{
  emit(output, 0x66);
  emit_rex_r(output, dst, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x73);
  emit_sse_operand_rr(output, GP_RSI, dst); // rsi == 6
  emit(output, imm);
  return 1;
}

int
emit_psrlq_ri(struct SizedBuffer* output, sgxwasm_register_t dst, uint8_t imm)
{
  emit(output, 0x66);
  emit_rex_r(output, dst, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x73);
  emit_sse_operand_rr(output, GP_RDX, dst); // rdx == 2
  emit(output, imm);
  return 1;
}

int
emit_addss_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src)
{
  emit(output, 0xf3);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x58);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_subss_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src)
{
  emit(output, 0xf3);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x5c);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_divss_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src)
{
  emit(output, 0xf3);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x5e);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_mulss_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src)
{
  emit(output, 0xf3);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x59);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_addsd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src)
{
  emit(output, 0xf2);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x58);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_subsd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src)
{
  emit(output, 0xf2);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x5c);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_divsd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src)
{
  emit(output, 0xf2);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x5e);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_mulsd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src)
{
  emit(output, 0xf2);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x59);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_roundss_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                sgxwasm_register_t src, rounding_mode_t mode)
{
  emit(output, 0x66);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x3a);
  emit(output, 0x0a);
  emit_sse_operand_rr(output, dst, src);
  // Mask precision exception.
  emit(output, (uint8_t)mode | 0x8);
  return 1;
}

int
emit_roundsd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                sgxwasm_register_t src, rounding_mode_t mode)
{
  emit(output, 0x66);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x3a);
  emit(output, 0x0b);
  emit_sse_operand_rr(output, dst, src);
  // Mask precision exception.
  emit(output, (uint8_t)mode | 0x8);
  return 1;
}

int
emit_sqrtss_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
               sgxwasm_register_t src)
{
  emit(output, 0xf3);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x51);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_sqrtsd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
               sgxwasm_register_t src)
{
  emit(output, 0xf2);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x51);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_sse_movd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                 sgxwasm_register_t src)
{
  // dst: GP, src: FP
  if (dst < FPGroupOffset && src >= FPGroupOffset) {
    emit(output, 0x66);
    emit_rex_rr(output, src, dst, VALTYPE_F32, 1);
    emit(output, 0x0f);
    emit(output, 0x7e);
    emit_sse_operand_rr(output, src, dst);
  } else // dst: FP, src: GP
    if (dst >= FPGroupOffset && src < FPGroupOffset) {
    emit(output, 0x66);
    emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
    emit(output, 0x0f);
    emit(output, 0x6e);
    emit_sse_operand_rr(output, dst, src);
  }
  return 1;
}

int
emit_sse_movq_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                 sgxwasm_register_t src)
{
  // dst: GP, src: FP
  if (dst < FPGroupOffset && src >= FPGroupOffset) {
    emit(output, 0x66);
    emit_rex_rr(output, src, dst, VALTYPE_F64, 1);
    emit(output, 0x0f);
    emit(output, 0x7e);
    emit_sse_operand_rr(output, src, dst);
  } else // dst: FP, src: GP
    if (dst >= FPGroupOffset && src < FPGroupOffset) {
    emit(output, 0x66);
    emit_rex_rr(output, dst, src, VALTYPE_F64, 1);
    emit(output, 0x0f);
    emit(output, 0x6e);
    emit_sse_operand_rr(output, dst, src);
  }
  return 1;
}

int
emit_movss_rm(struct SizedBuffer* output, sgxwasm_register_t dst,
              struct Operand* src)
{
  emit(output, 0xf3); // single
  emit_rex_rm(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x10); // load
  emit_sse_operand_rm(output, dst, src);
  return 1;
}

int
emit_movss_mr(struct SizedBuffer* output, struct Operand* dst,
              sgxwasm_register_t src)
{
  emit(output, 0xf3); // single
  emit_rex_rm(output, src, dst, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x11); // store
  emit_sse_operand_rm(output, src, dst);
  return 1;
}

int
emit_movss_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src)
{
  assert(dst >= FPGroupOffset && dst < RegisterNum && src >= FPGroupOffset &&
         dst < RegisterNum);
  emit(output, 0xf3); // single
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x10); // load
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_movsd_rm(struct SizedBuffer* output, sgxwasm_register_t dst,
              struct Operand* src)
{
  emit(output, 0xf2); // double
  emit_rex_rm(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x10); // load
  emit_sse_operand_rm(output, dst, src);
  return 1;
}

int
emit_movsd_mr(struct SizedBuffer* output, struct Operand* dst,
              sgxwasm_register_t src)
{
  emit(output, 0xf2); // double
  emit_rex_rm(output, src, dst, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x11); // store
  emit_sse_operand_rm(output, src, dst);
  return 1;
}

int
emit_movsd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src)
{
  emit(output, 0xf2); // double
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x10); // load
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_movmskps_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                 sgxwasm_register_t src)
{
  assert(is_gp(dst) && is_fp(src));
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x50);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_movmskpd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                 sgxwasm_register_t src)
{
  assert(is_gp(dst) && is_fp(src));
  emit(output, 0x66);
  emit_rex_rr(output, dst, src, VALTYPE_F32, 1);
  emit(output, 0x0f);
  emit(output, 0x50);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_cvttss2si_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                  sgxwasm_register_t src)
{
  assert(is_gp(dst) && is_fp(src));
  emit(output, 0xf3);
  emit_rex_rr(output, dst, src, VALTYPE_I32, 1);
  emit(output, 0x0f);
  emit(output, 0x2c);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_cvttsd2si_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                  sgxwasm_register_t src)
{
  assert(is_gp(dst) && is_fp(src));
  emit(output, 0xf2);
  emit_rex_rr(output, dst, src, VALTYPE_I32, 1);
  emit(output, 0x0f);
  emit(output, 0x2c);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_cvttss2siq_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                   sgxwasm_register_t src)
{
  assert(is_gp(dst) && is_fp(src));
  emit(output, 0xf3);
  emit_rex_rr(output, dst, src, VALTYPE_I64, 1);
  emit(output, 0x0f);
  emit(output, 0x2c);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_cvttsd2siq_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                   sgxwasm_register_t src)
{
  assert(is_gp(dst) && is_fp(src));
  emit(output, 0xf2);
  emit_rex_rr(output, dst, src, VALTYPE_I64, 1);
  emit(output, 0x0f);
  emit(output, 0x2c);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_cvtlsi2ss_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                  sgxwasm_register_t src)
{
  assert(is_fp(dst) && is_gp(src));
  emit(output, 0xf3);
  emit_rex_rr(output, dst, src, VALTYPE_I32, 1);
  emit(output, 0x0f);
  emit(output, 0x2a);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_cvtlsi2sd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                  sgxwasm_register_t src)
{
  assert(is_fp(dst) && is_gp(src));
  emit(output, 0xf2);
  emit_rex_rr(output, dst, src, VALTYPE_I32, 1);
  emit(output, 0x0f);
  emit(output, 0x2a);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_cvtqsi2ss_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                  sgxwasm_register_t src)
{
  assert(is_fp(dst) && is_gp(src));
  emit(output, 0xf3);
  emit_rex_rr(output, dst, src, VALTYPE_I64, 1);
  emit(output, 0x0f);
  emit(output, 0x2a);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_cvtqsi2sd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                  sgxwasm_register_t src)
{
  assert(is_fp(dst) && is_gp(src));
  emit(output, 0xf2);
  emit_rex_rr(output, dst, src, VALTYPE_I64, 1);
  emit(output, 0x0f);
  emit(output, 0x2a);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_cvtsd2ss_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                 sgxwasm_register_t src)
{
  assert(is_fp(dst) && is_fp(src));
  emit(output, 0xf2);
  emit_rex_rr(output, dst, src, VALTYPE_I32, 1);
  emit(output, 0x0f);
  emit(output, 0x5a);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_cvtss2sd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                 sgxwasm_register_t src)
{
  assert(is_fp(dst) && is_fp(src));
  emit(output, 0xf3);
  emit_rex_rr(output, dst, src, VALTYPE_I32, 1);
  emit(output, 0x0f);
  emit(output, 0x5a);
  emit_sse_operand_rr(output, dst, src);
  return 1;
}

int
emit_cvtqui2ss_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                  sgxwasm_register_t src)
{
  assert(is_fp(dst) && is_gp(src));
  int count = 0;
  label_t done = { 0, 0 };
  count += emit_cvtqsi2ss_rr(output, dst, src);
  count += emit_test_rr(output, src, src, VALTYPE_I64);
  count += emit_jcc(output, COND_POSITIVE, &done, Near);

  // Compute {src/2 | (src&1)} (retain the LSB to avoid rounding errors).
  if (src != ScratchGP) {
    count += emit_mov_rr(output, ScratchGP, src, VALTYPE_I64);
  }
  count += emit_shr_ri(output, ScratchGP, 1, VALTYPE_I64);
  // The LSB is shifted into CF. If it is set, set the LSB in {tmp}.
  label_t msb_not_set = { 0, 0 };
  count += emit_jcc(output, COND_GE_U, &msb_not_set, Near);
  count += emit_or_ri(output, ScratchGP, 1, VALTYPE_I64);
  bind_label(output, &msb_not_set, pc_offset(output));
  count += emit_cvtqsi2ss_rr(output, dst, ScratchGP);
  count += emit_addss_rr(output, dst, dst);
  bind_label(output, &done, pc_offset(output));
  return count;
}

int
emit_cvtqui2sd_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
                  sgxwasm_register_t src)
{
  assert(is_fp(dst) && is_gp(src));
  int count = 0;
  label_t done = { 0, 0 };
  count += emit_cvtqsi2sd_rr(output, dst, src);
  count += emit_test_rr(output, src, src, VALTYPE_I64);
  count += emit_jcc(output, COND_POSITIVE, &done, Near);

  // Compute {src/2 | (src&1)} (retain the LSB to avoid rounding errors).
  if (src != ScratchGP) {
    count += emit_mov_rr(output, ScratchGP, src, VALTYPE_I64);
  }
  count += emit_shr_ri(output, ScratchGP, 1, VALTYPE_I64);
  // The LSB is shifted into CF. If it is set, set the LSB in {tmp}.
  label_t msb_not_set = { 0, 0 };
  count += emit_jcc(output, COND_GE_U, &msb_not_set, Near);
  count += emit_or_ri(output, ScratchGP, 1, VALTYPE_I64);
  bind_label(output, &msb_not_set, pc_offset(output));
  count += emit_cvtqsi2sd_rr(output, dst, ScratchGP);
  count += emit_addsd_rr(output, dst, dst);
  bind_label(output, &done, pc_offset(output));
  return count;
}

int
emit_float_min_or_max(struct SizedBuffer* output, sgxwasm_register_t dst,
                      sgxwasm_register_t lhs, sgxwasm_register_t rhs,
                      float_min_or_max_t min_or_max, sgxwasm_valtype_t type)
{
  int count = 0;
  label_t is_nan = { 0, 0 };
  label_t lhs_below_rhs = { 0, 0 };
  label_t lhs_above_rhs = { 0, 0 };
  label_t done = { 0, 0 };

  // Check the easy cases first: nan (e.g. unordered), smaller and greater.
  // NaN has to be checked first, because PF=1 implies CF=1.
  if (type == VALTYPE_F32) {
    count += emit_ucomiss_rr(output, lhs, rhs);
  } else {
    assert(type == VALTYPE_F64);
    count += emit_ucomisd_rr(output, lhs, rhs);
  }
  count += emit_jcc(output, COND_EVEN, &is_nan, Near);        // PF = 1
  count += emit_jcc(output, COND_LT_U, &lhs_below_rhs, Near); // CF = 1
  count +=
    emit_jcc(output, COND_GT_U, &lhs_above_rhs, Near); // CF = 0 && ZF = 0

  // If we get here, then either
  // a) {lhs == rhs},
  // b) {lhs == -0.0} and {rhs == 0.0}, or
  // c) {lhs == 0.0} and {rhs == -0.0}.
  // For a), it does not matter whether we return {lhs} or {rhs}. Check the sign
  // bit of {rhs} to differentiate b) and c).
  if (type == VALTYPE_F32) {
    count += emit_movmskps_rr(output, ScratchGP, rhs);
  } else {
    assert(type == VALTYPE_F64);
    count += emit_movmskpd_rr(output, ScratchGP, rhs);
  }
  count += emit_test_ri(output, ScratchGP, 1, VALTYPE_I32);
  count += emit_jcc(output, COND_EQ, &lhs_below_rhs, Near);
  count += emit_jmp_label(output, &lhs_above_rhs, Near);

  bind_label(output, &is_nan, pc_offset(output));
  // Create a NaN output.
  if (type == VALTYPE_F32) {
    count += emit_xorps_rr(output, dst, dst);
    count += emit_divss_rr(output, dst, dst);
  } else {
    assert(type == VALTYPE_F64);
    count += emit_xorpd_rr(output, dst, dst);
    count += emit_divsd_rr(output, dst, dst);
  }
  count += emit_jmp_label(output, &done, Near);

  bind_label(output, &lhs_below_rhs, pc_offset(output));
  sgxwasm_register_t lhs_below_rhs_src = min_or_max == FLOAT_MIN ? lhs : rhs;
  if (dst != lhs_below_rhs_src) {
    if (type == VALTYPE_F32) {
      count += emit_movss_rr(output, dst, lhs_below_rhs_src);
    } else {
      assert(type == VALTYPE_F64);
      count += emit_movsd_rr(output, dst, lhs_below_rhs_src);
    }
  }
  count += emit_jmp_label(output, &done, Near);

  bind_label(output, &lhs_above_rhs, pc_offset(output));
  sgxwasm_register_t lhs_above_rhs_src = min_or_max == FLOAT_MIN ? rhs : lhs;
  if (dst != lhs_above_rhs_src) {
    if (type == VALTYPE_F32) {
      count += emit_movss_rr(output, dst, lhs_above_rhs_src);
    } else {
      assert(type == VALTYPE_F64);
      count += emit_movsd_rr(output, dst, lhs_above_rhs_src);
    }
  }

  bind_label(output, &done, pc_offset(output));
  return count;
}

int
emit_float_set_cond(struct SizedBuffer* output, condition_t cond,
                    sgxwasm_register_t dst, sgxwasm_register_t lhs,
                    sgxwasm_register_t rhs, sgxwasm_valtype_t type)
{
  int count = 0;
  label_t cont = { 0, 0 };
  label_t not_non = { 0, 0 };

  if (type == VALTYPE_F32) {
    count += emit_ucomiss_rr(output, lhs, rhs);
  } else {
    assert(type == VALTYPE_F64);
    count += emit_ucomisd_rr(output, lhs, rhs);
  }
  // If PF is one, one of the operands was NaN. This needs special handling.
  count += emit_jcc(output, COND_ODD, &not_non, Near);
  // Return 1 for f32.ne, 0 for all other cases.
  if (cond == COND_NE) {
    count += emit_movl_ri(output, dst, 1);
  } else {
    count += emit_xor_rr(output, dst, dst, VALTYPE_I32);
  }
  count += emit_jmp_label(output, &cont, Near);
  bind_label(output, &not_non, pc_offset(output));

  count += emit_setcc(output, cond, dst);
  count += emit_movzxb_rr(output, dst, dst);
  bind_label(output, &cont, pc_offset(output));
  return count;
}

// Jump

int
emit_jmp_label(struct SizedBuffer* output, label_t* label, distance_t distance)
{
  if (is_bound(label)) {
    int offset = label_pos(label) - pc_offset(output) - 1;
    /*printf("offset: %d, label pos: %d, pc_offset: %lu\n",
           offset,
           label_pos(label),
           pc_offset(output));*/
    if (is_int8((offset - JmpShortSize))) {
      emit(output, 0xeb);
      emit(output, (offset - JmpShortSize) & 0xff);
    } else {
      emit(output, 0xe9);
      emit_imm(output, (int64_t)(offset - JmpLongSize), IMM_4_BYTE);
    }
  } else if (distance == Near) {
    emit(output, 0xeb);
    uint8_t disp = 0x00;
    if (is_near_linked(label)) {
      int offset = near_link_pos(label) - pc_offset(output);
      assert(is_int8(offset));
      disp = (uint8_t)(offset & 0xff);
    }
    link_to(label, pc_offset(output), Near);
    emit(output, disp);
  } else {
    if (is_linked(label)) {
      emit(output, 0xe9);
      emit_imm(output, (int64_t)label_pos(label), IMM_4_BYTE);
      link_to(label, pc_offset(output) - JmpLongSize, Far);
#if DEBUG_ASSEMBLE
      printf("linked label, pos: %d\n", label_pos(label));
#endif
    } else {
      assert(is_unused(label));
      emit(output, 0xe9);
      int32_t current = pc_offset(output);
      emit_imm(output, (int64_t)current, IMM_4_BYTE);
      link_to(label, current, Far);
#if DEBUG_ASSEMBLE
      printf("unused label, pos: %d\n", label_pos(label));
#endif
    }
  }
  return 1;
}

int
emit_jmp_16(struct SizedBuffer* output, int8_t imm)
{
  emit(output, 0xeb);
  emit(output, imm);
  return 1;
}

int
emit_jmp_32(struct SizedBuffer* output, int32_t imm)
{
  emit(output, 0xe9);
  emit_imm(output, (int64_t)imm, IMM_4_BYTE);
  return 1;
}

int
emit_jmp_r(struct SizedBuffer* output, sgxwasm_register_t target)
{
  emit_rex_r(output, target, VALTYPE_I32, 1);
  emit(output, 0xff);
  emit_modrm_cr(output, 0x04, target);
  return 1;
}

int
emit_jcc(struct SizedBuffer* output, condition_t cond, label_t* label,
         distance_t distance)
{
  assert(is_uint4(cond));
  if (is_bound(label)) {
    int offset = label_pos(label) - pc_offset(output);

    if (is_int8((offset - JccShortSize))) {
      emit(output, 0x70 | cond);
      emit(output, (offset - JccShortSize) & 0xff);
    } else {
      emit(output, 0x0f);
      emit(output, 0x80 | cond);
      emit_imm(output, (int64_t)(offset - JccLongSize), IMM_4_BYTE);
    }
  } else if (distance == Near) {
    emit(output, 0x70 | cond);
    uint8_t disp = 0x00;
    if (is_near_linked(label)) {
      int offset = near_link_pos(label) - pc_offset(output);
      // printf("offset: %x (%d)\n", offset, offset);
      assert(is_int8(offset));
      disp = (uint8_t)(offset & 0xff);
    }
    link_to(label, pc_offset(output), Near);
    emit(output, disp);
  } else {
    if (is_linked(label)) {
      emit(output, 0x0f);
      emit(output, 0x80 | cond);
      emit_imm(output, (int64_t)label_pos(label), IMM_4_BYTE);
      link_to(label, pc_offset(output) - JmpLongSize, Far);
    } else {
      assert(is_unused(label));
      emit(output, 0x0f);
      emit(output, 0x80 | cond);
      int32_t current = pc_offset(output);
      emit_imm(output, (int64_t)current, IMM_4_BYTE);
      link_to(label, current, Far);
    }
  }
  return 1;
}

int
emit_cond_jump_rr(struct SizedBuffer* output, condition_t cond, label_t* label,
                  sgxwasm_valtype_t type, sgxwasm_register_t lhs,
                  sgxwasm_register_t rhs)
{
  int count = 0;
  if (rhs != REG_UNKNOWN) {
    switch (type) {
      case VALTYPE_I32:
        count += emit_cmp_rr(output, lhs, rhs, VALTYPE_I32);
        break;
      case VALTYPE_I64:
        count += emit_cmp_rr(output, lhs, rhs, VALTYPE_I32);
        break;
      default:
        assert(0);
    }
  } else {
    count += emit_test_rr(output, lhs, lhs, VALTYPE_I32);
  }
  count += emit_jcc(output, cond, label, Far);
  return count;
}

int
emit_popcnt_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
               sgxwasm_register_t src, sgxwasm_valtype_t type)
{
  emit(output, 0xf3);
  if (type == VALTYPE_I64) {
    emit_rex_rr(output, dst, src, VALTYPE_I64, 0);
  } else {
    assert(type == VALTYPE_I32);
    emit_rex_rr(output, dst, src, VALTYPE_I32, 1);
  }
  emit(output, 0x0f);
  emit(output, 0xb8);
  emit_modrm_rr(output, dst, src);
  return 5;
}

int
emit_tzcnt_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src, sgxwasm_valtype_t type)
{
  emit(output, 0xf3);
  if (type == VALTYPE_I64) {
    emit_rex_rr(output, dst, src, VALTYPE_I64, 0);
  } else {
    assert(type == VALTYPE_I32);
    emit_rex_rr(output, dst, src, VALTYPE_I32, 1);
  }
  emit(output, 0x0f);
  emit(output, 0xbc);
  emit_modrm_rr(output, dst, src);
  return 5;
}

int
emit_lzcnt_rr(struct SizedBuffer* output, sgxwasm_register_t dst,
              sgxwasm_register_t src, sgxwasm_valtype_t type)
{
  emit(output, 0xf3);
  if (type == VALTYPE_I64) {
    emit_rex_rr(output, dst, src, VALTYPE_I64, 0);
  } else {
    assert(type == VALTYPE_I32);
    emit_rex_rr(output, dst, src, VALTYPE_I32, 1);
  }
  emit(output, 0x0f);
  emit(output, 0xbd);
  emit_modrm_rr(output, dst, src);
  return 5;
}

// Intel TSX

int
emit_xbegin(struct SizedBuffer* output, label_t* label)
{
  const int XBEGIN_OFFSET = 6;
  int offset = pc_offset(output);
  emit(output, 0xc7);
  emit(output, 0xf8);
  if (is_bound(label)) {
    offset = label_pos(label) - offset;
    emit_imm(output, (int64_t)(offset - XBEGIN_OFFSET), IMM_4_BYTE);
  } else {
    if (is_linked(label)) {
      emit_imm(output, (int64_t)label_pos(label), IMM_4_BYTE);
    } else {
      assert(is_unused(label));
      int32_t current = pc_offset(output);
      emit_imm(output, (int64_t)current, IMM_4_BYTE);
      link_to(label, current, Far);
    }
  }
  return 1;
}

int
emit_xend(struct SizedBuffer* output)
{
  emit(output, 0x0f);
  emit(output, 0x01);
  emit(output, 0xd5);
  return 1;
}

int
emit_std(struct SizedBuffer* output)
{
  emit(output, 0xfd);
  return 1;
}

int
emit_cld(struct SizedBuffer* output)
{
  emit(output, 0xfc);
  return 1;
}

int
emit_rep_stosq(struct SizedBuffer* output)
{
  emit(output, 0xf3);
  emit(output, 0x48);
  emit(output, 0xab);
  return 1;
}

int
emit_pushfd(struct SizedBuffer* output)
{
  emit(output, 0x9c);
  return 1;
}

int
emit_popfd(struct SizedBuffer* output)
{
  emit(output, 0x9d);
  return 1;
}

int
emit_lfence(struct SizedBuffer* output)
{
  emit(output, 0x0f);
  emit(output, 0xae);
  emit(output, 0xe8);
  return 3;
}

// High-level APIs.

int
Move(struct SizedBuffer* output, sgxwasm_register_t dst, sgxwasm_register_t src,
     sgxwasm_valtype_t type)
{
  assert(dst != src);
  switch (type) {
    case VALTYPE_I32: {
      assert(is_gp(dst) && is_gp(src));
      emit_mov_rr(output, dst, src, VALTYPE_I32);
      break;
    }
    case VALTYPE_I64: {
      assert(is_gp(dst) && is_gp(src));
      emit_mov_rr(output, dst, src, VALTYPE_I64);
      break;
    }
    case VALTYPE_F32: {
      assert(is_fp(dst) && is_fp(src));
      emit_movss_rr(output, dst, src);
      break;
    }
    case VALTYPE_F64: {
      assert(is_fp(dst) && is_fp(src));
      emit_movsd_rr(output, dst, src);
      break;
    }
  }
  return 1;
}

int
LoadConstant(struct SizedBuffer* output, sgxwasm_register_t reg, int64_t val,
             sgxwasm_valtype_t type)
{
  assert(reg < RegisterNum);
  int count = 0;
  switch (type) {
    case VALTYPE_I32:
      if (val == 0) {
        count += emit_xor_rr(output, reg, reg, VALTYPE_I32);
      } else {
        count += emit_movl_ri(output, reg, (int32_t)val);
      }
      break;
    case VALTYPE_I64:
      if (val == 0) {
        count += emit_xor_rr(output, reg, reg, VALTYPE_I64);
      } else {
        count += emit_movq_ri(output, reg, val);
      }
      break;
    case VALTYPE_F32:
      if (val == 0) {
        count += emit_xorps_rr(output, reg, reg);
      } else {
        uint32_t nlz = count_leading_zeros(val, IMM_4_BYTE * 8);
        uint32_t ntz = count_trailing_zeros(val, IMM_4_BYTE);
        uint32_t pop = count_population(val, IMM_4_BYTE);
        if (pop + ntz + nlz == 32) {
          // Set reg to all ones
          count += emit_pcmpeqd_rr(output, reg, reg);
          if (ntz)
            count += emit_pslld_ri(output, reg, (uint8_t)(ntz + nlz));
          if (nlz)
            count += emit_psrld_ri(output, reg, (uint8_t)nlz);
        } else {
          count += emit_movl_ri(output, ScratchGP, (int32_t)val);
          count += emit_sse_movd_rr(output, reg, ScratchGP);
        }
      }
      break;
    case VALTYPE_F64:
      if (val == 0) {
        count += emit_xorpd_rr(output, reg, reg);
      } else {
        uint32_t nlz = count_leading_zeros(val, IMM_8_BYTE * 8);
        uint32_t ntz = count_trailing_zeros(val, IMM_8_BYTE);
        uint32_t pop = count_population(val, IMM_8_BYTE);
        if (pop + ntz + nlz == 64) {
          // Set reg to all ones
          count += emit_pcmpeqd_rr(output, reg, reg);
          if (ntz)
            count += emit_psllq_ri(output, reg, (uint8_t)(ntz + nlz));
          if (nlz)
            count += emit_psrlq_ri(output, reg, (uint8_t)nlz);
        } else {
          count += emit_movq_ri(output, ScratchGP, val);
          count += emit_sse_movq_rr(output, reg, ScratchGP);
        }
      }
      break;
    default:
      break;
  }
  return count;
}

// Load value from stack to register.
int
Fill(struct SizedBuffer* output, sgxwasm_register_t reg, uint32_t index,
     sgxwasm_valtype_t type)
{
  struct Operand src;
  int32_t offset;
  offset = index * StackSlotSize * -1 - StackOffset;
  build_operand(&src, GP_RBP, REG_UNKNOWN, SCALE_NONE, offset);
  switch (type) {
    case VALTYPE_I32:
      emit_mov_rm(output, reg, &src, VALTYPE_I32);
      break;
    case VALTYPE_I64:
      emit_mov_rm(output, reg, &src, VALTYPE_I64);
      break;
    case VALTYPE_F32:
      emit_movss_rm(output, reg, &src);
      break;
    case VALTYPE_F64:
      emit_movsd_rm(output, reg, &src);
      break;
    default:
      // Should not reach here.
      assert(0);
  }
  return 1;
}

// Store value to the stack.
int
Spill(struct SizedBuffer* output, uint32_t* num_used_spill_slots,
      uint32_t index, sgxwasm_register_t reg, int64_t val,
      sgxwasm_valtype_t type)
{
  int count = 0;
  struct Operand dst;
  int32_t offset;

  // Record number of used spill slots.
  if (num_used_spill_slots != NULL && *num_used_spill_slots <= index) {
    *num_used_spill_slots = index + 1;
  }
#if DEBUG_ASSEMBLE
  printf("[Spill] reg: %u, index: %u, used_spill_slots: %u\n", reg, index,
         *num_used_spill_slots);
#endif

  offset = index * StackSlotSize * -1 - StackOffset;
  build_operand(&dst, GP_RBP, REG_UNKNOWN, SCALE_NONE, offset);

  if (reg != REG_UNKNOWN) {
    switch (type) {
      case VALTYPE_I32:
        count += emit_mov_mr(output, &dst, reg, VALTYPE_I32);
        break;
      case VALTYPE_I64:
        count += emit_mov_mr(output, &dst, reg, VALTYPE_I64);
        break;
      case VALTYPE_F32:
        count += emit_movss_mr(output, &dst, reg);
        break;
      case VALTYPE_F64:
        count += emit_movsd_mr(output, &dst, reg);
        break;
      default:
        // Should not reach here.
        assert(0);
        break;
    }
  } else {
    switch (type) {
      case VALTYPE_I32:
        count += emit_mov_mi(output, &dst, val, VALTYPE_I32);
        break;
      case VALTYPE_I64: {
        if (is_int32(val)) {
          // Sign extend low word.
          count += emit_mov_mi(output, &dst, val, VALTYPE_I64);
        } else if (is_uint32(val)) {
          // Zero extend low word.
          count += emit_movl_ri(output, ScratchGP, val);
          count += emit_mov_mr(output, &dst, ScratchGP, VALTYPE_I64);
        } else {
          count += emit_movq_ri(output, ScratchGP, val);
          count += emit_mov_mr(output, &dst, ScratchGP, VALTYPE_I64);
        }
        break;
      }
      default:
        // Should not reach here.
        assert(0);
        break;
    }
  }
  return count;
}

static int
get_mem_op(struct SizedBuffer* output, struct Operand* op,
           sgxwasm_register_t addr, sgxwasm_register_t offset,
           uint32_t offset_imm)
{
  int count = 0;
  if (is_uint31(offset_imm)) {
    if (offset == REG_UNKNOWN) {
      build_operand(op, addr, REG_UNKNOWN, SCALE_NONE, offset_imm);
      return count;
    }
    build_operand(op, addr, offset, SCALE_1, offset_imm);
    return count;
  }
  sgxwasm_register_t scratch = ScratchGP;
  count += emit_movl_ri(output, scratch, offset_imm);
  if (offset != REG_UNKNOWN) {
    count += emit_add_rr(output, ScratchGP, offset, VALTYPE_I64);
  }
  build_operand(op, addr, scratch, SCALE_1, 0);
  return count;
}

load_type_t
get_load_type(sgxwasm_valtype_t valtype)
{
  switch (valtype) {
    case VALTYPE_I32:
      return I32Load;
    case VALTYPE_I64:
      return I64Load;
    case VALTYPE_F32:
      return F32Load;
    case VALTYPE_F64:
      return F64Load;
    default:
      assert(0);
  }
}

sgxwasm_valtype_t
get_load_value_type(load_type_t type)
{
  switch (type) {
    case I32Load:
    case I32Load8U:
    case I32Load8S:
    case I32Load16U:
    case I32Load16S:
      return VALTYPE_I32;
    case I64Load:
    case I64Load8U:
    case I64Load8S:
    case I64Load16U:
    case I64Load16S:
    case I64Load32U:
    case I64Load32S:
      return VALTYPE_I64;
    case F32Load:
      return VALTYPE_F32;
    case F64Load:
      return VALTYPE_F64;
    default:
      assert(0);
  }
}

int
Load(struct SizedBuffer* output, sgxwasm_register_t dst,
     sgxwasm_register_t src_addr, sgxwasm_register_t offset_reg,
     uint32_t offset_imm, load_type_t type, uint32_t* protected_load_pc,
     uint8_t is_load_mem)
{
  int count = 0;
  struct Operand src_op;

  (void)is_load_mem;

  count += get_mem_op(output, &src_op, src_addr, offset_reg, offset_imm);
  if (protected_load_pc) {
    *protected_load_pc = pc(output);
  }
  switch (type) {
    case I32Load8U:
    case I64Load8U:
      count += emit_movzxb_rm(output, dst, &src_op);
      break;
    case I32Load8S:
      count += emit_movsxb_rm(output, dst, &src_op, VALTYPE_I32);
      break;
    case I64Load8S:
      count += emit_movsxb_rm(output, dst, &src_op, VALTYPE_I64);
      break;
    case I32Load16U:
    case I64Load16U:
      count += emit_movzxw_rm(output, dst, &src_op);
      break;
    case I32Load16S:
      count += emit_movsxw_rm(output, dst, &src_op, VALTYPE_I32);
      break;
    case I64Load16S:
      count += emit_movsxw_rm(output, dst, &src_op, VALTYPE_I64);
      break;
    case I32Load:
    case I64Load32U:
      count += emit_mov_rm(output, dst, &src_op, VALTYPE_I32);
      break;
    case I64Load32S:
      count += emit_movsxlq_rm(output, dst, &src_op);
      break;
    case I64Load:
      count += emit_mov_rm(output, dst, &src_op, VALTYPE_I64);
      break;
    case F32Load:
      count += emit_movss_rm(output, dst, &src_op);
      break;
    case F64Load:
      count += emit_movsd_rm(output, dst, &src_op);
      break;
    default:
      assert(0);
  }
  return count;
}

static void
Load_RM(struct SizedBuffer* output, sgxwasm_register_t dst, struct Operand* src,
        sgxwasm_valtype_t type)
{
  switch (type) {
    case VALTYPE_I32:
      emit_mov_rm(output, dst, src, VALTYPE_I32);
      break;
    case VALTYPE_I64:
      emit_mov_rm(output, dst, src, VALTYPE_I64);
      break;
    case VALTYPE_F32:
      emit_movss_rm(output, dst, src);
      break;
    case VALTYPE_F64:
      emit_movsd_rm(output, dst, src);
      break;
    default:
      assert(0);
  }
}

int
LoadCallerFrameSlot(struct SizedBuffer* output, sgxwasm_register_t dst,
                    uint32_t caller_slot_idx, sgxwasm_valtype_t type)
{
  struct Operand src;
  int32_t offset = StackSlotSize * caller_slot_idx + CallerFrameOffset;
  build_operand(&src, GP_RBP, REG_UNKNOWN, SCALE_NONE, offset);
  Load_RM(output, dst, &src, type);
  return 1;
}

store_type_t
get_store_type(sgxwasm_valtype_t valtype)
{
  switch (valtype) {
    case VALTYPE_I32:
      return I32Store;
    case VALTYPE_I64:
      return I64Store;
    case VALTYPE_F32:
      return F32Store;
    case VALTYPE_F64:
      return F64Store;
    default:
      assert(0);
  }
}

sgxwasm_valtype_t
get_store_value_type(store_type_t type)
{
  switch (type) {
    case I32Store:
    case I32Store8:
    case I32Store16:
      return VALTYPE_I32;
    case I64Store:
    case I64Store8:
    case I64Store16:
    case I64Store32:
      return VALTYPE_I64;
    case F32Store:
      return VALTYPE_F32;
    case F64Store:
      return VALTYPE_F64;
    default:
      assert(0);
  }
}

int
Store(struct SizedBuffer* output, sgxwasm_register_t dst_addr,
      sgxwasm_register_t offset_reg, uint32_t offset_imm,
      sgxwasm_register_t src, store_type_t type, uint32_t* protected_load_pc,
      uint8_t is_store_mem)
{
  int count = 0;
  struct Operand dst_op;

  (void)is_store_mem;

  count += get_mem_op(output, &dst_op, dst_addr, offset_reg, offset_imm);
  if (protected_load_pc) {
    *protected_load_pc = pc(output);
  }
  switch (type) {
    case I32Store8:
    case I64Store8:
      count += emit_movb_mr(output, &dst_op, src);
      break;
    case I32Store16:
    case I64Store16:
      count += emit_movw_mr(output, &dst_op, src);
      break;
    case I32Store:
    case I64Store32:
      count += emit_mov_mr(output, &dst_op, src, VALTYPE_I32);
      break;
    case I64Store:
      count += emit_mov_mr(output, &dst_op, src, VALTYPE_I64);
      break;
    case F32Store:
      count += emit_movss_mr(output, &dst_op, src);
      break;
    case F64Store:
      count += emit_movsd_mr(output, &dst_op, src);
      break;
    default:
      assert(0);
  }
  return count;
}

int
Push(struct SizedBuffer* output, sgxwasm_register_t reg, sgxwasm_valtype_t type)
{
  int count = 0;
  switch (type) {
    case VALTYPE_I32:
    case VALTYPE_I64:
      count += emit_pushq_r(output, reg);
      break;
    case VALTYPE_F32: {
      struct Operand dst;
      count += get_mem_op(output, &dst, GP_RSP, REG_UNKNOWN, 0);
      count += emit_sub_ri(output, GP_RSP, StackSlotSize, VALTYPE_I64);
      count += emit_movss_mr(output, &dst, reg);
      break;
    }
    case VALTYPE_F64: {
      struct Operand dst;
      count += get_mem_op(output, &dst, GP_RSP, REG_UNKNOWN, 0);
      count += emit_sub_ri(output, GP_RSP, StackSlotSize, VALTYPE_I64);
      count += emit_movsd_mr(output, &dst, reg);
      break;
    }
    default:
      assert(0);
  }
  return count;
}

int
Shift(struct SizedBuffer* output, sgxwasm_register_t dst, uint8_t opcode)
{
  switch (opcode) {
    case OPCODE_I32_SHL:
      emit_shl_r(output, dst, VALTYPE_I32);
      break;
    case OPCODE_I32_SHR_U:
      emit_shr_r(output, dst, VALTYPE_I32);
      break;
    case OPCODE_I32_SHR_S:
      emit_sar_r(output, dst, VALTYPE_I32);
      break;
    case OPCODE_I32_ROTL:
      emit_rol_r(output, dst, VALTYPE_I32);
      break;
    case OPCODE_I32_ROTR:
      emit_ror_r(output, dst, VALTYPE_I32);
      break;
    case OPCODE_I64_SHL:
      emit_shl_r(output, dst, VALTYPE_I64);
      break;
    case OPCODE_I64_SHR_U:
      emit_shr_r(output, dst, VALTYPE_I64);
      break;
    case OPCODE_I64_SHR_S:
      emit_sar_r(output, dst, VALTYPE_I64);
      break;
    case OPCODE_I64_ROTL:
      emit_rol_r(output, dst, VALTYPE_I64);
      break;
    case OPCODE_I64_ROTR:
      emit_ror_r(output, dst, VALTYPE_I64);
      break;
    default:
      assert(0);
  }
  return 1;
}

static int
convert_float_to_uint64(struct SizedBuffer* output, sgxwasm_register_t dst,
                        sgxwasm_register_t src, sgxwasm_valtype_t src_type,
                        label_t* fail)
{
  int count = 0;
  label_t success = { 0, 0 };
  // There does not exist a native float-to-uint instruction, so we have to use
  // a float-to-int, and postprocess the result.
  if (src_type == VALTYPE_F64) {
    count += emit_cvttsd2siq_rr(output, dst, src);
  } else {
    assert(src_type == VALTYPE_F32);
    count += emit_cvttss2siq_rr(output, dst, src);
  }
  // If the result of the conversion is positive, we are already done.
  count += emit_test_rr(output, dst, dst, VALTYPE_I64);
  count += emit_jcc(output, COND_POSITIVE, &success, Far);
  // The result of the first conversion was negative, which means that the
  // input value was not within the positive int64 range. We subtract 2^63
  // and convert it again to see if it is within the uint64 range.
  if (src_type == VALTYPE_F64) {
    count += LoadConstant(output, ScratchFP,
                          f64_encoding(-9223372036854775808.0), VALTYPE_F64);
    count += emit_addsd_rr(output, ScratchFP, src);
    count += emit_cvttsd2siq_rr(output, dst, ScratchFP);
  } else {
    count += LoadConstant(output, ScratchFP,
                          f32_encoding(-9223372036854775808.0f), VALTYPE_F32);
    count += emit_addss_rr(output, ScratchFP, src);
    count += emit_cvttss2siq_rr(output, dst, ScratchFP);
  }
  count += emit_test_rr(output, dst, dst, VALTYPE_I64);
  // The only possible negative value here is 0x80000000000000000, which is
  // used on x64 to indicate an integer overflow.
  count += emit_jcc(output, COND_NEGATIVE, fail ? fail : &success, Far);
  // The input value is within uint64 range and the second conversion worked
  // successfully, but we still have to undo the subtraction we did
  // earlier.
  count += emit_movq_ri(output, ScratchGP, 0x8000000000000000);
  count += emit_or_rr(output, dst, ScratchGP, VALTYPE_I64);
  bind_label(output, &success, pc_offset(output));
  return count;
}

static int
convert_float_to_int_and_back(struct SizedBuffer* output,
                              sgxwasm_register_t dst,
                              sgxwasm_valtype_t dst_type,
                              val_signed_or_unsigned_t signed_or_unsigned,
                              sgxwasm_register_t src,
                              sgxwasm_valtype_t src_type,
                              sgxwasm_register_t converted_back)
{
  int count = 0;
  assert(is_fp(src) && is_fp(converted_back));
  if (src_type == VALTYPE_F64) { // f64
    if (dst_type == VALTYPE_I32 &&
        signed_or_unsigned == VAL_SIGNED) { // f64 -> i32
      count += emit_cvttsd2si_rr(output, dst, src);
      count += emit_cvtlsi2sd_rr(output, converted_back, dst);
    } else if (dst_type == VALTYPE_I32 &&
               signed_or_unsigned == VAL_UNSIGNED) { // f64 -> u32
      count += emit_cvttsd2siq_rr(output, dst, src);
      count += emit_mov_rr(output, dst, dst, VALTYPE_I32);
      count += emit_cvtqsi2sd_rr(output, converted_back, dst);
    } else if (dst_type == VALTYPE_I64 &&
               signed_or_unsigned == VAL_SIGNED) { // f64 -> i64
      count += emit_cvttsd2siq_rr(output, dst, src);
      count += emit_cvtqsi2sd_rr(output, converted_back, dst);
    } else {
      assert(0);
    }
  } else { // f32
    if (dst_type == VALTYPE_I32 &&
        signed_or_unsigned == VAL_SIGNED) { // f32 -> i32
      count += emit_cvttss2si_rr(output, dst, src);
      count += emit_cvtlsi2ss_rr(output, converted_back, dst);
    } else if (dst_type == VALTYPE_I32 &&
               signed_or_unsigned == VAL_UNSIGNED) { // f32 -> u32
      count += emit_cvttss2siq_rr(output, dst, src);
      count += emit_mov_rr(output, dst, dst, VALTYPE_I32);
      count += emit_cvtqsi2ss_rr(output, converted_back, dst);
    } else if (dst_type == VALTYPE_I64 &&
               signed_or_unsigned == VAL_SIGNED) { // f32 -> i64
      count += emit_cvttss2siq_rr(output, dst, src);
      count += emit_cvtqsi2ss_rr(output, converted_back, dst);
    } else {
      assert(0);
    }
  }
  return count;
}

static int
emit_truncate_float_to_int(struct SizedBuffer* output, sgxwasm_register_t dst,
                           sgxwasm_valtype_t dst_type,
                           val_signed_or_unsigned_t signed_or_unsigned,
                           sgxwasm_register_t src, sgxwasm_valtype_t src_type,
                           label_t* trap)
{
  int count = 0;
  sgxwasm_register_t rounded = ScratchFP;
  sgxwasm_register_t converted_back = ScratchFP2;

  if (src_type == VALTYPE_F64) {
    count += emit_roundsd_rr(output, rounded, src, RoundToZero);
  } else {
    assert(src_type == VALTYPE_F32);
    count += emit_roundss_rr(output, rounded, src, RoundToZero);
  }
  count +=
    convert_float_to_int_and_back(output, dst, dst_type, signed_or_unsigned,
                                  rounded, src_type, converted_back);
  if (src_type == VALTYPE_F64) {
    count += emit_ucomisd_rr(output, converted_back, rounded);
  } else {
    assert(src_type == VALTYPE_F32);
    count += emit_ucomiss_rr(output, converted_back, rounded);
  }

  // Jump to trap if PF is 0 (one of the operands was NaN) or they are not
  // equal.
  count += emit_jcc(output, COND_EVEN, trap, Far);
  count += emit_jcc(output, COND_NE, trap, Far);

  // return 1;
  return count;
}

int
emit_type_conversion(struct SizedBuffer* output, uint8_t opcode,
                     sgxwasm_register_t dst, sgxwasm_register_t src,
                     label_t* trap)
{
  int count = 0;
  switch (opcode) {
    case OPCODE_I32_WRAP_I64:
      count += emit_mov_rr(output, dst, src, VALTYPE_I32);
      break;
    case OPCODE_I32_TRUNC_S_F32:
      count += emit_truncate_float_to_int(output, dst, VALTYPE_I32, VAL_SIGNED,
                                          src, VALTYPE_F32, trap);
      break;
    case OPCODE_I32_TRUNC_U_F32:
      count += emit_truncate_float_to_int(output, dst, VALTYPE_I32,
                                          VAL_UNSIGNED, src, VALTYPE_F32, trap);
      break;
    case OPCODE_I32_TRUNC_S_F64:
      count += emit_truncate_float_to_int(output, dst, VALTYPE_I32, VAL_SIGNED,
                                          src, VALTYPE_F64, trap);
      break;
    case OPCODE_I32_TRUNC_U_F64:
      count += emit_truncate_float_to_int(output, dst, VALTYPE_I32,
                                          VAL_UNSIGNED, src, VALTYPE_F64, trap);
      break;
    case OPCODE_I32_REINTERPRET_F32:
      count += emit_sse_movd_rr(output, dst, src);
      break;
    case OPCODE_I64_EXTEND_S_I32:
      count += emit_movsxlq_rr(output, dst, src);
      break;
    case OPCODE_I64_EXTEND_U_I32:
      if (dst != src) {
        count += emit_mov_rr(output, dst, src, VALTYPE_I32);
      }
      break;
    case OPCODE_I64_TRUNC_S_F32:
      count += emit_truncate_float_to_int(output, dst, VALTYPE_I64, VAL_SIGNED,
                                          src, VALTYPE_F32, trap);
      break;
    case OPCODE_I64_TRUNC_U_F32:
      count += convert_float_to_uint64(output, dst, src, VALTYPE_F32, trap);
      break;
    case OPCODE_I64_TRUNC_S_F64:
      count += emit_truncate_float_to_int(output, dst, VALTYPE_I64, VAL_SIGNED,
                                          src, VALTYPE_F64, trap);
      break;
    case OPCODE_I64_TRUNC_U_F64:
      count += convert_float_to_uint64(output, dst, src, VALTYPE_F64, trap);
      break;
    case OPCODE_I64_REINTERPRET_F64:
      count += emit_sse_movq_rr(output, dst, src);
      break;
    case OPCODE_F32_CONVERT_S_I32:
      count += emit_cvtlsi2ss_rr(output, dst, src);
      break;
    case OPCODE_F32_CONVERT_U_I32: {
      count += emit_mov_rr(output, ScratchGP, src, VALTYPE_I32);
      count += emit_cvtqsi2ss_rr(output, dst, ScratchGP);
      break;
    }
    case OPCODE_F32_CONVERT_S_I64:
      count += emit_cvtqsi2ss_rr(output, dst, src);
      break;
    case OPCODE_F32_CONVERT_U_I64:
      count += emit_cvtqui2ss_rr(output, dst, src);
      break;
    case OPCODE_F32_DEMOTE_F64:
      count += emit_cvtsd2ss_rr(output, dst, src);
      break;
    case OPCODE_F32_REINTERPRET_I32:
      count += emit_sse_movd_rr(output, dst, src);
      break;
    case OPCODE_F64_CONVERT_S_I32:
      count += emit_cvtlsi2sd_rr(output, dst, src);
      break;
    case OPCODE_F64_CONVERT_U_I32: {
      count += emit_mov_rr(output, ScratchGP, src, VALTYPE_I32);
      count += emit_cvtqsi2sd_rr(output, dst, src);
      break;
    }
    case OPCODE_F64_CONVERT_S_I64:
      count += emit_cvtqsi2sd_rr(output, dst, src);
      break;
    case OPCODE_F64_CONVERT_U_I64:
      count += emit_cvtqui2sd_rr(output, dst, src);
      break;
    case OPCODE_F64_PROMOTE_F32:
      count += emit_cvtss2sd_rr(output, dst, src);
      break;
    case OPCODE_F64_REINTERPRET_I64:
      count += emit_sse_movq_rr(output, dst, src);
      break;
    default:
      assert(0); // unreachable.
  }
  return count;
}

int
InitStack(struct SizedBuffer* output, size_t index, size_t size)
{
  int count = 0;
  struct Operand dst;
  int32_t offset;

  if (size == 0) {
    return 0;
  }
  offset = index * StackSlotSize * -1 - StackOffset;
  build_operand(&dst, GP_RBP, REG_UNKNOWN, SCALE_NONE, offset);

  // Set DF = 0, ensure the direction of rep is increment.
  count += emit_cld(output);
  count += emit_lea_rm(output, GP_RDI, &dst, VALTYPE_I64);
  count += emit_xor_rr(output, GP_RAX, GP_RAX, VALTYPE_I64);
  count += emit_movl_ri(output, GP_RCX, (int32_t)size);
  count += emit_rep_stosq(output);
  return count;
}

// For runtime hook
void
SaveRegisterStates(struct SizedBuffer* output)
{
  // Stack padding.
  emit_pushq_r(output, GP_R11);

  emit_pushq_r(output, GP_RAX);
  emit_pushq_r(output, GP_RDI);
  emit_pushq_r(output, GP_RSI);
  emit_pushq_r(output, GP_RDX);
  emit_pushq_r(output, GP_RCX);
  emit_pushq_r(output, GP_R8);
  emit_pushq_r(output, GP_R9);
}

void
RestoreRegisterStates(struct SizedBuffer* output)
{
  emit_popq_r(output, GP_R9);
  emit_popq_r(output, GP_R8);
  emit_popq_r(output, GP_RCX);
  emit_popq_r(output, GP_RDX);
  emit_popq_r(output, GP_RSI);
  emit_popq_r(output, GP_RDI);
  emit_popq_r(output, GP_RAX);
  // Stack padding.
  emit_popq_r(output, GP_R11);
}

void
DebugPrint(char* fmt, ...)
{
#if __SGX__
  char buf[256] = { '\0' };
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, 256, fmt, ap);
  va_end(ap);
  ocall_print_string(buf);
#else
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
#endif
}

void
HookFunction(struct SizedBuffer* output, int n_args, char* fmt, ...)
{
  va_list ap;
  int i;
  uint32_t arg;
  sgxwasm_register_t reg;

  // We only support upto 6 args to
  // avoid messing up the stack.

  // Save registers.
  // SaveRegisterStates(output);

  emit_movq_ri(output, GP_RDI, (uint64_t)fmt);
  va_start(ap, fmt);
  for (i = 0; i < n_args; i++) {
    arg = va_arg(ap, uint32_t);
    reg = GPParameterList[i + 1];
    emit_movl_ri(output, reg, arg);
  }
  va_end(ap);

  emit_movq_ri(output, GP_RAX, (uint64_t)DebugPrint);
  emit_call_r(output, GP_RAX);

  // Restore registers.
  // RestoreRegisterStates(output);
}
