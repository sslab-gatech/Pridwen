/* -*-mode:c; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
  Copyright (c) 2018 Rian Hunter et. al, see AUTHORS file.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
 */

#ifndef __SGXWASM__EMSCRIPTEN_RUNTIME_SYS_H__
#define __SGXWASM__EMSCRIPTEN_RUNTIME_SYS_H__

#include <errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct msghdr user_msghdr_t;

#define SYS_CMSG_NXTHDR(msg, cmsg) CMSG_NXTHDR((msg), (cmsg))

#include <sgxwasm/util.h>

/*#if __linux__
int
llseek(uint32_t fd,
       uint32_t offset_high,
       uint32_t offset_low,
       uint64_t* result,
       uint32_t whence)
{
  //syscall(140, fd, offset_high, offset_low, result, whence);
  (void) fd;
  (void) offset_high;
  (void) offset_low;
  (void) result;
  (void) whence;
  return 0;
}
#endif*/

/* declare all sys calls */

#define __KDECL(to, n, t) t _##n

#define KWSCx(_n, _name, ...)                                                  \
  long sys_##_name(__KMAP(_n, __KDECL, __VA_ARGS__));

#define KWSC1(name, ...) KWSCx(1, name, __VA_ARGS__)
#define KWSC2(name, ...) KWSCx(2, name, __VA_ARGS__)
#define KWSC3(name, ...) KWSCx(3, name, __VA_ARGS__)
#define KWSC4(name, ...) KWSCx(4, name, __VA_ARGS__)
#define KWSC5(name, ...) KWSCx(5, name, __VA_ARGS__)
#define KWSC6(name, ...) KWSCx(6, name, __VA_ARGS__)

#include <sgxwasm/emscripten_runtime_sys_def.h>

#undef KWSC1
#undef KWSC2
#undef KWSC3
#undef KWSC5
#undef KWSC6
#undef KWSCx
#undef __KDECL

#endif
