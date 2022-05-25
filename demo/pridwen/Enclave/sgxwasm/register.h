#ifndef __SGXWASM__REGISTER_H__
#define __SGXWASM__REGISTER_H__

#include <sgxwasm/ast.h>

#define RegisterNum 32

enum REGISTERS
{
  GP_RAX,
  GP_RCX,
  GP_RDX,
  GP_RBX,
  GP_RSP,
  GP_RBP,
  GP_RSI,
  GP_RDI,
  GP_R8,
  GP_R9,
  GP_R10,
  GP_R11,
  GP_R12,
  GP_R13,
  GP_R14,
  GP_R15,
  FP_XMM0,
  FP_XMM1,
  FP_XMM2,
  FP_XMM3,
  FP_XMM4,
  FP_XMM5,
  FP_XMM6,
  FP_XMM7,
  FP_XMM8,
  FP_XMM9,
  FP_XMM10,
  FP_XMM11,
  FP_XMM12,
  FP_XMM13,
  FP_XMM14,
  FP_XMM15,
  REG_UNKNOWN,
};
typedef enum REGISTERS sgxwasm_register_t;

#define ScratchGP GP_R10
#define ScratchGP2 GP_R11
#define RootReg GP_R13
#define ScratchFP FP_XMM15
#define ScratchFP2 FP_XMM14

#define GPGroupOffset 0
#define GPGroupNum 16

#define FPGroupOffset 16
#define FPGroupNum 16

// 0b1100000000000000 (FP) | 0010110000111000 (GP)
// Callee-saved registers: RBX, RBP, RSP
//#define AllocableRegList 0xc0002c38
// pin r15 + r14 for TSGX do not pin r13
// 0b1100000000000000 1100 1100 0011 1000
//#define AllocableRegList 0xc000ac38
//#define AllocableRegList 0xc000cc38

enum REGISTERS_CLASS
{
  GP_REG,
  FP_REG,
  NO_REG,
};
typedef enum REGISTERS_CLASS sgxwasm_register_class_t;

#define is_gp(reg) (reg >= 0 && reg < GPGroupNum)
#define is_fp(reg) (reg >= FPGroupOffset && reg < FPGroupOffset + FPGroupNum)

#define GPRegisterParameterNum 6
extern sgxwasm_register_t GPParameterList[GPRegisterParameterNum];
#define GPReturnRegister GP_RAX

#define FPRegisterParameterNum 8
extern sgxwasm_register_t FPParemterList[FPRegisterParameterNum];
#define FPReturnRegister0 FP_XMM0
#define FPReturnRegister1 FP_XMM1

typedef uint32_t reglist_t;
#define EmptyRegList 0

void init_allocable_reg_list();
reglist_t get_allocable_reg_list();

// void
// register_cache_init(reglist_t*, uint32_t*, size_t);
void
set(reglist_t*, sgxwasm_register_t);
void
clear(reglist_t*, sgxwasm_register_t);
int has(reglist_t, sgxwasm_register_t);
int is_empty(reglist_t);
sgxwasm_register_t get_first_reg_set(reglist_t);
sgxwasm_register_t get_last_reg_set(reglist_t);
reglist_t mask_out(reglist_t, reglist_t);
reglist_t get_gp_list(reglist_t);
reglist_t get_fp_list(reglist_t);
reglist_t get_cache_reg_list(sgxwasm_register_class_t);
int has_unused_register_with_class(reglist_t,
                                   sgxwasm_register_class_t,
                                   reglist_t);
int has_unused_register(reglist_t, reglist_t, reglist_t);
sgxwasm_register_t unused_register_with_class(reglist_t,
                                              sgxwasm_register_class_t,
                                              reglist_t);
sgxwasm_register_t unused_register(reglist_t, reglist_t, reglist_t);
void
inc_used(reglist_t*, uint32_t*, sgxwasm_register_t);
void
dec_used(reglist_t*, uint32_t*, sgxwasm_register_t);
int
is_used(reglist_t, uint32_t*, sgxwasm_register_t);
uint32_t
get_use_count(uint32_t*, sgxwasm_register_t);
void
clear_used(reglist_t*, uint32_t*, sgxwasm_register_t);
int
is_free(reglist_t, uint32_t*, sgxwasm_register_t);
void
reset_used_registers(reglist_t*, uint32_t*);
sgxwasm_register_t
get_next_spill_reg(reglist_t, reglist_t*, reglist_t, reglist_t);
int reg_class_for(sgxwasm_valtype_t);
int reg_class(sgxwasm_register_t);
void
dump_reg_info(uint32_t*, sgxwasm_register_t);

#endif
