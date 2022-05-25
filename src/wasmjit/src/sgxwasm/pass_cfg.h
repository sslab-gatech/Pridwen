#ifndef __SGXWASM__PASS_CFG_H__
#define __SGXWASM__PASS_CFG_H__

struct Pass;

enum ControlState
{
  FunctionStart = 0xf0,
  ControlStart = 0xf1,
  ControlEnd = 0xf2
};

enum TargetType
{
  TargetKnown = 0xe0,
  IfUnknown = 0xe1,
  ElseUnknown = 0xe2,
  BrUnknown = 0xe3,
  TargetNone = 0xe4,
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