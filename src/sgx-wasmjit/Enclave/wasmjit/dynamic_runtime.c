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

#include <wasmjit/runtime.h>

#include <wasmjit/sys.h>

int64_t wasmjit_code_pointer = (int64_t)__WASMJIT_CODE_BASE;
void *wasmjit_map_code_segment(size_t code_size)
{
  uint64_t code_start, code_end;

  code_start = wasmjit_code_pointer;
  code_end = wasmjit_code_pointer + code_size;

  if (code_end > (int64_t) __WASMJIT_CODE_END) {
    printf("wasmjit_code is full\n");
    return NULL; 
  }

  wasmjit_code_pointer = code_end;

  return code_start;
}

// XXX: Not supported in SGX.
int wasmjit_mark_code_segment_executable(void *code, size_t code_size)
{
  return 1;
}


// XXX: Not supported in SGX.
int wasmjit_unmap_code_segment(void *code, size_t code_size)
{
  return 1;
}

#if 0
wasmjit_tls_key_t jmp_buf_key;

__attribute__((constructor))
static void _init_jmp_buf(void)
{
	wasmjit_init_tls_key(&jmp_buf_key, NULL);
}

jmp_buf *wasmjit_get_jmp_buf(void)
{
	jmp_buf *toret;
	int ret;
	ret = wasmjit_get_tls_key(jmp_buf_key, &toret);
	if (!ret) return NULL;
	return toret;
}

int wasmjit_set_jmp_buf(jmp_buf *jmpbuf)
{
	return wasmjit_set_tls_key(jmp_buf_key, jmpbuf);
}

wasmjit_tls_key_t stack_top_key;

__attribute__((constructor))
static void _init_stack_top(void)
{
	wasmjit_init_tls_key(&stack_top_key, NULL);
}
#endif

// FIXME: hard-code correct value
uint64_t _wasmjit_stack_top = 0;
void *wasmjit_stack_top(void)
{
    return _wasmjit_stack_top;
}

int wasmjit_set_stack_top(void *stack_top)
{
  if (!stack_top)
    _wasmjit_stack_top = stack_top;

   return 0;
}

__attribute__((noreturn))
void wasmjit_trap(int reason)
{
	//assert(reason);
	//longjmp(*wasmjit_get_jmp_buf(), reason);
  printf("wasmjit trap, reason: %d\n", reason);
}

int wasmjit_invoke_function(struct FuncInst *funcinst,
			    union ValueUnion *values,
			    union ValueUnion *out)
{
  union ValueUnion lout;
  int ret;

  lout = wasmjit_invoke_function_raw(funcinst, values);
  if (out)
    *out = lout;
  ret = 0;

  return ret;
}
