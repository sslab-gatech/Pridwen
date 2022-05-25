#include <sgxwasm/emscripten.h>
#include <sgxwasm/high_level.h>
#include <sgxwasm/instantiate.h>
#include <sgxwasm/parse.h>
#include <sgxwasm/pass.h>
#include <sgxwasm/runtime.h>
#include <sgxwasm/sense.h>
#include <sgxwasm/util.h>

#if !__SGX__
#include <stdio.h>
#endif

#define DUMP_CODE 0

__attribute__((unused)) static void
dump_spec_test(const char* fun, uint64_t* args, uint8_t* args_type,
               size_t n_args, uint64_t expected, uint8_t expected_type)
{
  printf("[spec test] fun: %s args: ", fun);
  if (n_args == 0) {
    printf("none ");
  } else {
    for (size_t i = 0; i < n_args; i++) {
      switch (args_type[i]) {
        case 0:
          printf("%zu: %u (i32) ", i, (uint32_t)args[i]);
          break;
        case 1:
          printf("%zu: %lu (i64) ", i, args[i]);
          break;
        case 2: {
          float f;
          memcpy(&f, &args[i], sizeof(uint32_t));
          printf("%zu: %f (f32) ", i, f);
          break;
        }
        case 3: {
          double f;
          memcpy(&f, &args[i], sizeof(uint64_t));
          printf("%zu: %lf (f64) ", i, f);
          break;
        }
        default:
          printf("%zu: unknown ", i);
          break;
      }
    }
  }
  switch (expected_type) {
    case 0:
      printf("expected: %u (i32)", (uint32_t)expected);
      break;
    case 1:
      printf("expected: %lu (i64)", expected);
      break;
    case 2: {
      float f;
      memcpy(&f, &expected, sizeof(uint32_t));
      printf("expected: %f (f32)", f);
      break;
    }
    case 3: {
      double f;
      memcpy(&f, &expected, sizeof(uint64_t));
      printf("expected: %lf (f64)", f);
      break;
    }
    case 4: {
      printf("expected: void");
      break;
    }
    default:
      printf("expected: unknown");
      break;
  }
  printf("\n");
}

int
test_sum(int x, int y)
{
  return x + y;
}

__attribute__((unused)) static void
dump_compile_code(uint8_t* code, size_t size)
{
  size_t i;
  for (i = 0; i < size; i++) {
    uint8_t byte = code[i];
    if (byte < 16) {
      printf("0%x ", byte);
      continue;
    }
    printf("%x ", byte);
  }
  printf("\n");
}

static void
test_void_start(void)
{
  printf("test_void... ");
}

static void
test_void_end(void)
{
  printf("pass\n");
}

static void
test_i32(int lv, int rv, int* count)
{
  if (count != NULL) {
    printf("case %d ... ", ++(*count));
  } else {
    printf("test_i32... ");
  }
  if (lv == rv) {
    printf("pass: %d\n", lv);
  } else {
    printf("fail, expected: %d, returned: %d\n", rv, lv);
  }
}

static void
test_i64(long lv, long rv, int* count)
{
  if (count != NULL) {
    printf("case %d ... ", ++(*count));
  } else {
    printf("test_i64... ");
  }
  if (lv == rv) {
    printf("pass\n");
  } else {
    printf("fail, expected: %ld, returned: %ld\n", rv, lv);
  }
}

static void
test_f32(float lv, float rv, int* count)
{
  if (count != NULL) {
    printf("case %d ... ", ++(*count));
  } else {
    printf("test_f32... ");
  }
  if (lv == rv) {
    printf("pass\n");
  } else if (isnan(rv) && isnan(lv)) {
    printf("pass\n");
  } else if ((int32_t)rv == (uint32_t)lv) {
    printf("pass\n");
  } else {
    printf("fail, expected: %f, returned: %f\n", rv, lv);
  }
}

static void
test_f64(double lv, double rv, int* count)
{
  if (count != NULL) {
    printf("case %d ... ", ++(*count));
  } else {
    printf("test_f64... ");
  }
  if (lv == rv) {
    printf("pass\n");
  } else if (isnan(rv) && isnan(lv)) {
    printf("pass\n");
  } else if ((int64_t)rv == (uint64_t)lv) {
    printf("pass\n");
  } else {
    printf("fail, expected: %lf, returned: %lf\n", rv, lv);
  }
}

__attribute__((unused)) static void
test_ret_void(void* fun_ptr, uint64_t* args, uint8_t* args_type, size_t n_args)
{
  size_t i;

  switch (n_args) {
    case 0: {
      void((*fun)()) = fun_ptr;
      test_void_start();
      fun();
      test_void_end();
      break;
    }
    case 1: {
      switch (args_type[0]) {
        case 0: {
          void((*fun)(uint32_t)) = fun_ptr;
          test_void_start();
          fun((uint32_t)args[0]);
          test_void_end();
          break;
        }
        case 1: {
          void((*fun)(uint64_t)) = fun_ptr;
          test_void_start();
          fun(args[0]);
          test_void_end();
          break;
        }
        case 2: {
          void((*fun)(float)) = fun_ptr;
          float f;
          memcpy(&f, &args[0], sizeof(uint32_t));
          test_void_start();
          fun(f);
          test_void_end();
          break;
        }
        case 3: {
          void((*fun)(double)) = fun_ptr;
          double f;
          memcpy(&f, &args[0], sizeof(uint64_t));
          test_void_start();
          fun(f);
          test_void_end();
          break;
        }
        default:
          printf("arg 0: unknown");
          return;
      }
      break;
    }
    case 2: {
      if (args_type[0] == 0 && args_type[1] == 0) {
        void((*fun)(uint32_t, uint32_t)) = fun_ptr;
        test_void_start();
        fun((uint32_t)args[0], (uint32_t)args[1]);
        test_void_end();
      } else if (args_type[0] == 1 && args_type[1] == 1) {
        void((*fun)(uint64_t, uint64_t)) = fun_ptr;
        test_void_start();
        fun(args[0], args[1]);
        test_void_end();
      } else if (args_type[0] == 2 && args_type[1] == 2) {
        void((*fun)(float, float)) = fun_ptr;
        float f0, f1;
        memcpy(&f0, &args[0], sizeof(uint32_t));
        memcpy(&f1, &args[1], sizeof(uint32_t));
        test_void_start();
        fun(f0, f1);
        test_void_end();
      } else if (args_type[0] == 3 && args_type[1] == 3) {
        void((*fun)(double, double)) = fun_ptr;
        double f0, f1;
        memcpy(&f0, &args[0], sizeof(uint64_t));
        memcpy(&f1, &args[1], sizeof(uint64_t));
        test_void_start();
        fun(f0, f1);
        test_void_end();
      } else {
        printf("unsupported 2 args types (%u, %u)\n", args_type[0],
               args_type[1]);
      }
      break;
    }
    default:
      printf("unsupported types: ret void, n_args - %zu\n", n_args);
      break;
  }
}

__attribute__((unused)) static void
test_ret_i32(void* fun_ptr, uint32_t ret, uint64_t* args, uint8_t* args_type,
             size_t n_args)
{
  size_t i;

  switch (n_args) {
    case 0: {
      uint32_t((*fun)()) = fun_ptr;
      test_i32(fun(), ret, NULL);
      break;
    }
    case 1: {
      switch (args_type[0]) {
        case 0: {
          uint32_t((*fun)(uint32_t)) = fun_ptr;
          test_i32(fun((uint32_t)args[0]), ret, NULL);
          break;
        }
        case 1: {
          uint32_t((*fun)(uint64_t)) = fun_ptr;
          test_i32(fun(args[0]), ret, NULL);
          break;
        }
        case 2: {
          uint32_t((*fun)(float)) = fun_ptr;
          float f;
          memcpy(&f, &args[0], sizeof(uint32_t));
          test_i32(fun(f), ret, NULL);
          break;
        }
        case 3: {
          uint32_t((*fun)(double)) = fun_ptr;
          double f;
          memcpy(&f, &args[0], sizeof(uint64_t));
          test_i32(fun(f), ret, NULL);
          break;
        }
        default:
          printf("arg 0: unknown");
          return;
      }
      break;
    }
    case 2: {
      if (args_type[0] == 0 && args_type[1] == 0) {
        uint32_t((*fun)(uint32_t, uint32_t)) = fun_ptr;
        test_i32(fun((uint32_t)args[0], (uint32_t)args[1]), ret, NULL);
      } else if (args_type[0] == 0 && args_type[1] == 1) {
        uint32_t((*fun)(uint32_t, uint64_t)) = fun_ptr;
        test_i32(fun((uint32_t)args[0], args[1]), ret, NULL);
      } else if (args_type[0] == 1 && args_type[1] == 1) {
        uint32_t((*fun)(uint64_t, uint64_t)) = fun_ptr;
        test_i32(fun(args[0], args[1]), ret, NULL);
      } else if (args_type[0] == 2 && args_type[1] == 2) {
        uint32_t((*fun)(float, float)) = fun_ptr;
        float f0, f1;
        memcpy(&f0, &args[0], sizeof(uint32_t));
        memcpy(&f1, &args[1], sizeof(uint32_t));
        test_i32(fun(f0, f1), ret, NULL);
      } else if (args_type[0] == 3 && args_type[1] == 3) {
        uint32_t((*fun)(double, double)) = fun_ptr;
        double f0, f1;
        memcpy(&f0, &args[0], sizeof(uint64_t));
        memcpy(&f1, &args[1], sizeof(uint64_t));
        test_i32(fun(f0, f1), ret, NULL);
      } else {
        printf("unsupported 2 args types (%u, %u)\n", args_type[0],
               args_type[1]);
      }
      break;
    }
    case 3: {
      if (args_type[0] == 0 && args_type[1] == 0 && args_type[2] == 0) {
        uint32_t((*fun)(uint32_t, uint32_t, uint32_t)) = fun_ptr;
        test_i32(fun((uint32_t)args[0], (uint32_t)args[1], (uint32_t)args[2]),
                 ret, NULL);
      }
      break;
    }
    default:
      printf("unsupported types: ret i32, n_args - %zu\n", n_args);
      break;
  }
}

__attribute__((unused)) static void
test_ret_i64(void* fun_ptr, uint64_t ret, uint64_t* args, uint8_t* args_type,
             size_t n_args)
{
  size_t i;

  switch (n_args) {
    case 0: {
      uint64_t((*fun)()) = fun_ptr;
      test_i64(fun(), ret, NULL);
      break;
    }
    case 1: {
      switch (args_type[0]) {
        case 0: {
          uint64_t((*fun)(uint32_t)) = fun_ptr;
          test_i64(fun((uint32_t)args[0]), ret, NULL);
          break;
        }
        case 1: {
          uint64_t((*fun)(uint64_t)) = fun_ptr;
          test_i64(fun(args[0]), ret, NULL);
          break;
        }
        case 2: {
          uint64_t((*fun)(float)) = fun_ptr;
          float f;
          memcpy(&f, &args[0], sizeof(uint32_t));
          test_i64(fun(f), ret, NULL);
          break;
        }
        case 3: {
          uint64_t((*fun)(double)) = fun_ptr;
          double f;
          memcpy(&f, &args[0], sizeof(uint64_t));
          test_i64(fun(f), ret, NULL);
          break;
        }
        default:
          printf("arg 0: unknown");
          return;
      }
      break;
    }
    case 2: {
      if (args_type[0] == 0 && args_type[1] == 0) {
        uint64_t((*fun)(uint32_t, uint32_t)) = fun_ptr;
        test_i64(fun((uint32_t)args[0], (uint32_t)args[1]), ret, NULL);
      } else if (args_type[0] == 0 && args_type[1] == 1) {
        uint64_t((*fun)(uint32_t, uint64_t)) = fun_ptr;
        test_i64(fun((uint32_t)args[0], args[1]), ret, NULL);
      } else if (args_type[0] == 1 && args_type[1] == 1) {
        uint64_t((*fun)(uint64_t, uint64_t)) = fun_ptr;
        test_i64(fun(args[0], args[1]), ret, NULL);
      } else if (args_type[0] == 2 && args_type[1] == 2) {
        uint64_t((*fun)(float, float)) = fun_ptr;
        float f0, f1;
        memcpy(&f0, &args[0], sizeof(uint32_t));
        memcpy(&f1, &args[1], sizeof(uint32_t));
        test_i64(fun(f0, f1), ret, NULL);
      } else if (args_type[0] == 3 && args_type[1] == 3) {
        uint64_t((*fun)(double, double)) = fun_ptr;
        double f0, f1;
        memcpy(&f0, &args[0], sizeof(uint64_t));
        memcpy(&f1, &args[1], sizeof(uint64_t));
        test_i64(fun(f0, f1), ret, NULL);
      } else {
        printf("unsupported 2 args types (%u, %u)\n", args_type[0],
               args_type[1]);
      }
      break;
    }
    case 3: {
      if (args_type[0] == 1 && args_type[1] == 1 && args_type[2] == 1) {
        uint64_t((*fun)(uint64_t, uint64_t, uint64_t)) = fun_ptr;
        test_i64(fun(args[0], args[1], args[2]), ret, NULL);
      } else if (args_type[0] == 1 && args_type[1] == 1 && args_type[2] == 0) {
        uint64_t((*fun)(uint64_t, uint64_t, uint32_t)) = fun_ptr;
        test_i64(fun(args[0], args[1], (uint32_t)args[2]), ret, NULL);
      } else {
        printf("unsupported 3 args types ret i64 - (%u, %u, %u)\n",
               args_type[0], args_type[1], args_type[2]);
      }
      break;
    }
    default:
      printf("unsupported types: ret i64, n_args - %zu\n", n_args);
      break;
  }
}

__attribute__((unused)) static void
test_ret_f32(void* fun_ptr, float ret, uint64_t* args, uint8_t* args_type,
             size_t n_args)
{
  size_t i;

  switch (n_args) {
    case 0: {
      float((*fun)()) = fun_ptr;
      test_f32(fun(), ret, NULL);
      break;
    }
    case 1: {
      switch (args_type[0]) {
        case 0: {
          float((*fun)(uint32_t)) = fun_ptr;
          test_f32(fun((uint32_t)args[0]), ret, NULL);
          break;
        }
        case 1: {
          float((*fun)(uint64_t)) = fun_ptr;
          test_f32(fun(args[0]), ret, NULL);
          break;
        }
        case 2: {
          float((*fun)(float)) = fun_ptr;
          float f;
          memcpy(&f, &args[0], sizeof(uint32_t));
          test_f32(fun(f), ret, NULL);
          break;
        }
        case 3: {
          float((*fun)(double)) = fun_ptr;
          double f;
          memcpy(&f, &args[0], sizeof(uint64_t));
          test_f32(fun(f), ret, NULL);
          break;
        }
        default:
          printf("arg 0: unknown");
          return;
      }
      break;
    }
    case 2: {
      if (args_type[0] == 0 && args_type[1] == 0) {
        float((*fun)(uint32_t, uint32_t)) = fun_ptr;
        test_f32(fun((uint32_t)args[0], (uint32_t)args[1]), ret, NULL);
      } else if (args_type[0] == 1 && args_type[1] == 1) {
        float((*fun)(uint64_t, uint64_t)) = fun_ptr;
        test_f32(fun(args[0], args[1]), ret, NULL);
      } else if (args_type[0] == 2 && args_type[1] == 2) {
        float((*fun)(float, float)) = fun_ptr;
        float f0, f1;
        memcpy(&f0, &args[0], sizeof(uint32_t));
        memcpy(&f1, &args[1], sizeof(uint32_t));
        test_f32(fun(f0, f1), ret, NULL);
      } else if (args_type[0] == 3 && args_type[1] == 3) {
        float((*fun)(double, double)) = fun_ptr;
        double f0, f1;
        memcpy(&f0, &args[0], sizeof(uint64_t));
        memcpy(&f1, &args[1], sizeof(uint64_t));
        test_f32(fun(f0, f1), ret, NULL);
      } else if (args_type[0] == 2 && args_type[1] == 3) {
        float((*fun)(float, double)) = fun_ptr;
        float f0;
        double f1;
        memcpy(&f0, &args[0], sizeof(uint32_t));
        memcpy(&f1, &args[1], sizeof(uint64_t));
        test_f32(fun(f0, f1), ret, NULL);
      } else if (args_type[0] == 3 && args_type[1] == 2) {
        float((*fun)(double, float)) = fun_ptr;
        double f0;
        float f1;
        memcpy(&f0, &args[0], sizeof(uint64_t));
        memcpy(&f1, &args[1], sizeof(uint32_t));
        test_f32(fun(f0, f1), ret, NULL);
      } else {
        printf("unsupported 2 args types (%u, %u)\n", args_type[0],
               args_type[1]);
      }
      break;
    }
    case 3: {
      if (args_type[0] == 2 && args_type[1] == 2 && args_type[2] == 2) {
        float((*fun)(float, float, float)) = fun_ptr;
        float f0, f1, f2;
        memcpy(&f0, &args[0], sizeof(uint64_t));
        memcpy(&f1, &args[1], sizeof(uint64_t));
        memcpy(&f2, &args[2], sizeof(uint64_t));
        test_f32(fun(f0, f1, f2), ret, NULL);
      } else if (args_type[0] == 2 && args_type[1] == 2 && args_type[2] == 0) {
        float((*fun)(float, float, uint32_t)) = fun_ptr;
        float f0, f1;
        memcpy(&f0, &args[0], sizeof(uint64_t));
        memcpy(&f1, &args[1], sizeof(uint64_t));
        test_f32(fun(f0, f1, (uint32_t)args[2]), ret, NULL);
      } else {
        printf("unsupported 3 args types ret f32 - (%u, %u, %u)\n",
               args_type[0], args_type[1], args_type[2]);
      }
      break;
    }
    case 4: {
      if (args_type[0] == 2 && args_type[1] == 2 && args_type[2] == 2 &&
          args_type[3] == 2) {
        float((*fun)(float, float, float, float)) = fun_ptr;
        float f0, f1, f2, f3;
        memcpy(&f0, &args[0], sizeof(uint32_t));
        memcpy(&f1, &args[1], sizeof(uint32_t));
        memcpy(&f2, &args[2], sizeof(uint32_t));
        memcpy(&f3, &args[3], sizeof(uint32_t));
        test_f32(fun(f0, f1, f2, f3), ret, NULL);
      }
      break;
    }
    default:
      printf("unsupported types: ret f32, n_args - %zu\n", n_args);
      break;
  }
}

__attribute__((unused)) static void
test_ret_f64(void* fun_ptr, double ret, uint64_t* args, uint8_t* args_type,
             size_t n_args)
{
  size_t i;

  switch (n_args) {
    case 0: {
      double((*fun)()) = fun_ptr;
      test_f64(fun(), ret, NULL);
      break;
    }
    case 1: {
      switch (args_type[0]) {
        case 0: {
          double((*fun)(uint32_t)) = fun_ptr;
          test_f64(fun((uint32_t)args[0]), ret, NULL);
          break;
        }
        case 1: {
          double((*fun)(uint64_t)) = fun_ptr;
          test_f64(fun(args[0]), ret, NULL);
          break;
        }
        case 2: {
          double((*fun)(float)) = fun_ptr;
          float f;
          memcpy(&f, &args[0], sizeof(uint32_t));
          test_f64(fun(f), ret, NULL);
          break;
        }
        case 3: {
          double((*fun)(double)) = fun_ptr;
          double f;
          memcpy(&f, &args[0], sizeof(uint64_t));
          test_f64(fun(f), ret, NULL);
          break;
        }
        default:
          printf("arg 0: unknown");
          return;
      }
      break;
    }
    case 2: {
      if (args_type[0] == 0 && args_type[1] == 0) {
        double((*fun)(uint32_t, uint32_t)) = fun_ptr;
        test_f64(fun((uint32_t)args[0], (uint32_t)args[1]), ret, NULL);
      } else if (args_type[0] == 1 && args_type[1] == 1) {
        double((*fun)(uint64_t, uint64_t)) = fun_ptr;
        test_f64(fun(args[0], args[1]), ret, NULL);
      } else if (args_type[0] == 2 && args_type[1] == 2) {
        double((*fun)(float, float)) = fun_ptr;
        float f0, f1;
        memcpy(&f0, &args[0], sizeof(uint32_t));
        memcpy(&f1, &args[1], sizeof(uint32_t));
        test_f64(fun(f0, f1), ret, NULL);
      } else if (args_type[0] == 3 && args_type[1] == 3) {
        double((*fun)(double, double)) = fun_ptr;
        double f0, f1;
        memcpy(&f0, &args[0], sizeof(uint64_t));
        memcpy(&f1, &args[1], sizeof(uint64_t));
        test_f64(fun(f0, f1), ret, NULL);
      } else if (args_type[0] == 2 && args_type[1] == 3) {
        double((*fun)(float, double)) = fun_ptr;
        float f0;
        double f1;
        memcpy(&f0, &args[0], sizeof(uint32_t));
        memcpy(&f1, &args[1], sizeof(uint64_t));
        test_f64(fun(f0, f1), ret, NULL);
      } else if (args_type[0] == 3 && args_type[1] == 2) {
        double((*fun)(double, float)) = fun_ptr;
        double f0;
        float f1;
        memcpy(&f0, &args[0], sizeof(uint64_t));
        memcpy(&f1, &args[1], sizeof(uint32_t));
        test_f64(fun(f0, f1), ret, NULL);
      } else {
        printf("unsupported 2 args types (%u, %u)\n", args_type[0],
               args_type[1]);
      }
      break;
    }
    case 3: {
      if (args_type[0] == 3 && args_type[1] == 3 && args_type[2] == 3) {
        double((*fun)(double, double, double)) = fun_ptr;
        double f0, f1, f2;
        memcpy(&f0, &args[0], sizeof(uint64_t));
        memcpy(&f1, &args[1], sizeof(uint64_t));
        memcpy(&f2, &args[2], sizeof(uint64_t));
        test_f64(fun(f0, f1, f2), ret, NULL);
      } else if (args_type[0] == 3 && args_type[1] == 3 && args_type[2] == 0) {
        double((*fun)(double, double, uint32_t)) = fun_ptr;
        double f0, f1;
        memcpy(&f0, &args[0], sizeof(uint64_t));
        memcpy(&f1, &args[1], sizeof(uint64_t));
        test_f64(fun(f0, f1, (uint32_t)args[2]), ret, NULL);
      } else {
        printf("unsupported 3 args types ret f64 - (%u, %u, %u)\n",
               args_type[0], args_type[1], args_type[2]);
      }
      break;
    }
    case 4: {
      if (args_type[0] == 3 && args_type[1] == 3 && args_type[2] == 3 &&
          args_type[3] == 3) {
        double((*fun)(double, double, double, double)) = fun_ptr;
        double f0, f1, f2, f3;
        memcpy(&f0, &args[0], sizeof(uint64_t));
        memcpy(&f1, &args[1], sizeof(uint64_t));
        memcpy(&f2, &args[2], sizeof(uint64_t));
        memcpy(&f3, &args[3], sizeof(uint64_t));
        test_f64(fun(f0, f1, f2, f3), ret, NULL);
      }
      break;
    }
    default:
      printf("unsupported types: ret f64, n_args - %zu\n", n_args);
      break;
  }
}

__attribute__((unused)) static void
do_spec_test(struct Module* module, const char* fun_name, uint64_t* args,
             uint8_t* args_type, size_t n_args, uint64_t expected,
             uint8_t expected_type)
{
  size_t i;
  struct Function* func = NULL;

  for (i = 0; i < module->funcs.size; i++) {
    func = module->funcs.data[i];
    if (func->name == NULL) {
      continue;
    }
    if (strcmp(func->name, fun_name) == 0) {
      break;
    }
    func = NULL;
  }

  if (func == NULL) {
    printf("Target function %s not found!\n", fun_name);
    return;
  }

  // Build function type.
  switch (expected_type) {
    case 0:
      test_ret_i32(func->code, (uint32_t)expected, args, args_type, n_args);
      break;
    case 1:
      test_ret_i64(func->code, expected, args, args_type, n_args);
      break;
    case 2: {
      float f;
      memcpy(&f, &expected, sizeof(uint32_t));
      test_ret_f32(func->code, f, args, args_type, n_args);
      break;
    }
    case 3: {
      double f;
      memcpy(&f, &expected, sizeof(uint64_t));
      test_ret_f64(func->code, f, args, args_type, n_args);
      break;
    }
    case 4: {
      test_ret_void(func->code, args, args_type, n_args);
      break;
    }
    default:
      printf("expected: unknown");
      return;
  }
}

static void*
get_stack_top(void)
{
  return NULL;
}

// This is only for manual tests, should support automatic
// spec test in the future.
int
run_wasm_test(const char* filename, uint32_t static_bump, int has_table,
              size_t tablemin, size_t tablemax, const char* fun_name,
              uint64_t* args, uint8_t* args_type, size_t n_args,
              uint64_t expected, uint8_t expected_type)
{
  struct WasmJITHigh high;
  int ret = 1;
  void* stack_top;
  int high_init = 0;
  const char* msg;
  uint32_t flags = 0;
  struct EmscriptenContext ctx;
  struct PassManager pm;
  struct SystemConfig sys_config;

  emscripten_context_init(&ctx);

  pass_manager_init(&pm);

  passes_init(&pm);

  init_system_sensing(&sys_config);
  system_sensing(&sys_config);

  stack_top = get_stack_top();
  if (!stack_top) {
    // fprintf(stderr, "warning: running without a stack limit\n");
  }

  if (sgxwasm_high_init(&high)) {
    msg = "failed to initialize";
    goto error;
  }
  high_init = 1;

  if (!has_table)
    flags |= SGXWASM_HIGH_INSTANTIATE_EMSCRIPTEN_RUNTIME_FLAGS_NO_TABLE;

  if (sgxwasm_high_instantiate_emscripten_runtime(&high, &ctx, static_bump,
                                                  tablemin, tablemax, flags)) {
    msg = "failed to instantiate emscripten runtime";
    goto error;
  }

  if (sgxwasm_high_instantiate(&high, &pm, &sys_config, filename, "asm", 0)) {
    msg = "failed to instantiate module";
    goto error;
  }

  size_t i = 0;
  uint8_t found = 0;
  struct NamedModule* module_list = high.modules;
  for (; i < high.n_modules; i++) {
    if (strcmp(module_list[i].name, "asm") == 0) {
      found = 1;
      break;
    }
  }

  if (found == 0) {
    msg = "module asm not found";
    goto error;
  }

  struct Module* module = module_list[i].module;
  struct Function* func;

  memory_preloading();

#if 0
  // dump_spec_test(fun_name, args, args_type, n_args, expected, expected_type);

  do_spec_test(module, fun_name, args, args_type, n_args, expected,
               expected_type);
#endif

#if __SGX__
  if (strcmp(filename, "../../tests/micro_benchmark/sum/test.wasm") == 0) {
    {
      printf("_sum\n");
      uint64_t t1, t2;
      // func = module->funcs.data[21];
      func = module->funcs.data[22];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      // int((*fun)(uint32_t, uint32_t)) = func->code;
      int((*fun)()) = func->code;
      ocall_sgx_rdtsc(&t1);
      // test_i32(fun(5, 6), 11, &count);
      // fun(5, 6);
      fun();
      ocall_sgx_rdtsc(&t2);
      printf("t: %lu\n", t2 - t1);
    }
  } else if (strcmp(filename, "../../tests/micro_benchmark/sum/sum.wasm") ==
             0) {
    {
      printf("sum\n");
      uint64_t t1, t2;
      int i = 0;
      func = module->funcs.data[0];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      int((*fun)(uint32_t, uint32_t)) = func->code;

      ocall_sgx_rdtsc(&t1);
      // test_i32(fun(5, 6), 11, &count);
      for (i = 0; i < 1000000; i++) {
        fun(5, 6);
        // test_sum(5, 6);
      }
      ocall_sgx_rdtsc(&t2);
      printf("t: %lu\n", t2 - t1);
    }
  } else if (strcmp(filename, "../../tests/micro_benchmark/fib/fib.wasm") ==
             0) {
    {
      printf("fib\n");
      uint64_t t1, t2;
      int i = 0;
      func = module->funcs.data[1];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      // int((*fun)(uint32_t)) = func->code;
      int((*fun)()) = func->code;

      // printf("fib: %d\n", fun(10));
      ocall_sgx_rdtsc(&t1);
      // test_i32(fun(5, 6), 11, &count);
      for (i = 0; i < 1000000; i++) {
        // fun(10);
        fun();
      }
      ocall_sgx_rdtsc(&t2);
      printf("t: %lu\n", t2 - t1);
    }
  } else if (strcmp(filename, "../../tests/micro_benchmark/fib/test.wasm") ==
             0) {
    {
      printf("fib\n");
      uint64_t t1, t2;
      int i = 0;
      func = module->funcs.data[22];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      int((*fun)()) = func->code;

      ocall_sgx_rdtsc(&t1);
      for (i = 0; i < 1000000; i++) {
        fun();
      }
      ocall_sgx_rdtsc(&t2);
      printf("t: %lu\n", t2 - t1);
    }
  } else if (strcmp(filename, "../../tests/micro_benchmark/sha/test.wasm") ==
             0) {
    {
      //printf("sum\n");
      uint64_t t1, t2;
      func = module->funcs.data[32];
      //#if DUMP_CODE
      //dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      int((*fun)()) = func->code;

      ocall_sgx_rdtsc(&t1);
      // test_i32(fun(5, 6), 11, &count);
      fun();
      ocall_sgx_rdtsc(&t2);
      printf("%lu\n", t2 - t1);
    }
  } else if (strcmp(filename, "../../tests/micro_benchmark/nbody/test.wasm") ==
             0) {
    {
      //printf("nbody\n");
      uint64_t t1, t2;
      func = module->funcs.data[24];
      //#if DUMP_CODE
      //dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      int((*fun)()) = func->code;

      ocall_sgx_rdtsc(&t1);
      fun();
      ocall_sgx_rdtsc(&t2);
      printf("%lu\n", t2 - t1);
    }
  } else if (strcmp(filename,
                    "../../tests/micro_benchmark/fannkuchredux/test.wasm") ==
             0) {
    {
      //printf("fannkuchredux\n");
      uint64_t t1, t2;
      func = module->funcs.data[25];
      //#if DUMP_CODE
      //dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      int((*fun)(uint32_t)) = func->code;

      ocall_sgx_rdtsc(&t1);
      fun(7);
      ocall_sgx_rdtsc(&t2);
      printf("%lu\n", t2 - t1);
    }
  }
#endif

  if (0) {
    char error_buffer[256];

  error:
    ret = sgxwasm_high_error_message(&high, error_buffer, sizeof(error_buffer));
    if (!ret) {
      fprintf(stderr, "%s: %s\n", msg, error_buffer);
      ret = -1;
    }
  }
  if (high_init)
    sgxwasm_high_close(&high);

  return ret;
}
