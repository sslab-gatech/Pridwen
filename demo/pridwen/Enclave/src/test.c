#include "ocall_stub.h"
#include <string.h>

char *test()
{
  char *buf;
  printf("test!!!\n");

  buf = (char *)malloc(4000);

  printf("before: @buf %p\n", (uint64_t)buf);

  return buf;
}
