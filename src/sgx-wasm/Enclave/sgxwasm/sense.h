#ifndef __SGXWASM__SENSE_H__
#define __SGXWASM__SENSE_H__

#include <sgxwasm/config.h>
#include <sgxwasm/sys.h>

struct SystemConfig {
  uint64_t exittype_addr;
  int tsx_support;
};

void init_system_sensing(struct SystemConfig*);
void system_sensing(struct SystemConfig*);
void finalize_system_sensing();

void dump_system_config(struct SystemConfig*);

#endif
