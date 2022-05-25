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

#include <sgxwasm/high_level.h>

#include <sgxwasm/dynamic_emscripten_runtime.h>
#include <sgxwasm/instantiate.h>
#include <sgxwasm/parse.h>
#include <sgxwasm/sys.h>
#include <sgxwasm/util.h>

static int
add_named_module(struct WasmJITHigh* self,
                 const char* module_name,
                 struct Module* module)
{
  int ret;
  char* new_module_name = NULL;
  struct NamedModule* new_named_modules = NULL;
  self->n_modules += 1;

  new_module_name = strdup(module_name);
  if (!new_module_name)
    goto error;

  new_named_modules =
    realloc(self->modules, self->n_modules * sizeof(self->modules[0]));
  if (!new_named_modules) {
    goto error;
  }

  new_named_modules[self->n_modules - 1].name = new_module_name;
  new_named_modules[self->n_modules - 1].module = module;
  self->modules = new_named_modules;
  new_module_name = NULL;
  new_named_modules = NULL;

  if (0) {
  error:
    ret = 0;
  } else {
    ret = 1;
  }

  if (new_named_modules)
    free(new_named_modules);

  if (new_module_name)
    free(new_module_name);

  return ret;
}

int
sgxwasm_high_init(struct WasmJITHigh* self)
{
  self->n_modules = 0;
  self->modules = NULL;
  self->emscripten_asm_module = NULL;
  self->emscripten_env_module = NULL;
  memset(self->error_buffer, 0, sizeof(self->error_buffer));
  return 0;
}

static int
sgxwasm_high_instantiate_buf(struct WasmJITHigh* self,
                             struct PassManager* pm,
                             const char* buf,
                             size_t size,
                             const char* module_name,
                             uint32_t flags)
{
  int ret;
  struct ParseState pstate;
  struct WASMModule wasm_module;
  struct Module* module = NULL;

  (void)flags;

  sgxwasm_init_wasm_module(&wasm_module);

  if (!init_pstate(&pstate, buf, size)) {
    goto error;
  }

  if (!read_wasm_module(&pstate, &wasm_module, NULL, 0)) {
    goto error;
  }

  /* TODO: validate module */

  module = sgxwasm_instantiate(&wasm_module,
                               pm,
                               self->n_modules,
                               self->modules,
                               self->error_buffer,
                               sizeof(self->error_buffer));
  if (!module) {
    goto error;
  }

  if (!add_named_module(self, module_name, module)) {
    goto error;
  }
  module = NULL;

  if (0) {
  error:
    ret = -1;
  } else {
    ret = 0;
  }

  sgxwasm_free_wasm_module(&wasm_module);

  if (module) {
    sgxwasm_free_module(module);
  }

  return ret;
}

int
sgxwasm_high_instantiate(struct WasmJITHigh* self,
                         struct PassManager* pm,
                         const char* filename,
                         const char* module_name,
                         uint32_t flags)
{
  int ret;
  size_t size;
  char* buf;

  self->error_buffer[0] = '\0';

  /* TODO: do incremental reading */

  buf = sgxwasm_load_file(filename, &size);
  if (!buf)
    goto error;

  ret = sgxwasm_high_instantiate_buf(self, pm, buf, size, module_name, flags);

  if (0) {
  error:
    ret = -1;
  }

  if (buf)
    sgxwasm_unload_file(buf, size);

  return ret;
}

int
sgxwasm_high_instantiate_emscripten_runtime(struct WasmJITHigh* self,
                                            struct EmscriptenContext* ctx,
                                            uint32_t static_bump,
                                            size_t tablemin,
                                            size_t tablemax,
                                            uint32_t flags)
{
  int ret, has_table;
  size_t n_modules, i;
  struct NamedModule* modules = NULL;
  struct Memory* mem = NULL;

  self->error_buffer[0] = '\0';

  has_table =
    !(flags & SGXWASM_HIGH_INSTANTIATE_EMSCRIPTEN_RUNTIME_FLAGS_NO_TABLE);

  modules = sgxwasm_instantiate_emscripten_runtime(
    ctx, static_bump, has_table, tablemin, tablemax, &n_modules);
  if (!modules) {
    goto error;
  }

  for (i = 0; i < n_modules; ++i) {
    if (!add_named_module(self, modules[i].name, modules[i].module)) {
      goto error;
    }

    /* TODO: delegate to emscripten_runtime how to find
       emscripten cleanup module */
    if (!strcmp(modules[i].name, "env")) {
      self->emscripten_env_module = modules[i].module;
    }

    modules[i].module = NULL;
  }

  // TODO: Find better way to do this.
  mem = self->emscripten_env_module->mems.data[0];
  emscripten_setMemBase((uint64_t)mem->data, mem->size);

  if (emscripten_setDynamicBase(ctx) != 0) {
    printf("emscripten_setDynamicBase failed\n");
    goto error;
  }

  ret = 0;

  if (0) {
  error:
    ret = -1;
  }

  if (modules) {
    for (i = 0; i < n_modules; ++i) {
      free(modules[i].name);
      if (modules[i].module)
        sgxwasm_free_module(modules[i].module);
    }
    free(modules);
  }

  return ret;
}

int
sgxwasm_high_emscripten_invoke_main(struct WasmJITHigh* self,
                                    struct EmscriptenContext* ctx,
                                    const char* module_name,
                                    int argc,
                                    char** argv,
                                    char** envp,
                                    uint32_t flags)
{
  size_t i;
  struct Module* env_module;
  struct Function *em_main, *em_stack_alloc, *environ_constructor;
  struct Memory* mem;
  struct Module* module;
  int ret;

  (void)flags;

  self->error_buffer[0] = '\0';

  module = NULL;
  for (i = 0; i < self->n_modules; ++i) {
    if (!strcmp(self->modules[i].name, module_name)) {
      module = self->modules[i].module;
    }
  }
  if (!module)
    return -1;

  env_module = NULL;
  for (i = 0; i < self->n_modules; ++i) {
    if (!strcmp(self->modules[i].name, "env")) {
      env_module = self->modules[i].module;
      break;
    }
  }
  if (!env_module)
    return -1;

  em_main = sgxwasm_get_export(module, "_main", IMPORT_DESC_TYPE_FUNC).func;
  if (!em_main)
    return -1;
  em_stack_alloc =
    sgxwasm_get_export(module, "stackAlloc", IMPORT_DESC_TYPE_FUNC).func;
  if (!em_stack_alloc)
    return -1;
  mem = sgxwasm_get_export(env_module, "memory", IMPORT_DESC_TYPE_MEM).mem;
  if (!mem)
    return -1;

  if (self->emscripten_env_module) {
    assert(self->emscripten_env_module == env_module);
    if (!self->emscripten_asm_module) {
      struct Function *em_errno_location, *em_malloc, *em_free;

      em_errno_location =
        sgxwasm_get_export(module, "___errno_location", IMPORT_DESC_TYPE_FUNC)
          .func;

      em_malloc =
        sgxwasm_get_export(module, "_malloc", IMPORT_DESC_TYPE_FUNC).func;
      if (!em_malloc)
        return -1;

      em_free = sgxwasm_get_export(module, "_free", IMPORT_DESC_TYPE_FUNC).func;

      if (emscripten_context_set_imports(
            ctx, em_errno_location, em_malloc, em_free, envp))
        return -1;

      self->emscripten_asm_module = module;
    }

    if (self->emscripten_asm_module != module) {
      ret = -1;
      goto error;
    }
  }

  environ_constructor = sgxwasm_get_export(module,
                                           "___emscripten_environ_constructor",
                                           IMPORT_DESC_TYPE_FUNC)
                          .func;
  ret = emscripten_build_environment(environ_constructor);
  if (ret) {
    ret = -1;
    goto error;
  }

  ret = emscripten_invoke_main(em_stack_alloc, em_main, argc, argv);
error:
  return ret;
}

void
sgxwasm_high_close(struct WasmJITHigh* self)
{
  size_t i;

  if (self->emscripten_env_module)
    emscripten_cleanup(self->emscripten_env_module);

  self->error_buffer[0] = '\0';

  for (i = 0; i < self->n_modules; ++i) {
    free(self->modules[i].name);
    sgxwasm_free_module(self->modules[i].module);
  }
  if (self->modules)
    free(self->modules);
}

int
sgxwasm_high_error_message(struct WasmJITHigh* self, char* buf, size_t buf_size)
{
  strncpy(buf, self->error_buffer, buf_size);
  if (buf_size > 0)
    buf[buf_size - 1] = '\0';

  return 0;
}
