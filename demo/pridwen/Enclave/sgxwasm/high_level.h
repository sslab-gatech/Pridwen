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

#ifndef __SGXWASM__HIGH_LEVEL_H
#define __SGXWASM__HIGH_LEVEL_H

#include <sgxwasm/emscripten.h>
#include <sgxwasm/sys.h>
#include <sgxwasm/pass.h>
#include <sgxwasm/sense.h>

/* this interface mimics the kernel interface and thus lacks power
   since we can't pass in abitrary objects for import, like host functions */

struct WasmJITHigh
{
  size_t n_modules;
  struct NamedModule* modules;
  char error_buffer[256];
  struct Module* emscripten_asm_module;
  struct Module* emscripten_env_module;
};

#define SGXWASM_HIGH_INSTANTIATE_EMSCRIPTEN_RUNTIME_FLAGS_NO_TABLE 1

int
sgxwasm_high_init(struct WasmJITHigh* self);
int
sgxwasm_high_instantiate(struct WasmJITHigh* self,
                         struct PassManager *pm,
                         struct SystemConfig *config,
                         const char* filename,
                         const char* module_name,
                         uint32_t flags);
int
sgxwasm_high_instantiate_emscripten_runtime(struct WasmJITHigh* self,
                                            struct EmscriptenContext* ctx,
                                            uint32_t static_bump,
                                            size_t tablemin,
                                            size_t tablemax,
                                            uint32_t flags);
int
sgxwasm_high_emscripten_invoke_main(struct WasmJITHigh* self,
                                    struct EmscriptenContext* ctx,
                                    const char* module_name,
                                    int argc,
                                    char** argv,
                                    char** envp,
                                    uint32_t flags);
void
sgxwasm_high_close(struct WasmJITHigh* self);
int
sgxwasm_high_error_message(struct WasmJITHigh* self,
                           char* buf,
                           size_t buf_size);

#endif
