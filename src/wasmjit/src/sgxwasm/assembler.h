#ifndef __SGXWASM__ASSEMBLER_H__
#define __SGXWASM__ASSEMBLER_H__

#include <sgxwasm/ast.h>
#include <sgxwasm/register.h>
#include <sgxwasm/util.h>

//#include <sgxwasm/sse-instr.h>

#define StackSlotSize 8
#define StackOffset 8
#define CallerFrameOffset 16
#define JmpShortSize 1
#define JmpLongSize 4
#define JccShortSize 2
#define JccLongSize 6

#define SIGN_BIT_64 0x8000000000000000
#define SIGN_BIT_32 0x80000000

enum CONDITION
{
  COND_NONE = -1,
  COND_OVERFLOW = 0,
  COND_LT_U = 2,
  COND_GE_U = 3,
  COND_EQ = 4,
  COND_NE = 5,
  COND_LE_U = 6,
  COND_GT_U = 7,
  COND_NEGATIVE = 8,
  COND_POSITIVE = 9,
  COND_EVEN = 10,
  COND_ODD = 11,
  COND_LT_S = 12,
  COND_GE_S = 13,
  COND_LE_S = 14,
  COND_GT_S = 15,
};
typedef enum CONDITION condition_t;

enum SCALE_FACTOR
{
  SCALE_1 = 0,
  SACLE_2 = 1,
  SCALE_4 = 2,
  SCALE_8 = 3,
  SCALE_NONE,
};
typedef enum SCALE_FACTOR scale_factor_t;

enum LOAD_TYPE
{
  I32Load8U,
  I64Load8U,
  I32Load8S,
  I64Load8S,
  I32Load16U,
  I64Load16U,
  I32Load16S,
  I64Load16S,
  I32Load,
  I64Load32U,
  I64Load32S,
  I64Load,
  F32Load,
  F64Load,
};
typedef enum LOAD_TYPE load_type_t;
#define PointerLoadType I64Load;

enum STORE_TYPE
{
  I32Store8,
  I64Store8,
  I32Store16,
  I64Store16,
  I32Store,
  I64Store32,
  I64Store,
  F32Store,
  F64Store,
};
typedef enum STORE_TYPE store_type_t;

enum SHIFT_CODE
{
  SHIFT_ROL = 0x0,
  SHIFT_ROR = 0x1,
  SHIFT_RCL = 0x2,
  SHIFT_RCR = 0x3,
  SHIFT_SHL = 0x4,
  SHIFT_SHR = 0x5,
  SHIFT_SAR = 0x7,
};
typedef enum SHIFT_CODE shift_code_t;

enum FLOAT_MIN_OR_MAX
{
  FLOAT_MIN,
  FLOAT_MAX,
};
typedef enum FLOAT_MIN_OR_MAX float_min_or_max_t;

enum INT_DIV_OR_REM
{
  INT_DIV,
  INT_REM,
};
typedef enum INT_DIV_OR_REM int_div_or_rem_t;

enum VAL_SIGNED_OR_UNSIGNED
{
  VAL_SIGNED,
  VAL_UNSIGNED,
};
typedef enum VAL_SIGNED_OR_UNSIGNED val_signed_or_unsigned_t;

enum ROUNDING_MODE
{
  RoundToNearest = 0x0,
  RoundDown = 0x1,
  RoundUp = 0x2,
  RoundToZero = 0x3,
};
typedef enum ROUNDING_MODE rounding_mode_t;

struct Label
{
  // pos <  0  bound label, pos() returns the jump target position
  // pos == 0  unused label
  // pos >  0  linked label, pos() returns the last reference position
  int32_t pos;
  int32_t near_link_pos;
};
typedef struct Label label_t;

enum Distance
{
  Near,
  Far,
};
typedef enum Distance distance_t;

int
is_bound(label_t*);

int
is_unused(label_t*);

int
is_linked(label_t*);

void
bind_label(struct SizedBuffer*, label_t*, int);

struct Operand
{
  uint8_t rex;
  uint8_t buf[9];
  uint8_t len;   // number of bytes of buf_ in use.
  int8_t addend; // for rip + offset + addend.
};
void
build_operand(struct Operand*,
              sgxwasm_register_t,
              sgxwasm_register_t,
              scale_factor_t,
              int32_t);

void
emit(struct SizedBuffer*, int8_t);
void
emit_imm(struct SizedBuffer*, int64_t, uint8_t);
void
emit_rex_rr(struct SizedBuffer*,
            sgxwasm_register_t,
            sgxwasm_register_t,
            sgxwasm_valtype_t,
            uint8_t);
void
emit_rex_rm(struct SizedBuffer*,
            sgxwasm_register_t,
            struct Operand*,
            sgxwasm_valtype_t,
            uint8_t);
void
emit_rex_r(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_valtype_t, uint8_t);
void
emit_operand_rm(struct SizedBuffer*, sgxwasm_register_t, struct Operand*);
void
emit_operand_cm(struct SizedBuffer*, int, struct Operand*);
void
emit_operand_cr(struct SizedBuffer*, int, sgxwasm_register_t);
void
emit_modrm_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_modrm_cr(struct SizedBuffer*, int8_t, sgxwasm_register_t);
void
emit_sse_operand_rr(struct SizedBuffer*,
                    sgxwasm_register_t,
                    sgxwasm_register_t);
void
emit_addss_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_subss_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_mulss_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_divss_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_addsd_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_subsd_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_mulsd_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_divsd_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_pushq_r(struct SizedBuffer*, sgxwasm_register_t);
void
emit_pushq_m(struct SizedBuffer*, struct Operand*);
void
emit_pushq_i(struct SizedBuffer*, uint32_t);
void
emit_popq_r(struct SizedBuffer*, sgxwasm_register_t);
void
emit_ret(struct SizedBuffer*, int);
void
emit_neq_r(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_valtype_t);
void
emit_and_rr(struct SizedBuffer*,
            sgxwasm_register_t,
            sgxwasm_register_t,
            sgxwasm_valtype_t);
void
emit_and_ri(struct SizedBuffer*,
            sgxwasm_register_t,
            int64_t,
            sgxwasm_valtype_t);
void
emit_or_rr(struct SizedBuffer*,
           sgxwasm_register_t,
           sgxwasm_register_t,
           sgxwasm_valtype_t);
void
emit_or_ri(struct SizedBuffer*, sgxwasm_register_t, int64_t, sgxwasm_valtype_t);
void
emit_xor_rr(struct SizedBuffer*,
            sgxwasm_register_t,
            sgxwasm_register_t,
            sgxwasm_valtype_t);
void
emit_xor_ri(struct SizedBuffer*,
            sgxwasm_register_t,
            int64_t,
            sgxwasm_valtype_t);
void
emit_add_rr(struct SizedBuffer*,
            sgxwasm_register_t,
            sgxwasm_register_t,
            sgxwasm_valtype_t);
void
emit_sub_rr(struct SizedBuffer*,
            sgxwasm_register_t,
            sgxwasm_register_t,
            sgxwasm_valtype_t);
void
emit_sub_ri(struct SizedBuffer*,
            sgxwasm_register_t,
            int64_t,
            sgxwasm_valtype_t);
void
emit_imul_rr(struct SizedBuffer*,
             sgxwasm_register_t,
             sgxwasm_register_t,
             sgxwasm_valtype_t);
void
emit_cmp_rr(struct SizedBuffer*,
            sgxwasm_register_t,
            sgxwasm_register_t,
            sgxwasm_valtype_t);
void
emit_cmp_ri(struct SizedBuffer*,
            sgxwasm_register_t,
            int64_t,
            sgxwasm_valtype_t);

void
emit_idiv_r(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_valtype_t);

void
emit_div_r(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_valtype_t);
void
emit_div_or_rem(struct SizedBuffer*,
                sgxwasm_register_t,
                sgxwasm_register_t,
                sgxwasm_register_t,
                label_t*,
                label_t*,
                sgxwasm_valtype_t,
                int_div_or_rem_t,
                val_signed_or_unsigned_t);
void
emit_cdq(struct SizedBuffer*);
void
emit_cqo(struct SizedBuffer*);

void
emit_setcc(struct SizedBuffer*, condition_t, sgxwasm_register_t);
void
emit_set_cond(struct SizedBuffer*,
              condition_t,
              sgxwasm_register_t,
              sgxwasm_register_t,
              sgxwasm_register_t,
              sgxwasm_valtype_t);
void
emit_test_rr(struct SizedBuffer*,
             sgxwasm_register_t,
             sgxwasm_register_t,
             sgxwasm_valtype_t);

void
emit_test_ri(struct SizedBuffer*,
             sgxwasm_register_t,
             int64_t,
             sgxwasm_valtype_t);
void
emit_lea_rm(struct SizedBuffer*,
            sgxwasm_register_t,
            struct Operand*,
            sgxwasm_valtype_t);
void
emit_call_r(struct SizedBuffer*, sgxwasm_valtype_t);
void
emit_subq_sp_32(struct SizedBuffer*, uint32_t);
// Bit operations.
void
emit_btr_ri(struct SizedBuffer*, sgxwasm_register_t, int8_t);
void
emit_bsr_rr(struct SizedBuffer*,
            sgxwasm_register_t,
            sgxwasm_register_t,
            sgxwasm_valtype_t);
void
emit_bsf_rr(struct SizedBuffer*,
            sgxwasm_register_t,
            sgxwasm_register_t,
            sgxwasm_valtype_t);
// Shift operations.
void
shift_r(struct SizedBuffer*,
        sgxwasm_register_t,
        shift_code_t,
        sgxwasm_valtype_t);
void
shift_ri(struct SizedBuffer*,
         sgxwasm_register_t,
         int8_t,
         shift_code_t,
         sgxwasm_valtype_t);
void
emit_shr_r(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_valtype_t);
void
emit_shr_ri(struct SizedBuffer*, sgxwasm_register_t, int8_t, sgxwasm_valtype_t);
void
emit_shl_r(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_valtype_t);
void
emit_shl_ri(struct SizedBuffer*, sgxwasm_register_t, int8_t, sgxwasm_valtype_t);
void
emit_sar_r(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_valtype_t);
void
emit_sar_ri(struct SizedBuffer*, sgxwasm_register_t, int8_t, sgxwasm_valtype_t);
void
arithmetic_op_rr(struct SizedBuffer*,
                 int8_t,
                 sgxwasm_register_t,
                 sgxwasm_register_t,
                 sgxwasm_valtype_t);
void
arithmetic_op_ri(struct SizedBuffer*,
                 int8_t,
                 sgxwasm_register_t,
                 int64_t,
                 sgxwasm_valtype_t);
// Moves
void
emit_movl_ri(struct SizedBuffer*, sgxwasm_register_t, int32_t);
void
emit_movq_ri(struct SizedBuffer*, sgxwasm_register_t, int64_t);
void
emit_mov_rr(struct SizedBuffer*,
            sgxwasm_register_t,
            sgxwasm_register_t,
            sgxwasm_valtype_t);
void
emit_mov_rm(struct SizedBuffer*,
            sgxwasm_register_t,
            struct Operand*,
            sgxwasm_valtype_t);
void
emit_mov_mr(struct SizedBuffer*,
            struct Operand*,
            sgxwasm_register_t,
            sgxwasm_valtype_t);
void
emit_movb_mr(struct SizedBuffer*, struct Operand*, sgxwasm_register_t);
void
emit_movw_mr(struct SizedBuffer*, struct Operand*, sgxwasm_register_t);
void
emit_movzxb_rm(struct SizedBuffer*, sgxwasm_register_t, struct Operand*);
void
emit_movzxb_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_movzxw_rm(struct SizedBuffer*, sgxwasm_register_t, struct Operand*);
void
emit_movsxb_rm(struct SizedBuffer*,
               sgxwasm_register_t,
               struct Operand*,
               sgxwasm_valtype_t);

void
emit_movsxw_rm(struct SizedBuffer*,
               sgxwasm_register_t,
               struct Operand*,
               sgxwasm_valtype_t);

void
emit_movsxlq_rm(struct SizedBuffer*, sgxwasm_register_t, struct Operand*);
void
emit_movsxlq_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
// SSE operations.
void
sse2_instr_rr(struct SizedBuffer*,
              sgxwasm_register_t,
              sgxwasm_register_t,
              uint8_t,
              uint8_t,
              uint8_t);
void
emit_ucomiss_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_ucomisd_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_andps_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_andpd_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_xorps_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_xorpd_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_roundss_rr(struct SizedBuffer*,
                sgxwasm_register_t,
                sgxwasm_register_t,
                rounding_mode_t);
void
emit_roundsd_rr(struct SizedBuffer*,
                sgxwasm_register_t,
                sgxwasm_register_t,
                rounding_mode_t);
void
emit_sqrtss_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_sqrtsd_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_movss_rm(struct SizedBuffer*, sgxwasm_register_t, struct Operand*);
void
emit_movss_mr(struct SizedBuffer*, struct Operand*, sgxwasm_register_t);
void
emit_movss_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_movsd_rm(struct SizedBuffer*, sgxwasm_register_t, struct Operand*);
void
emit_movsd_mr(struct SizedBuffer*, struct Operand*, sgxwasm_register_t);
void
emit_movsd_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_movmskps_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_movmskpd_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_cvttss2si_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_cvttsd2si_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_cvttss2siq_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_cvttsd2siq_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_cvtlsi2ss_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_cvtlsi2sd_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_cvtqsi2ss_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_cvtqsi2sd_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_cvtsd2ss_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_cvtss2sd_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_cvtqui2ss_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_cvtqui2sd_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_float_min_or_max(struct SizedBuffer*,
                      sgxwasm_register_t,
                      sgxwasm_register_t,
                      sgxwasm_register_t,
                      float_min_or_max_t,
                      sgxwasm_valtype_t);
void
emit_pcmpeqd_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);

void
emit_pslld_ri(struct SizedBuffer*, sgxwasm_register_t, uint8_t);

void
emit_psrld_ri(struct SizedBuffer*, sgxwasm_register_t, uint8_t);

void
emit_psllq_ri(struct SizedBuffer*, sgxwasm_register_t, uint8_t);

void
emit_psrlq_ri(struct SizedBuffer*, sgxwasm_register_t, uint8_t);

void
emit_sse_movd_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);

void
emit_sse_movq_rr(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_register_t);
void
emit_float_set_cond(struct SizedBuffer*,
                    condition_t,
                    sgxwasm_register_t,
                    sgxwasm_register_t,
                    sgxwasm_register_t,
                    sgxwasm_valtype_t);
// Jumps
void
emit_jmp_label(struct SizedBuffer*, label_t*, distance_t);
void
emit_jmp_16(struct SizedBuffer*, int8_t);
void
emit_jmp_32(struct SizedBuffer*, int32_t);
void
emit_jmp_r(struct SizedBuffer*, sgxwasm_register_t);
void
emit_jcc(struct SizedBuffer*, condition_t, label_t*, distance_t);
void
emit_cond_jump_rr(struct SizedBuffer*,
                  condition_t,
                  label_t*,
                  sgxwasm_valtype_t,
                  sgxwasm_register_t,
                  sgxwasm_register_t);

// Intel TSX
void
emit_xbegin(struct SizedBuffer*, label_t*);

void
emit_xend(struct SizedBuffer*);

#if 0
#define DECLARE_SSE2_INSTRUCTION(instruction, prefix, escape, opcode)          \
  void emit_##instruction##_rr(struct SizedBuffer* output,                     \
                               sgxwasm_register_t dst,                         \
                               sgxwasm_register_t src)                         \
  {                                                                            \
    sse2_instr_rr(output, dst, src, 0x##prefix, 0x##escape, 0x##opcode);       \
  }                                                              \

  SSE2_INSTRUCTION_LIST(DECLARE_SSE2_INSTRUCTION)
#undef DECLARE_SSE2_INSTRUCTION
#endif

void
Move(struct SizedBuffer*,
     sgxwasm_register_t,
     sgxwasm_register_t,
     sgxwasm_valtype_t);
void
LoadConstant(struct SizedBuffer*,
             sgxwasm_register_t,
             int64_t,
             sgxwasm_valtype_t);

void
Fill(struct SizedBuffer*, sgxwasm_register_t, uint32_t, sgxwasm_valtype_t);

void
Spill(struct SizedBuffer*,
      uint32_t*,
      uint32_t,
      sgxwasm_register_t,
      int64_t,
      sgxwasm_valtype_t);

load_type_t get_load_type(sgxwasm_valtype_t);
sgxwasm_valtype_t get_load_value_type(load_type_t);
void
Load(struct SizedBuffer*,
     sgxwasm_register_t,
     sgxwasm_register_t,
     sgxwasm_register_t,
     uint32_t,
     load_type_t,
     uint32_t*,
     uint8_t);
void
LoadCallerFrameSlot(struct SizedBuffer*,
                    sgxwasm_register_t,
                    uint32_t,
                    sgxwasm_valtype_t);
store_type_t get_store_type(sgxwasm_valtype_t);
sgxwasm_valtype_t get_store_value_type(store_type_t);
void
Store(struct SizedBuffer*,
      sgxwasm_register_t,
      sgxwasm_register_t,
      uint32_t,
      sgxwasm_register_t,
      store_type_t,
      uint32_t*,
      uint8_t);
void
Push(struct SizedBuffer*, sgxwasm_register_t, sgxwasm_valtype_t);
void
Shift(struct SizedBuffer*, sgxwasm_register_t, uint8_t);
void
InitStack(struct SizedBuffer*, size_t, size_t);
int
emit_type_conversion(struct SizedBuffer*,
                     uint8_t,
                     sgxwasm_register_t,
                     sgxwasm_register_t,
                     label_t*);
uint32_t
test_double_fp(double);
uint32_t
test_fp(float);

// For runtime hook.
void
SaveRegisterStates(struct SizedBuffer*);
void
RestoreRegisterStates(struct SizedBuffer*);
void
HookFunction(struct SizedBuffer*, int, char*, ...);

#define CALL_HOOK_FUNC(n, fmt, ...)                                            \
  HookFunction(output(ctx), n, fmt, __VA_ARGS__)

#endif
