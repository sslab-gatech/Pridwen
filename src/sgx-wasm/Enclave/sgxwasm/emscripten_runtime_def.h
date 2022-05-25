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

#define DEFINE_WASM_I32_GLOBAL(_name, _init, _mut)                             \
  DEFINE_WASM_GLOBAL(_name, _init, VALTYPE_I32, i32, _mut)
#define DEFINE_WASM_F64_GLOBAL(_name, _init, _mut)                             \
  DEFINE_WASM_GLOBAL(_name, _init, VALTYPE_F64, f64, _mut)

#define CURRENT_MODULE global

START_MODULE()

START_TABLE_DEFS()
END_TABLE_DEFS()

START_MEMORY_DEFS()
END_MEMORY_DEFS()

START_GLOBAL_DEFS()
DEFINE_WASM_F64_GLOBAL(NaN, NAN, 0)
DEFINE_WASM_F64_GLOBAL(Infinity, INFINITY, 0)
END_GLOBAL_DEFS()

START_FUNCTION_DEFS()
END_FUNCTION_DEFS()

END_MODULE()

#undef CURRENT_MODULE

//#if 0
#define CURRENT_MODULE global.Math

START_MODULE()

START_TABLE_DEFS()
END_TABLE_DEFS()

START_MEMORY_DEFS()
END_MEMORY_DEFS()

START_GLOBAL_DEFS()
END_GLOBAL_DEFS()

#define DEFINE_EMSCRIPTEN_FUNCTION(_name, _output, _n, ...)                    \
  DEFINE_WASM_FUNCTION(_name, &(emscripten_##_name), _output, _n, ##__VA_ARGS__)

START_FUNCTION_DEFS()
DEFINE_EMSCRIPTEN_FUNCTION(pow, VALTYPE_F64, 2, VALTYPE_F64, VALTYPE_F64)
DEFINE_EMSCRIPTEN_FUNCTION(exp, VALTYPE_F64, 1, VALTYPE_F64)
END_FUNCTION_DEFS()

END_MODULE()

#undef DEFINE_EMSCRIPTEN_FUNCTION

#undef CURRENT_MODULE
//#endif

#define CURRENT_MODULE env

START_MODULE()

START_TABLE_DEFS()
DEFINE_EXTERNAL_WASM_TABLE(table)
END_TABLE_DEFS()

START_MEMORY_DEFS()
DEFINE_WASM_MEMORY(memory, 256, 256)
END_MEMORY_DEFS()

START_GLOBAL_DEFS()
DEFINE_EXTERNAL_WASM_GLOBAL(memoryBase)
DEFINE_EXTERNAL_WASM_GLOBAL(tempDoublePtr)
DEFINE_EXTERNAL_WASM_GLOBAL(DYNAMICTOP_PTR)
DEFINE_EXTERNAL_WASM_GLOBAL(STACKTOP)
DEFINE_EXTERNAL_WASM_GLOBAL(STACK_MAX)
DEFINE_WASM_I32_GLOBAL(tableBase, 0, 0)
DEFINE_WASM_I32_GLOBAL(ABORT, 0, 0)
END_GLOBAL_DEFS()

#define DEFINE_EMSCRIPTEN_FUNCTION(_name, _output, _n, ...)                    \
  DEFINE_WASM_FUNCTION(_name, &(emscripten_##_name), _output, _n, ##__VA_ARGS__)

START_FUNCTION_DEFS()
DEFINE_WASM_FUNCTIONS(DEFINE_EMSCRIPTEN_FUNCTION)
END_FUNCTION_DEFS()

END_MODULE()

#undef DEFINE_EMSCRIPTEN_FUNCTION

#undef CURRENT_MODULE

#undef DEFINE_WASM_F64_GLOBAL
#undef DEFINE_WASM_I32_GLOBAL
