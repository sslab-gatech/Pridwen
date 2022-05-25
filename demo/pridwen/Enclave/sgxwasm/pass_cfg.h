#ifndef __SGXWASM__PASS_CFG_H__
#define __SGXWASM__PASS_CFG_H__

struct Pass;

enum ControlState
{
  FunctionStart = 0xf0,
  ControlStart = 0xf1,
  ControlEnd = 0xf2,
  BranchEnd = 0xf3,
};

enum TargetType
{
  TargetKnown = 0xe0,
  TargetLoopKnown = 0xe1,
  IfUnknown = 0xe2,
  ElseUnknown = 0xe3,
  BrUnknown = 0xe4,
  TargetNone = 0xe5,
};

struct ControlFlowGraph
{
  size_t capacity;
  size_t size;
  struct FunctionCFG
  {
    size_t id;
  	size_t capacity;
  	size_t size;
    struct CFGNode
    {
      size_t id;
      size_t offset;
      size_t size;
      size_t depth;
      size_t low_instr_offset;
      size_t low_instr_num;
      control_type_t control_type;
      uint8_t control_state;
      // CFG target list
      struct CFGTargetList
      {
        size_t capacity;
        size_t size;
        struct CFGTarget
        {
          size_t offset;
          size_t id;
          size_t type;
          size_t depth; // Relative depth.
          control_type_t control_type;
          uint8_t control_state;
        } * data;
      } target_list;
    } * data;
  } * data;
};

DECLARE_VECTOR_INIT(cfg_target_list, struct CFGTargetList);
DECLARE_VECTOR_GROW(fun_cfg, struct FunctionCFG);


struct CFGTarget*
new_target(struct CFGNode*);
struct CFGTarget*
get_target(struct CFGNode*);

struct ControlFlowGraph*
get_cfg(struct Pass*);

#endif
