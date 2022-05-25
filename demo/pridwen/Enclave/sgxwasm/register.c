#include <sgxwasm/register.h>
#include <sgxwasm/util.h>

#ifndef DEBUG_REGISTER
#define DEBUG_REGISTER 0
#endif

#define SGXWASM_INT_MAX 2147483647

sgxwasm_register_t GPParameterList[GPRegisterParameterNum] = { GP_RDI, GP_RSI,
                                                               GP_RDX, GP_RCX,
                                                               GP_R8,  GP_R9 };

sgxwasm_register_t FPParemterList[FPRegisterParameterNum] = {
  FP_XMM0, FP_XMM1, FP_XMM2, FP_XMM3, FP_XMM4, FP_XMM5, FP_XMM6, FP_XMM7
};

static reglist_t AllocableRegList = 0;
void
init_allocable_reg_list()
{
  set(&AllocableRegList, ScratchGP);
  set(&AllocableRegList, ScratchGP2);
  set(&AllocableRegList, ScratchFP);
  set(&AllocableRegList, ScratchFP2);

  // Callee-saved registers.
  set(&AllocableRegList, GP_RBX);
  set(&AllocableRegList, GP_RBP);
  set(&AllocableRegList, GP_RSP);

  // For ASLR & T-SGX.
#if __TSGX__ || __VARYS__
  set(&AllocableRegList, GP_R14);
  set(&AllocableRegList, GP_R15);
#endif
}

reglist_t
get_allocable_reg_list()
{
  return AllocableRegList;
}

// Reference implementation of LiftoffRegList class

void
set(reglist_t* list, sgxwasm_register_t reg)
{
  assert(reg < RegisterNum && list != NULL);
  *list |= 1 << reg;
}

void
clear(reglist_t* list, sgxwasm_register_t reg)
{
  assert(reg < RegisterNum && list != NULL);
  *list &= ~(1 << reg);
}

int
has(reglist_t list, sgxwasm_register_t reg)
{
  assert(reg < RegisterNum);
  return (list & (1 << reg)) != 0;
}

int
is_empty(reglist_t list)
{
  return (list == 0);
}

sgxwasm_register_t
get_first_reg_set(reglist_t list)
{
  assert(!is_empty(list));
  unsigned first_code = count_trailing_zeros((uint64_t)list, 4);
  return first_code;
}

sgxwasm_register_t
get_last_reg_set(reglist_t list)
{
  assert(!is_empty(list));
  unsigned last_code =
    8 * sizeof(reglist_t) - 1 - count_leading_zeros((uint64_t)list, 32);
  return last_code;
}

reglist_t
mask_out(reglist_t list, reglist_t mask)
{
  return (list & ~mask);
}

#define GpMask 0xffff
#define FpMask 0xffff0000

reglist_t
get_gp_list(reglist_t list)
{
  return list & GpMask;
}

reglist_t
get_fp_list(reglist_t list)
{
  return list & FpMask;
}

reglist_t
get_cache_reg_list(sgxwasm_register_class_t rc)
{
  return rc == FP_REG ? FpMask : GpMask;
}

int
has_unused_register_with_class(reglist_t used_registers,
                               sgxwasm_register_class_t rc,
                               reglist_t pinned)
{
  reglist_t candidates = get_cache_reg_list(rc);
  return has_unused_register(candidates, used_registers, pinned);
}

int
has_unused_register(reglist_t candidates,
                    reglist_t used_registers,
                    reglist_t pinned)
{
  reglist_t available_regs;

  available_regs = mask_out(candidates, used_registers);
  available_regs = mask_out(available_regs, pinned);
  return !is_empty(available_regs);
}

sgxwasm_register_t
unused_register_with_class(reglist_t used_registers,
                           sgxwasm_register_class_t rc,
                           reglist_t pinned)
{
  reglist_t candidates = get_cache_reg_list(rc);
  return unused_register(candidates, used_registers, pinned);
}

sgxwasm_register_t
unused_register(reglist_t candidates,
                reglist_t used_registers,
                reglist_t pinned)
{
  reglist_t available_regs;

  available_regs = mask_out(candidates, used_registers);
  available_regs = mask_out(available_regs, pinned);
  return get_first_reg_set(available_regs);
}

// End of reference implementation of LiftoffRegList class

void
inc_used(reglist_t* used_registers,
         uint32_t* register_use_count,
         sgxwasm_register_t reg)
{
  assert(used_registers != NULL && register_use_count != NULL &&
         reg < RegisterNum);

  set(used_registers, reg);
  assert(register_use_count[reg] < SGXWASM_INT_MAX);
  register_use_count[reg]++;
}

void
dec_used(reglist_t* used_registers,
         uint32_t* register_use_count,
         sgxwasm_register_t reg)
{
  assert(used_registers != NULL && register_use_count != NULL &&
         reg < RegisterNum);
  assert(is_used(*used_registers, register_use_count, reg));
  assert(register_use_count[reg] > 0);

  register_use_count[reg]--;
  if (register_use_count[reg] == 0) {
    clear(used_registers, reg);
  }
}

int
is_used(reglist_t used_registers,
        uint32_t* register_use_count,
        sgxwasm_register_t reg)
{
  assert(register_use_count != NULL && reg < RegisterNum);
  int used = has(used_registers, reg);
  assert(used == (register_use_count[reg] != 0));
  return used;
}

uint32_t
get_use_count(uint32_t* register_use_count, sgxwasm_register_t reg)
{
  assert(register_use_count != NULL && reg < RegisterNum);
  return register_use_count[reg];
}

void
clear_used(reglist_t* used_registers,
           uint32_t* register_use_count,
           sgxwasm_register_t reg)
{
  assert(used_registers != NULL && register_use_count != NULL &&
         reg < RegisterNum);
  register_use_count[reg] = 0;
  clear(used_registers, reg);
}

int
is_free(reglist_t used_registers,
        uint32_t* register_use_count,
        sgxwasm_register_t reg)
{
  return !is_used(used_registers, register_use_count, reg);
}

void
reset_used_registers(reglist_t* used_registers, uint32_t* register_use_count)
{
  assert(used_registers != NULL && register_use_count != NULL);
  *used_registers = 0;
  memset(register_use_count, 0, RegisterNum * sizeof(uint32_t));
  // Set non-allocoable registers
  *used_registers = AllocableRegList;
}

sgxwasm_register_t
get_next_spill_reg(reglist_t used_registers,
                   reglist_t* last_spilled_regs,
                   reglist_t candidates,
                   reglist_t pinned)
{
  // Always pinned the allocable register list.
  reglist_t unpinned = mask_out(candidates, pinned | AllocableRegList);
  assert(!is_empty(unpinned));
  // This method should only be called if none of the candidates is free.
  assert(is_empty(mask_out(unpinned, used_registers)));
  reglist_t unspilled = mask_out(unpinned, *last_spilled_regs);
  if (is_empty(unspilled)) {
    unspilled = unpinned;
    *last_spilled_regs = 0;
  }
  sgxwasm_register_t reg = get_first_reg_set(unspilled);
  set(last_spilled_regs, reg);
  return reg;
}

int
reg_class_for(sgxwasm_valtype_t type)
{
  switch (type) {
    case VALTYPE_I32:
    case VALTYPE_I64:
      return GP_REG;
    case VALTYPE_F32:
    case VALTYPE_F64:
      return FP_REG;
    default:
      return NO_REG;
  }
}

int
reg_class(sgxwasm_register_t reg)
{
  if (is_gp(reg)) {
    return GP_REG;
  } else if (is_fp(reg)) {
    return FP_REG;
  } else {
    return NO_REG;
  }
}

void
dump_reg_info(uint32_t* register_use_count, sgxwasm_register_t reg)
{
  if (reg >= RegisterNum) {
    printf("REG_UNKNOWN ");
    return;
  }

  char name[6] = { 0 };
  switch (reg) {
    case GP_RAX:
      memcpy(name, "RAX", 3);
      break;
    case GP_RCX:
      memcpy(name, "RCX", 3);
      break;
    case GP_RDX:
      memcpy(name, "RDX", 3);
      break;
    case GP_RBX:
      memcpy(name, "RBX", 3);
      break;
    case GP_RSP:
      memcpy(name, "RSP", 3);
      break;
    case GP_RBP:
      memcpy(name, "RBP", 3);
      break;
    case GP_RSI:
      memcpy(name, "RSI", 3);
      break;
    case GP_RDI:
      memcpy(name, "RDI", 3);
      break;
    case GP_R8:
      memcpy(name, "R8", 2);
      break;
    case GP_R9:
      memcpy(name, "R9", 2);
      break;
    case GP_R10:
      memcpy(name, "R10", 3);
      break;
    case GP_R11:
      memcpy(name, "R11", 3);
      break;
    case GP_R12:
      memcpy(name, "R12", 3);
      break;
    case GP_R13:
      memcpy(name, "R13", 3);
      break;
    case GP_R14:
      memcpy(name, "R14", 3);
      break;
    case GP_R15:
      memcpy(name, "R15", 3);
      break;
    case FP_XMM0:
      memcpy(name, "XMM0", 4);
      break;
    case FP_XMM1:
      memcpy(name, "XMM1", 4);
      break;
    case FP_XMM2:
      memcpy(name, "XMM2", 4);
      break;
    case FP_XMM3:
      memcpy(name, "XMM3", 4);
      break;
    case FP_XMM4:
      memcpy(name, "XMM4", 4);
      break;
    case FP_XMM5:
      memcpy(name, "XMM5", 4);
      break;
    case FP_XMM6:
      memcpy(name, "XMM6", 4);
      break;
    case FP_XMM7:
      memcpy(name, "XMM7", 4);
      break;
    case FP_XMM8:
      memcpy(name, "XMM8", 4);
      break;
    case FP_XMM9:
      memcpy(name, "XMM9", 4);
      break;
    case FP_XMM10:
      memcpy(name, "XMM10", 5);
      break;
    case FP_XMM11:
      memcpy(name, "XMM11", 5);
      break;
    case FP_XMM12:
      memcpy(name, "XMM12", 5);
      break;
    case FP_XMM13:
      memcpy(name, "XMM13", 5);
      break;
    case FP_XMM14:
      memcpy(name, "XMM14", 5);
      break;
    case FP_XMM15:
      memcpy(name, "XMM15", 5);
      break;
    default:
      memcpy(name, "ERROR", 5);
      break;
  }

  if (register_use_count != NULL) {
    printf("%s (count: %u)", name, register_use_count[reg]);
  } else {
    printf("%s", name);
  }
}
