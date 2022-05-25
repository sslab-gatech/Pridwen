#ifndef __USE_GNU
#define __USE_GNU 1
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <sys/socket.h>
#include <sys/types.h>

int main()
{
  struct sockaddr addr;
  socklen_t len;

  accept4(3, &addr, &len, 0);

  return 0;
}
