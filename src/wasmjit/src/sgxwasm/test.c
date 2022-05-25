#include <sgxwasm/emscripten.h>
#include <sgxwasm/high_level.h>
#include <sgxwasm/instantiate.h>
#include <sgxwasm/parse.h>
#include <sgxwasm/pass.h>
#include <sgxwasm/runtime.h>
#include <sgxwasm/util.h>

#include <stdio.h>

#define DUMP_CODE 0

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
test_i32(int lv, int rv, int* count)
{
  printf("case %d ... ", ++(*count));
  if (lv == rv) {
    printf("pass: %d\n", lv);
  } else {
    printf("fail, expected: %d, returned: %d\n", rv, lv);
  }
}

static void
test_i64(long lv, long rv, int* count)
{
  printf("case %d ... ", ++(*count));
  if (lv == rv) {
    printf("pass\n");
  } else {
    printf("fail, expected: %ld, returned: %ld\n", rv, lv);
  }
}

static void
test_f32(float lv, float rv, int* count)
{
  printf("case %d ... ", ++(*count));
  if (lv == rv) {
    printf("pass\n");
  } else {
    printf("fail, expected: %f, returned: %f\n", rv, lv);
  }
}

static void
test_f64(double lv, double rv, int* count)
{
  printf("case %d ... ", ++(*count));
  if (lv == rv) {
    printf("pass\n");
  } else {
    printf("fail, expected: %lf, returned: %lf\n", rv, lv);
  }
}

static void*
get_stack_top(void)
{
  return NULL;
}

static void
test_br_table(struct Module* module)
{
  struct Function* func;
  {
    printf("fun 0\n");
    int count = 0;
    func = module->funcs.data[0];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    void((*fun)(void)) = func->code;

    (void)count;
    fun();
  }
  {
    printf("type-i32\n");
    int count = 0;
    func = module->funcs.data[1];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    void((*fun)(void)) = func->code;

    (void)count;
    fun();
  }
  {
    printf("type-i64\n");
    int count = 0;
    func = module->funcs.data[2];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    void((*fun)(void)) = func->code;

    (void)count;
    fun();
  }
  {
    printf("type-f32\n");
    int count = 0;
    func = module->funcs.data[3];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    void((*fun)(void)) = func->code;

    (void)count;
    fun();
  }
  {
    printf("type-f64\n");
    int count = 0;
    func = module->funcs.data[4];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    void((*fun)(void)) = func->code;

    (void)count;
    fun();
  }
  {
    printf("type-i32-value\n");
    int count = 0;
    func = module->funcs.data[5];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 1, &count);
  }
  {
    printf("type-i64-value\n");
    int count = 0;
    func = module->funcs.data[6];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    long((*fun)(void)) = func->code;

    test_i64(fun(), 2, &count);
  }
  {
    printf("type-f32-value\n");
    int count = 0;
    func = module->funcs.data[7];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    float((*fun)(void)) = func->code;

    test_f32(fun(), 3, &count);
  }
  {
    printf("type-f64-value\n");
    int count = 0;
    func = module->funcs.data[8];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    double((*fun)(void)) = func->code;

    test_f64(fun(), 4, &count);
  }
  {
    printf("empty\n");
    int count = 0;
    func = module->funcs.data[9];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int)) = func->code;

    test_i32(fun(0), 22, &count);
    test_i32(fun(1), 22, &count);
    test_i32(fun(11), 22, &count);
    test_i32(fun(-1), 22, &count);
    test_i32(fun(-100), 22, &count);
    test_i32(fun(0xffffffff), 22, &count);
  }
  {
    printf("empty-value\n");
    int count = 0;
    func = module->funcs.data[10];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int)) = func->code;

    test_i32(fun(0), 33, &count);
    test_i32(fun(1), 33, &count);
    test_i32(fun(11), 33, &count);
    test_i32(fun(-1), 33, &count);
    test_i32(fun(-100), 33, &count);
    test_i32(fun(0xffffffff), 33, &count);
  }
  {
    printf("singleton\n");
    int count = 0;
    func = module->funcs.data[11];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int)) = func->code;

    test_i32(fun(0), 22, &count);
    test_i32(fun(1), 20, &count);
    test_i32(fun(11), 20, &count);
    test_i32(fun(-1), 20, &count);
    test_i32(fun(-100), 20, &count);
    test_i32(fun(0xffffffff), 20, &count);
  }
  {
    printf("singleton-value\n");
    int count = 0;
    func = module->funcs.data[12];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int)) = func->code;

    test_i32(fun(0), 32, &count);
    test_i32(fun(1), 33, &count);
    test_i32(fun(11), 33, &count);
    test_i32(fun(-1), 33, &count);
    test_i32(fun(-100), 33, &count);
    test_i32(fun(0xffffffff), 33, &count);
  }
  {
    printf("multiple\n");
    int count = 0;
    func = module->funcs.data[13];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int)) = func->code;

    test_i32(fun(0), 103, &count);
    test_i32(fun(1), 102, &count);
    test_i32(fun(2), 101, &count);
    test_i32(fun(3), 100, &count);
    test_i32(fun(4), 104, &count);
    test_i32(fun(5), 104, &count);
    test_i32(fun(6), 104, &count);
    test_i32(fun(10), 104, &count);
    test_i32(fun(-1), 104, &count);
    test_i32(fun(0xffffffff), 104, &count);
  }
  {
    printf("multiple-value\n");
    int count = 0;
    func = module->funcs.data[14];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int)) = func->code;

    test_i32(fun(0), 213, &count);
    test_i32(fun(1), 212, &count);
    test_i32(fun(2), 211, &count);
    test_i32(fun(3), 210, &count);
    test_i32(fun(4), 214, &count);
    test_i32(fun(5), 214, &count);
    test_i32(fun(6), 214, &count);
    test_i32(fun(10), 214, &count);
    test_i32(fun(-1), 214, &count);
    test_i32(fun(0xffffffff), 214, &count);
  }
  {
    printf("large\n");
    int count = 0;
    func = module->funcs.data[15];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int)) = func->code;

    test_i32(fun(0), 0, &count);
    test_i32(fun(1), 1, &count);
    test_i32(fun(100), 0, &count);
    test_i32(fun(101), 1, &count);
    test_i32(fun(10000), 0, &count);
    test_i32(fun(10001), 1, &count);
    test_i32(fun(1000000), 1, &count);
    test_i32(fun(1000001), 1, &count);
  }
  {
    printf("as-block-first\n");
    int count = 0;
    func = module->funcs.data[16];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    void((*fun)(void)) = func->code;

    (void)count;
    fun();
  }
  {
    printf("as-block-mid\n");
    int count = 0;
    func = module->funcs.data[17];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    void((*fun)(void)) = func->code;

    (void)count;
    fun();
  }
  {
    printf("as-block-last\n");
    int count = 0;
    func = module->funcs.data[18];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    void((*fun)(void)) = func->code;

    (void)count;
    fun();
  }
  {
    printf("as-block-value\n");
    int count = 0;
    func = module->funcs.data[19];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 2, &count);
  }
  {
    printf("as-loop-first\n");
    int count = 0;
    func = module->funcs.data[20];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 3, &count);
  }
  {
    printf("as-loop-mid\n");
    int count = 0;
    func = module->funcs.data[21];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 4, &count);
  }
  {
    printf("as-loop-last\n");
    int count = 0;
    func = module->funcs.data[22];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 5, &count);
  }
  {
    printf("as-br-value\n");
    int count = 0;
    func = module->funcs.data[23];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 9, &count);
  }
  {
    printf("as-br_if-cond\n");
    int count = 0;
    func = module->funcs.data[24];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    void((*fun)(void)) = func->code;

    (void)count;
    fun();
  }
  {
    printf("as-br_if-value\n");
    int count = 0;
    func = module->funcs.data[25];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 8, &count);
  }
  {
    printf("as-br_if-value-cond\n");
    int count = 0;
    func = module->funcs.data[26];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 9, &count);
  }
  {
    printf("as-br_table-index\n");
    int count = 0;
    func = module->funcs.data[27];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    void((*fun)(void)) = func->code;

    (void)count;
    fun();
  }
  {
    printf("as-br_table-value\n");
    int count = 0;
    func = module->funcs.data[28];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 10, &count);
  }
  {
    printf("as-br_table-value-index\n");
    int count = 0;
    func = module->funcs.data[29];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 11, &count);
  }
  {
    printf("as-return-value\n");
    int count = 0;
    func = module->funcs.data[30];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    long((*fun)(void)) = func->code;

    test_i64(fun(), 7, &count);
  }
  {
    printf("as-if-cond\n");
    int count = 0;
    func = module->funcs.data[31];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 2, &count);
  }
  {
    printf("as-if-then\n");
    int count = 0;
    func = module->funcs.data[32];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int, int)) = func->code;

    test_i32(fun(1, 6), 3, &count);
    test_i32(fun(0, 6), 6, &count);
  }
  {
    printf("as-if-else\n");
    int count = 0;
    func = module->funcs.data[33];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int, int)) = func->code;

    test_i32(fun(0, 6), 4, &count);
    test_i32(fun(1, 6), 6, &count);
  }
  {
    printf("as-select-first\n");
    int count = 0;
    func = module->funcs.data[34];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int, int)) = func->code;

    test_i32(fun(0, 6), 5, &count);
    test_i32(fun(1, 6), 5, &count);
  }
  {
    printf("as-select-second\n");
    int count = 0;
    func = module->funcs.data[35];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int, int)) = func->code;

    test_i32(fun(0, 6), 6, &count);
    test_i32(fun(1, 6), 6, &count);
  }
  {
    printf("as-select-cond\n");
    int count = 0;
    func = module->funcs.data[36];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 7, &count);
  }
  // Function 37 is used by the following functions.
  {
    printf("as-call-first\n");
    int count = 0;
    func = module->funcs.data[38];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 12, &count);
  }
  {
    printf("as-call-mid\n");
    int count = 0;
    func = module->funcs.data[39];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 13, &count);
  }
  {
    printf("as-call-last\n");
    int count = 0;
    func = module->funcs.data[40];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 14, &count);
  }
  // Function 41 - 44: indirect call
  // Function 45 - 51: load & store
  {
    printf("as-unary-operand\n");
    int count = 0;
    func = module->funcs.data[52];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    float((*fun)(void)) = func->code;

    test_f32(fun(), 3.4, &count);
  }
  {
    printf("as-binary-left\n");
    int count = 0;
    func = module->funcs.data[53];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 3, &count);
  }
  {
    printf("as-binary-right\n");
    int count = 0;
    func = module->funcs.data[54];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    long((*fun)(void)) = func->code;

    test_i64(fun(), 45, &count);
  }
  {
    printf("as-test-operand\n");
    int count = 0;
    func = module->funcs.data[55];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 44, &count);
  }
  {
    printf("as-compare-left\n");
    int count = 0;
    func = module->funcs.data[56];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 43, &count);
  }
  {
    printf("as-compare-right\n");
    int count = 0;
    func = module->funcs.data[57];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 42, &count);
  }
  {
    printf("as-convert-operand\n");
    int count = 0;
    func = module->funcs.data[58];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(void)) = func->code;

    test_i32(fun(), 41, &count);
  }
  // Function 59: memory_grow
  {
    printf("nested-block-value\n");
    int count = 0;
    func = module->funcs.data[60];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int)) = func->code;

    test_i32(fun(0), 19, &count);
    test_i32(fun(1), 17, &count);
    test_i32(fun(2), 16, &count);
    test_i32(fun(10), 16, &count);
    test_i32(fun(-1), 16, &count);
    test_i32(fun(100000), 16, &count);
  }
  {
    printf("nested-br-value\n");
    int count = 0;
    func = module->funcs.data[61];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int)) = func->code;

    test_i32(fun(0), 8, &count);
    test_i32(fun(1), 9, &count);
    test_i32(fun(2), 17, &count);
    test_i32(fun(11), 17, &count);
    test_i32(fun(-4), 17, &count);
    test_i32(fun(10213210), 17, &count);
  }
  {
    printf("nested-br_if-value\n");
    int count = 0;
    func = module->funcs.data[62];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int)) = func->code;

    test_i32(fun(0), 17, &count);
    test_i32(fun(1), 9, &count);
    test_i32(fun(2), 8, &count);
    test_i32(fun(9), 8, &count);
    test_i32(fun(-9), 8, &count);
    test_i32(fun(999999), 8, &count);
  }
  {
    printf("nested-br_if-value-cond\n");
    int count = 0;
    func = module->funcs.data[63];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int)) = func->code;

    test_i32(fun(0), 9, &count);
    test_i32(fun(1), 8, &count);
    test_i32(fun(2), 9, &count);
    test_i32(fun(3), 9, &count);
    test_i32(fun(-1000000), 9, &count);
    test_i32(fun(9423975), 9, &count);
  }
  {
    printf("nested-br_table-value\n");
    int count = 0;
    func = module->funcs.data[64];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int)) = func->code;

    test_i32(fun(0), 17, &count);
    test_i32(fun(1), 9, &count);
    test_i32(fun(2), 8, &count);
    test_i32(fun(9), 8, &count);
    test_i32(fun(-9), 8, &count);
    test_i32(fun(999999), 8, &count);
  }
  {
    printf("nested-br_table-value-index\n");
    int count = 0;
    func = module->funcs.data[65];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int)) = func->code;

    test_i32(fun(0), 9, &count);
    test_i32(fun(1), 8, &count);
    test_i32(fun(2), 9, &count);
    test_i32(fun(3), 9, &count);
    test_i32(fun(-1000000), 9, &count);
    test_i32(fun(9423975), 9, &count);
  }
  {
    printf("nested-br_table-loop-block\n");
    int count = 0;
    func = module->funcs.data[66];
#if DUMP_CODE
    dump_compile_code((uint8_t*)func->code, func->size);
#endif
    int((*fun)(int)) = func->code;

    test_i32(fun(1), 3, &count);
  }
}

static void
test_conversions(struct Module* module)
{
  struct Function* func;
  {
    printf("i64.extend_s_i32\n");
    int count = 0;
    func = module->funcs.data[0];
    long((*fun)(int)) = func->code;
    test_i64(fun(0), 0, &count);
    test_i64(fun(10000), 10000, &count);
    test_i64(fun(-10000), -10000, &count);
    test_i64(fun(-1), -1, &count);
    test_i64(fun(0x7fffffff), 0x000000007fffffff, &count);
    test_i64(fun(0x80000000), 0xffffffff80000000, &count);
  }
  {
    printf("i64.extend_u_i32\n");
    int count = 0;
    func = module->funcs.data[1];
    long((*fun)(int)) = func->code;
    test_i64(fun(0), 0, &count);
    test_i64(fun(10000), 10000, &count);
    test_i64(fun(-10000), 0x00000000ffffd8f0, &count);
    test_i64(fun(-1), 0xffffffff, &count);
    test_i64(fun(0x7fffffff), 0x000000007fffffff, &count);
    test_i64(fun(0x80000000), 0x0000000080000000, &count);
  }
  {
    printf("i32.wrap_i64\n");
    int count = 0;
    func = module->funcs.data[2];
    int((*fun)(long)) = func->code;
    test_i32(fun(-1), -1, &count);
    test_i32(fun(-100000), -100000, &count);
    test_i32(fun(0x80000000), 0x80000000, &count);
    test_i32(fun(0xffffffff7fffffff), 0x7fffffff, &count);
    test_i32(fun(0xffffffff00000000), 0x00000000, &count);
    test_i32(fun(0xfffffffeffffffff), 0xffffffff, &count);
    test_i32(fun(0xffffffff00000001), 0x00000001, &count);
    test_i32(fun(0), 0, &count);
    test_i32(fun(1311768467463790320), 0x9abcdef0, &count);
    test_i32(fun(0x00000000ffffffff), 0xffffffff, &count);
    test_i32(fun(0x0000000100000000), 0x00000000, &count);
    test_i32(fun(0x0000000100000001), 0x00000001, &count);
  }
  {
    printf("i32.trunc_s_f32\n");
    int count = 0;
    func = module->funcs.data[3];
    int((*fun)(float)) = func->code;
    test_i32(fun(0.0), 0, &count);
    test_i32(fun(-0.0), 0, &count);
    test_i32(fun(0x1p-149), 0, &count);
    test_i32(fun(-0x1p-149), 0, &count);
    test_i32(fun(1.0), 1, &count);
    test_i32(fun(0x1.19999ap+0), 1, &count);
    test_i32(fun(1.5), 1, &count);
    test_i32(fun(-1.0), -1, &count);
    test_i32(fun(-0x1.19999ap+0), -1, &count);
    test_i32(fun(-1.5), -1, &count);
    test_i32(fun(-1.9), -1, &count);
    test_i32(fun(-2.0), -2, &count);
    test_i32(fun(2147483520.0), 2147483520, &count);
    test_i32(fun(-2147483648.0), -2147483648, &count);
  }
  {
    printf("i32.trunc_u_f32\n");
    int count = 0;
    func = module->funcs.data[4];
    int((*fun)(float)) = func->code;
    test_i32(fun(0.0), 0, &count);
    test_i32(fun(-0.0), 0, &count);
    test_i32(fun(0x1p-149), 0, &count);
    test_i32(fun(-0x1p-149), 0, &count);
    test_i32(fun(1.0), 1, &count);
    test_i32(fun(0x1.19999ap+0), 1, &count);
    test_i32(fun(1.5), 1, &count);
    test_i32(fun(1.9), 1, &count);
    test_i32(fun(2.0), 2, &count);
    test_i32(fun(2147483648), -2147483648, &count);
    test_i32(fun(4294967040.0), -256, &count);
    test_i32(fun(-0x1.ccccccp-1), 0, &count);
    test_i32(fun(-0x1.fffffep-1), 0, &count);
  }
  {
    printf("i32.trunc_s_f64\n");
    int count = 0;
    func = module->funcs.data[5];
    int((*fun)(double)) = func->code;
    test_i32(fun(0.0), 0, &count);
    test_i32(fun(-0.0), 0, &count);
    test_i32(fun(0x0.0000000000001p-1022), 0, &count);
    test_i32(fun(-0x0.0000000000001p-1022), 0, &count);
    test_i32(fun(1.0), 1, &count);
    test_i32(fun(0x1.199999999999ap+0), 1, &count);
    test_i32(fun(1.5), 1, &count);
    test_i32(fun(-1.0), -1, &count);
    test_i32(fun(-0x1.199999999999ap+0), -1, &count);
    test_i32(fun(-1.5), -1, &count);
    test_i32(fun(-1.9), -1, &count);
    test_i32(fun(-2.0), -2, &count);
    test_i32(fun(2147483647.0), 2147483647, &count);
    test_i32(fun(-2147483648.0), -2147483648, &count);
  }
  {
    printf("i32.trunc_u_f64\n");
    int count = 0;
    func = module->funcs.data[6];
    int((*fun)(double)) = func->code;
    test_i32(fun(0.0), 0, &count);
    test_i32(fun(-0.0), 0, &count);
    test_i32(fun(0x0.0000000000001p-1022), 0, &count);
    test_i32(fun(-0x0.0000000000001p-1022), 0, &count);
    test_i32(fun(1.0), 1, &count);
    test_i32(fun(0x1.199999999999ap+0), 1, &count);
    test_i32(fun(1.5), 1, &count);
    test_i32(fun(1.9), 1, &count);
    test_i32(fun(2.0), 2, &count);
    test_i32(fun(2147483648), -2147483648, &count);
    test_i32(fun(4294967295.0), -1, &count);
    test_i32(fun(-0x1.ccccccccccccdp-1), 0, &count);
    test_i32(fun(-0x1.fffffffffffffp-1), 0, &count);
    test_i32(fun(1e8), 100000000, &count);
  }
  {
    printf("i64.trunc_s_f32\n");
    int count = 0;
    func = module->funcs.data[7];
    long((*fun)(float)) = func->code;
    test_i64(fun(0.0), 0, &count);
    test_i64(fun(-0.0), 0, &count);
    test_i64(fun(0x1p-149), 0, &count);
    test_i64(fun(-0x1p-149), 0, &count);
    test_i64(fun(1.0), 1, &count);
    test_i64(fun(0x1.19999ap+0), 1, &count);
    test_i64(fun(1.5), 1, &count);
    test_i64(fun(-1.0), -1, &count);
    test_i64(fun(-0x1.19999ap+0), -1, &count);
    test_i64(fun(-1.5), -1, &count);
    test_i64(fun(-1.9), -1, &count);
    test_i64(fun(-2.0), -2, &count);
    test_i64(fun(4294967296), 4294967296, &count);
    test_i64(fun(-4294967296), -4294967296, &count);
    test_i64(fun(9223371487098961920.0), 9223371487098961920, &count);
    // test_i64(fun(-9223372036854775808.0), -9223372036854775808, &count);
  }
  {
    printf("i64.trunc_u_f32\n");
    int count = 0;
    func = module->funcs.data[8];
    long((*fun)(float)) = func->code;
    test_i64(fun(0.0), 0, &count);
    test_i64(fun(-0.0), 0, &count);
    test_i64(fun(0x1p-149), 0, &count);
    test_i64(fun(-0x1p-149), 0, &count);
    test_i64(fun(1.0), 1, &count);
    test_i64(fun(0x1.19999ap+0), 1, &count);
    test_i64(fun(1.5), 1, &count);
    test_i64(fun(4294967296), 4294967296, &count);
    test_i64(fun(18446742974197923840.0), -1099511627776, &count);
    test_i64(fun(-0x1.ccccccp-1), 0, &count);
    test_i64(fun(-0x1.fffffep-1), 0, &count);
  }
  {
    printf("i64.trunc_s_f64\n");
    int count = 0;
    func = module->funcs.data[9];
    long((*fun)(double)) = func->code;
    test_i64(fun(0.0), 0, &count);
    test_i64(fun(-0.0), 0, &count);
    test_i64(fun(0x0.0000000000001p-1022), 0, &count);
    test_i64(fun(-0x0.0000000000001p-1022), 0, &count);
    test_i64(fun(1.0), 1, &count);
    test_i64(fun(0x1.199999999999ap+0), 1, &count);
    test_i64(fun(1.5), 1, &count);
    test_i64(fun(-1.0), -1, &count);
    test_i64(fun(-0x1.199999999999ap+0), -1, &count);
    test_i64(fun(-1.5), -1, &count);
    test_i64(fun(-1.9), -1, &count);
    test_i64(fun(-2.0), -2, &count);
    test_i64(fun(4294967296), 4294967296, &count);
    test_i64(fun(-4294967296), -4294967296, &count);
    test_i64(fun(9223372036854774784.0), 9223372036854774784, &count);
    // test_i64(fun(-9223372036854775808.0), -9223372036854775808, &count);
  }
  {
    printf("i64.trunc_u_f64\n");
    int count = 0;
    func = module->funcs.data[10];
    long((*fun)(double)) = func->code;
    test_i64(fun(0.0), 0, &count);
    test_i64(fun(-0.0), 0, &count);
    test_i64(fun(0x0.0000000000001p-1022), 0, &count);
    test_i64(fun(-0x0.0000000000001p-1022), 0, &count);
    test_i64(fun(1.0), 1, &count);
    test_i64(fun(0x1.199999999999ap+0), 1, &count);
    test_i64(fun(1.5), 1, &count);
    test_i64(fun(4294967295), 0xffffffff, &count);
    test_i64(fun(4294967296), 0x100000000, &count);
    test_i64(fun(18446744073709549568.0), -2048, &count);
    test_i64(fun(-0x1.ccccccccccccdp-1), 0, &count);
    test_i64(fun(-0x1.fffffffffffffp-1), 0, &count);
    test_i64(fun(1e8), 100000000, &count);
    test_i64(fun(1e16), 10000000000000000, &count);
    // test_i64(fun(9223372036854775808), -9223372036854775808, &count);
  }
  {
    printf("f32.convert_s_i32\n");
    int count = 0;
    func = module->funcs.data[11];
    float((*fun)(int)) = func->code;
    test_f32(fun(1), 1.0, &count);
    test_f32(fun(-1), -1.0, &count);
    test_f32(fun(0), 0.0, &count);
    test_f32(fun(2147483647), 2147483648, &count);
    test_f32(fun(-2147483648), -2147483648, &count);
    test_f32(fun(1234567890), 0x1.26580cp+30, &count);
    test_f32(fun(16777217), 16777216.0, &count);
    test_f32(fun(-16777217), -16777216.0, &count);
    test_f32(fun(16777219), 16777220.0, &count);
    test_f32(fun(-16777219), -16777220.0, &count);
  }
  {
    printf("f32.convert_s_i64\n");
    int count = 0;
    func = module->funcs.data[12];
    float((*fun)(long)) = func->code;
    test_f32(fun(1), 1.0, &count);
    test_f32(fun(-1), -1.0, &count);
    test_f32(fun(0), 0.0, &count);
    test_f32(fun(9223372036854775807), 9223372036854775807, &count);
    // test_f32(fun(-9223372036854775808), -9223372036854775808, &count);
    test_f32(fun(314159265358979), 0x1.1db9e8p+48, &count);
    test_f32(fun(16777217), 16777216.0, &count);
    test_f32(fun(-16777217), -16777216.0, &count);
    test_f32(fun(16777219), 16777220.0, &count);
    test_f32(fun(-16777219), -16777220.0, &count);
  }
  {
    printf("f64.convert_s_i32\n");
    int count = 0;
    func = module->funcs.data[13];
    double((*fun)(int)) = func->code;
    test_f64(fun(1), 1.0, &count);
    test_f64(fun(-1), -1.0, &count);
    test_f64(fun(0), 0.0, &count);
    test_f64(fun(2147483647), 2147483647, &count);
    test_f64(fun(-2147483648), -2147483648, &count);
    test_f64(fun(987654321), 987654321, &count);
  }
  {
    printf("f64.convert_s_i64\n");
    int count = 0;
    func = module->funcs.data[14];
    double((*fun)(long)) = func->code;
    test_f64(fun(1), 1.0, &count);
    test_f64(fun(-1), -1.0, &count);
    test_f64(fun(0), 0.0, &count);
    test_f64(fun(9223372036854775807), 9223372036854775807, &count);
    // test_f64(fun(-9223372036854775808), -9223372036854775808, &count);
    test_f64(fun(4669201609102990), 4669201609102990, &count);
    test_f64(fun(9007199254740993), 9007199254740992, &count);
    test_f64(fun(-9007199254740993), -9007199254740992, &count);
    test_f64(fun(9007199254740995), 9007199254740996, &count);
    test_f64(fun(-9007199254740995), -9007199254740996, &count);
  }
  {
    printf("f32.convert_u_i32\n");
    int count = 0;
    func = module->funcs.data[15];
    float((*fun)(int)) = func->code;
    test_f32(fun(1), 1.0, &count);
    test_f32(fun(0), 0.0, &count);
    test_f32(fun(2147483647), 2147483648, &count);
    test_f32(fun(-2147483648), 2147483648, &count);
    test_f32(fun(0x12345678), 0x1.234568p+28, &count);
    test_f32(fun(0xffffffff), 4294967296.0, &count);
    test_f32(fun(0x80000080), 0x1.000000p+31, &count);
    test_f32(fun(0x80000081), 0x1.000002p+31, &count);
    test_f32(fun(0x80000082), 0x1.000002p+31, &count);
    test_f32(fun(0xfffffe80), 0x1.fffffcp+31, &count);
    test_f32(fun(0xfffffe81), 0x1.fffffep+31, &count);
    test_f32(fun(0xfffffe82), 0x1.fffffep+31, &count);
    test_f32(fun(16777217), 16777216.0, &count);
    test_f32(fun(16777219), 16777220.0, &count);
  }
  {
    printf("f32.convert_u_i64\n");
    int count = 0;
    func = module->funcs.data[16];
    float((*fun)(long)) = func->code;
    test_f32(fun(1), 1.0, &count);
    test_f32(fun(0), 0.0, &count);
    test_f32(fun(9223372036854775807), 9223372036854775807, &count);
    // test_f32(fun(-9223372036854775808), 9223372036854775808, &count);
    test_f32(fun(0xffffffffffffffff), 18446744073709551616.0, &count);
    test_f32(fun(16777217), 16777216.0, &count);
    test_f32(fun(16777219), 16777220.0, &count);
  }
  {
    printf("f64.convert_u_i32\n");
    int count = 0;
    func = module->funcs.data[17];
    double((*fun)(int)) = func->code;
    test_f64(fun(1), 1.0, &count);
    test_f64(fun(0), 0.0, &count);
    test_f64(fun(2147483647), 2147483647, &count);
    test_f64(fun(-2147483648), 2147483648, &count);
    test_f64(fun(0xffffffff), 4294967295.0, &count);
  }
  {
    printf("f64.convert_u_i64\n");
    int count = 0;
    func = module->funcs.data[18];
    double((*fun)(long)) = func->code;
    test_f64(fun(1), 1.0, &count);
    test_f64(fun(0), 0.0, &count);
    test_f64(fun(9223372036854775807), 9223372036854775807, &count);
    // test_f64(fun(-9223372036854775808), 9223372036854775808, &count);
    test_f64(fun(0xffffffffffffffff), 18446744073709551616.0, &count);
    test_f64(fun(0x8000000000000400), 0x1.0000000000000p+63, &count);
    test_f64(fun(0x8000000000000401), 0x1.0000000000001p+63, &count);
    test_f64(fun(0x8000000000000402), 0x1.0000000000001p+63, &count);
    test_f64(fun(0xfffffffffffff400), 0x1.ffffffffffffep+63, &count);
    test_f64(fun(0xfffffffffffff401), 0x1.fffffffffffffp+63, &count);
    test_f64(fun(0xfffffffffffff402), 0x1.fffffffffffffp+63, &count);
    test_f64(fun(9007199254740993), 9007199254740992, &count);
    test_f64(fun(9007199254740995), 9007199254740996, &count);
  }
  {
    printf("f64.promote_f32\n");
    int count = 0;
    func = module->funcs.data[19];
    double((*fun)(float)) = func->code;
    test_f64(fun(0.0), 0.0, &count);
    test_f64(fun(-0.0), -0.0, &count);
    test_f64(fun(0x1p-149), 0x1p-149, &count);
    test_f64(fun(-0x1p-149), -0x1p-149, &count);
    test_f64(fun(1.0), 1.0, &count);
    test_f64(fun(-1.0), -1.0, &count);
    test_f64(fun(-0x1.fffffep+127), -0x1.fffffep+127, &count);
    test_f64(fun(0x1.fffffep+127), 0x1.fffffep+127, &count);
    test_f64(fun(0x1p-119), 0x1p-119, &count);
    test_f64(fun(0x1.8f867ep+125), 6.6382536710104395e+37, &count);
    // test_f64(fun(inf), inf, &count);
    // test_f64(fun(-inf), -inf, &count);
  }
  {
    printf("f32.demote_f64\n");
    int count = 0;
    func = module->funcs.data[20];
    float((*fun)(double)) = func->code;
    test_f32(fun(0.0), 0.0, &count);
    test_f32(fun(-0.0), -0.0, &count);
    test_f32(fun(0x0.0000000000001p-1022), 0.0, &count);
    test_f32(fun(-0x0.0000000000001p-1022), -0.0, &count);
    test_f32(fun(1.0), 1.0, &count);
    test_f32(fun(-1.0), -1.0, &count);
    test_f32(fun(0x1.fffffe0000000p-127), 0x1p-126, &count);
    test_f32(fun(-0x1.fffffe0000000p-127), -0x1p-126, &count);
    test_f32(fun(0x1.fffffdfffffffp-127), 0x1.fffffcp-127, &count);
    test_f32(fun(-0x1.fffffdfffffffp-127), -0x1.fffffcp-127, &count);
    test_f32(fun(0x1p-149), 0x1p-149, &count);
    test_f32(fun(-0x1p-149), -0x1p-149, &count);
    test_f32(fun(0x1.fffffd0000000p+127), 0x1.fffffcp+127, &count);
    test_f32(fun(-0x1.fffffd0000000p+127), -0x1.fffffcp+127, &count);
    test_f32(fun(0x1.fffffd0000001p+127), 0x1.fffffep+127, &count);
    test_f32(fun(-0x1.fffffd0000001p+127), -0x1.fffffep+127, &count);
    test_f32(fun(0x1.fffffep+127), 0x1.fffffep+127, &count);
    test_f32(fun(-0x1.fffffep+127), -0x1.fffffep+127, &count);
    test_f32(fun(0x1.fffffefffffffp+127), 0x1.fffffep+127, &count);
    test_f32(fun(-0x1.fffffefffffffp+127), -0x1.fffffep+127, &count);
    // test_f32(fun(0x1.ffffffp+127), inf, &count);
    // test_f32(fun(-0x1.ffffffp+127), -inf, &count);
    test_f32(fun(0x1p-119), 0x1p-119, &count);
    test_f32(fun(0x1.8f867ep+125), 0x1.8f867ep+125, &count);
    // test_f32(fun(inf), inf, &count);
    // test_f32(fun(-inf), -inf, &count);
    test_f32(fun(0x1.0000000000001p+0), 1.0, &count);
    test_f32(fun(0x1.fffffffffffffp-1), 1.0, &count);
    test_f32(fun(0x1.0000010000000p+0), 0x1.000000p+0, &count);
    test_f32(fun(0x1.0000010000001p+0), 0x1.000002p+0, &count);
    test_f32(fun(0x1.000002fffffffp+0), 0x1.000002p+0, &count);
    test_f32(fun(0x1.0000030000000p+0), 0x1.000004p+0, &count);
    test_f32(fun(0x1.0000050000000p+0), 0x1.000004p+0, &count);
    test_f32(fun(0x1.0000010000000p+24), 0x1.0p+24, &count);
    test_f32(fun(0x1.0000010000001p+24), 0x1.000002p+24, &count);
    test_f32(fun(0x1.000002fffffffp+24), 0x1.000002p+24, &count);
    test_f32(fun(0x1.0000030000000p+24), 0x1.000004p+24, &count);
    test_f32(fun(0x1.4eae4f7024c7p+108), 0x1.4eae5p+108, &count);
    test_f32(fun(0x1.a12e71e358685p-113), 0x1.a12e72p-113, &count);
    test_f32(fun(0x1.cb98354d521ffp-127), 0x1.cb9834p-127, &count);
    test_f32(fun(-0x1.6972b30cfb562p+1), -0x1.6972b4p+1, &count);
    test_f32(fun(-0x1.bedbe4819d4c4p+112), -0x1.bedbe4p+112, &count);
    test_f32(fun(0x1p-1022), 0.0, &count);
    test_f32(fun(-0x1p-1022), -0.0, &count);
    test_f32(fun(0x1.0p-150), 0.0, &count);
    test_f32(fun(-0x1.0p-150), -0.0, &count);
    test_f32(fun(0x1.0000000000001p-150), 0x1p-149, &count);
    test_f32(fun(-0x1.0000000000001p-150), -0x1p-149, &count);
  }
  {
    printf("f32.reinterpret_i32\n");
    int count = 0;
    func = module->funcs.data[21];
    float((*fun)(int)) = func->code;
    test_f32(fun(0), 0.0, &count);
    test_f32(fun(0x80000000), -0.0, &count);
    test_f32(fun(1), 0x1p-149, &count);
    // test_f32(fun(-1), -nan:0x7fffff, &count);
    test_f32(fun(123456789), 0x1.b79a2ap-113, &count);
    test_f32(fun(-2147483647), -0x1p-149, &count);
    // test_f32(fun(0x7f800000), inf, &count);
    // test_f32(fun(0xff800000), -inf, &count);
    // test_f32(fun(0x7fc00000), nan, &count);
    // test_f32(fun(0xffc00000), -nan, &count);
    // test_f32(fun(0x7fa00000), nan:0x200000, &count);
    // test_f32(fun(0xffa00000), -nan:0x200000, &count);
  }
  {
    printf("f64.reinterpret_i64\n");
    int count = 0;
    func = module->funcs.data[22];
    double((*fun)(long)) = func->code;
    test_f64(fun(0), 0.0, &count);
    test_f64(fun(1), 0x0.0000000000001p-1022, &count);
    // test_f64(fun(-1), -nan:0xfffffffffffff, &count);
    test_f64(fun(0x8000000000000000), -0.0, &count);
    test_f64(fun(1234567890), 0x0.00000499602d2p-1022, &count);
    test_f64(fun(-9223372036854775807), -0x0.0000000000001p-1022, &count);
    // test_f64(fun(0x7ff0000000000000), inf, &count);
    // test_f64(fun(0xfff0000000000000), -inf, &count);
    // test_f64(fun(0x7ff8000000000000), nan, &count);
    // test_f64(fun(0xfff8000000000000), -nan, &count);
    // test_f64(fun(0x7ff4000000000000), nan:0x4000000000000, &count);
    // test_f64(fun(0xfff4000000000000), -nan:0x4000000000000, &count);
  }
  {
    printf("i32.reinterpret_f32\n");
    int count = 0;
    func = module->funcs.data[23];
    int((*fun)(float)) = func->code;
    test_i32(fun(0.0), 0, &count);
    test_i32(fun(-0.0), 0x80000000, &count);
    test_i32(fun(0x1p-149), 1, &count);
    // test_i32(fun(-nan:0x7fffff), -1, &count);
    test_i32(fun(-0x1p-149), 0x80000001, &count);
    test_i32(fun(1.0), 1065353216, &count);
    test_i32(fun(3.1415926), 1078530010, &count);
    test_i32(fun(0x1.fffffep+127), 2139095039, &count);
    test_i32(fun(-0x1.fffffep+127), -8388609, &count);
    // test_i32(fun(inf), 0x7f800000, &count);
    // test_i32(fun(-inf), 0xff800000, &count);
    // test_i32(fun(nan), 0x7fc00000, &count);
    // test_i32(fun(-nan), 0xffc00000, &count);
    // test_i32(fun(nan:0x200000), 0x7fa00000, &count);
    // test_i32(fun(-nan:0x200000), 0xffa00000, &count);
  }
  {
    printf("i64.reinterpret_f64\n");
    int count = 0;
    func = module->funcs.data[24];
    long((*fun)(double)) = func->code;
    test_i64(fun(0.0), 0, &count);
    test_i64(fun(-0.0), 0x8000000000000000, &count);
    test_i64(fun(0x0.0000000000001p-1022), 1, &count);
    // test_i64(fun(-nan:0xfffffffffffff), -1, &count);
    test_i64(fun(-0x0.0000000000001p-1022), 0x8000000000000001, &count);
    test_i64(fun(1.0), 4607182418800017408, &count);
    test_i64(fun(3.14159265358979), 4614256656552045841, &count);
    test_i64(fun(0x1.fffffffffffffp+1023), 9218868437227405311, &count);
    test_i64(fun(-0x1.fffffffffffffp+1023), -4503599627370497, &count);
    // test_i64(fun(inf), 0x7ff0000000000000, &count);
    // test_i64(fun(-inf), 0xfff0000000000000, &count);
    // test_i64(fun(nan), 0x7ff8000000000000, &count);
    // test_i64(fun(-nan), 0xfff8000000000000, &count);
    // test_i64(fun(nan:0x4000000000000), 0x7ff4000000000000, &count);
    // test_i64(fun(-nan:0x4000000000000), 0xfff4000000000000, &count);
  }
}

// This is only for manual tests, should support automatic
// spec test in the future.
int
run_wasm_test(const char* filename,
              uint32_t static_bump,
              int has_table,
              size_t tablemin,
              size_t tablemax)
{
  struct WasmJITHigh high;
  int ret = 1;
  void* stack_top;
  int high_init = 0;
  const char* msg;
  uint32_t flags = 0;
  struct EmscriptenContext ctx;
  struct PassManager pm;

  emscripten_context_init(&ctx);

  pass_manager_init(&pm);

  stack_top = get_stack_top();
  if (!stack_top) {
    fprintf(stderr, "warning: running without a stack limit\n");
  }

  if (sgxwasm_high_init(&high)) {
    msg = "failed to initialize";
    goto error;
  }
  high_init = 1;

  if (!has_table)
    flags |= SGXWASM_HIGH_INSTANTIATE_EMSCRIPTEN_RUNTIME_FLAGS_NO_TABLE;

  if (sgxwasm_high_instantiate_emscripten_runtime(
        &high, &ctx, static_bump, tablemin, tablemax, flags)) {
    msg = "failed to instantiate emscripten runtime";
    goto error;
  }

  if (sgxwasm_high_instantiate(&high, &pm, filename, "asm", 0)) {
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

  if (strcmp(filename, "test/spec/f32_cmp.0.wasm") == 0) {
    {
      // f32_eq
      printf("f32_eq\n");
      int count = 0;
      func = module->funcs.data[0];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      int((*fun)(float, float)) = func->code;

      test_i32(fun(0, 0), 1, &count);
      test_i32(fun(0, -0), 1, &count);
      test_i32(fun(-0, 0), 1, &count);
      test_i32(fun(-0, -0), 1, &count);
      test_i32(fun(0.01, 0.01), 1, &count);
      test_i32(fun(-0.01, -0.01), 1, &count);
      test_i32(fun(0, 0.01), 0, &count);
      test_i32(fun(0.01, 0), 0, &count);
      test_i32(fun(0.01, -0.01), 0, &count);
      test_i32(fun(-0.01, 0.01), 0, &count);
    }
    {
      // f32_ne
      printf("f32_ne\n");
      int count = 0;
      func = module->funcs.data[1];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      int((*fun)(float, float)) = func->code;

      test_i32(fun(0, 0), 0, &count);
      test_i32(fun(0, -0), 0, &count);
      test_i32(fun(-0, 0), 0, &count);
      test_i32(fun(-0, -0), 0, &count);
      test_i32(fun(0.01, 0.01), 0, &count);
      test_i32(fun(-0.01, -0.01), 0, &count);
      test_i32(fun(0, 0.01), 1, &count);
      test_i32(fun(0.01, 0), 1, &count);
      test_i32(fun(0.01, -0.01), 1, &count);
      test_i32(fun(-0.01, 0.01), 1, &count);
    }

    {
      // f32_lt
      printf("f32_lt\n");
      int count = 0;
      func = module->funcs.data[2];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      int((*fun)(float, float)) = func->code;

      test_i32(fun(0, 0), 0, &count);
      test_i32(fun(0, -0), 0, &count);
      test_i32(fun(-0, 0), 0, &count);
      test_i32(fun(-0, -0), 0, &count);
      test_i32(fun(0.01, 0.01), 0, &count);
      test_i32(fun(-0.01, -0.01), 0, &count);
      test_i32(fun(0, 0.01), 1, &count);
      test_i32(fun(0.01, 0), 0, &count);
      test_i32(fun(0.01, -0.01), 0, &count);
      test_i32(fun(-0.01, 0.01), 1, &count);
    }

    {
      // f32_le
      printf("f32_le\n");
      int count = 0;
      func = module->funcs.data[3];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      int((*fun)(float, float)) = func->code;

      test_i32(fun(0, 0), 1, &count);
      test_i32(fun(0, -0), 1, &count);
      test_i32(fun(-0, 0), 1, &count);
      test_i32(fun(-0, -0), 1, &count);
      test_i32(fun(0.01, 0.01), 1, &count);
      test_i32(fun(-0.01, -0.01), 1, &count);
      test_i32(fun(0, 0.01), 1, &count);
      test_i32(fun(0.01, 0), 0, &count);
      test_i32(fun(0.01, -0.01), 0, &count);
      test_i32(fun(-0.01, 0.01), 1, &count);
    }

    {
      // f32_gt
      printf("f32_gt\n");
      int count = 0;
      func = module->funcs.data[4];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      int((*fun)(float, float)) = func->code;

      test_i32(fun(0, 0), 0, &count);
      test_i32(fun(0, -0), 0, &count);
      test_i32(fun(-0, 0), 0, &count);
      test_i32(fun(-0, -0), 0, &count);
      test_i32(fun(0.01, 0.01), 0, &count);
      test_i32(fun(-0.01, -0.01), 0, &count);
      test_i32(fun(0, 0.01), 0, &count);
      test_i32(fun(0.01, 0), 1, &count);
      test_i32(fun(0.01, -0.01), 1, &count);
      test_i32(fun(-0.01, 0.01), 0, &count);
    }

    {
      // f32_le
      printf("f32_ge\n");
      int count = 0;
      func = module->funcs.data[5];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      int((*fun)(float, float)) = func->code;

      test_i32(fun(0, 0), 1, &count);
      test_i32(fun(0, -0), 1, &count);
      test_i32(fun(-0, 0), 1, &count);
      test_i32(fun(-0, -0), 1, &count);
      test_i32(fun(0.01, 0.01), 1, &count);
      test_i32(fun(-0.01, -0.01), 1, &count);
      test_i32(fun(0, 0.01), 0, &count);
      test_i32(fun(0.01, 0), 1, &count);
      test_i32(fun(0.01, -0.01), 1, &count);
      test_i32(fun(-0.01, 0.01), 0, &count);
    }
  } else if (strcmp(filename, "test/spec/f32.0.wasm") == 0) {
    {
      // f32_add
      printf("f32_add\n");
      int count = 0;
      func = module->funcs.data[0];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      float((*fun)(float, float)) = func->code;

      test_f32(fun(0, 0), 0, &count);
      test_f32(fun(0, -1e-149), -1e-149, &count);
      test_f32(fun(0, 1e-149), 1e-149, &count);
      test_f32(fun(1e-149, 0), 1e-149, &count);
      test_f32(fun(-1e-149, 0), -1e-149, &count);
      test_f32(fun(1e-149, -1e-149), 0, &count);
      test_f32(fun(-1e-149, 1e-149), 0, &count);
    }
    {
      // f32_sub
      printf("f32_sub\n");
      int count = 0;
      func = module->funcs.data[1];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      float((*fun)(float, float)) = func->code;

      test_f32(fun(0, 0), 0, &count);
      test_f32(fun(0, -1e-149), 1e-149, &count);
      test_f32(fun(0, 1e-149), -1e-149, &count);
      test_f32(fun(1e-149, 0), 1e-149, &count);
      test_f32(fun(-1e-149, 0), -1e-149, &count);
      test_f32(fun(1e-149, 1e-149), 0, &count);
      test_f32(fun(-1e-149, -1e-149), 0, &count);
    }
    {
      // f32_mul
      printf("f32_mul\n");
      int count = 0;
      func = module->funcs.data[2];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      float((*fun)(float, float)) = func->code;

      test_f32(fun(0, 0), 0, &count);
      test_f32(fun(0, -1e-149), 0, &count);
      test_f32(fun(0, 1e-149), 0, &count);
      test_f32(fun(1e-149, 1), 1e-149, &count);
      test_f32(fun(-1e-149, 1), -1e-149, &count);
    }
    {
      // f32_div
      printf("f32_div\n");
      int count = 0;
      func = module->funcs.data[3];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      float((*fun)(float, float)) = func->code;

      test_f32(fun(-0.0, -1e-45), 0.0, &count);
      test_f32(fun(-0.0, -1e-149), 0.0, &count);
      test_f32(fun(0, 1e-149), 0, &count);
      test_f32(fun(1e-149, 1), 1e-149, &count);
      test_f32(fun(-1e-149, 1), -1e-149, &count);
      test_f32(fun(1e-149, 1e-149), 1, &count);
      test_f32(fun(-1e-149, -1e-149), 1, &count);
    }
    {
      // f32_sqrt
      printf("f32_sqrt\n");
      int count = 0;
      func = module->funcs.data[4];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      float((*fun)(float)) = func->code;

      test_f32(fun(-0.0), 0.0, &count);
      test_f32(fun(0.0), 0.0, &count);
      test_f32(fun(1e-126), 1e-63, &count);
      test_f32(fun(1), 1, &count);
      test_f32(fun(1.44), 1.2, &count);
    }
    {
      // f32_min
      printf("f32_min\n");
      int count = 0;
      func = module->funcs.data[5];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      float((*fun)(float, float)) = func->code;

      test_f32(fun(0, -1e-149), -1e-149, &count);
      test_f32(fun(0, 1e-149), 0, &count);
      test_f32(fun(1e-149, -1e-149), -1e-149, &count);
      test_f32(fun(-1e-149, 1e-149), -1e-149, &count);
      test_f32(fun(1e-149, 1e-149), 1e-149, &count);
      test_f32(fun(-1e-149, -1e-149), -1e-149, &count);
    }
    {
      // f32_max
      printf("f32_max\n");
      int count = 0;
      func = module->funcs.data[6];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      float((*fun)(float, float)) = func->code;

      test_f32(fun(0, -1e-149), 0, &count);
      test_f32(fun(0, 1e-149), 1e-149, &count);
      test_f32(fun(1e-149, -1e-149), 1e-149, &count);
      test_f32(fun(-1e-149, 1e-149), 1e-149, &count);
      test_f32(fun(1e-149, 1e-149), 1e-149, &count);
      test_f32(fun(-1e-149, -1e-149), -1e-149, &count);
    }
    {
      // f32_ceil
      printf("f32_ceil\n");
      int count = 0;
      func = module->funcs.data[7];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      float((*fun)(float)) = func->code;

      test_f32(fun(-0.0), -0.0, &count);
      test_f32(fun(0), 0, &count);
      test_f32(fun(-1e-149), -0.0, &count);
      test_f32(fun(1e-149), 1, &count);
      test_f32(fun(-1e-126), -0.0, &count);
      test_f32(fun(1e-126), 1, &count);
      test_f32(fun(-1e-1), -0.0, &count);
      test_f32(fun(1e-1), 1, &count);
      test_f32(fun(-1), -1, &count);
      test_f32(fun(1), 1, &count);
    }
    {
      // f32_floor
      printf("f32_floor\n");
      int count = 0;
      func = module->funcs.data[8];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      float((*fun)(float)) = func->code;

      test_f32(fun(-0.0), -0.0, &count);
      test_f32(fun(0), 0, &count);
      test_f32(fun(-1e-149), -1.0, &count);
      test_f32(fun(1e-149), 0, &count);
      test_f32(fun(-1e-126), -1.0, &count);
      test_f32(fun(1e-126), 0, &count);
      test_f32(fun(-1e-1), -1.0, &count);
      test_f32(fun(1e-1), 0, &count);
      test_f32(fun(-1), -1, &count);
      test_f32(fun(1), 1, &count);
    }
  } else if (strcmp(filename, "test/spec/f32_bitwise.0.wasm") == 0) {
    {
      // f32_abs
      printf("f32_abs\n");
      int count = 0;
      func = module->funcs.data[0];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      float((*fun)(float)) = func->code;

      test_f32(fun(-0.0), 0.0, &count);
      test_f32(fun(0.0), 0.0, &count);
      test_f32(fun(1e-149), 1e-149, &count);
      test_f32(fun(-1e-149), 1e-149, &count);
      test_f32(fun(1e-126), 1e-126, &count);
      test_f32(fun(-1e-126), 1e-126, &count);
      test_f32(fun(-1.0), 1.0, &count);
      test_f32(fun(1.0), 1.0, &count);
    }
    {
      // f32_neg
      printf("f32_neg\n");
      int count = 0;
      func = module->funcs.data[1];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      float((*fun)(float)) = func->code;

      test_f32(fun(-0.0), 0.0, &count);
      test_f32(fun(0.0), -0.0, &count);
      test_f32(fun(1e-149), -1e-149, &count);
      test_f32(fun(-1e-149), 1e-149, &count);
      test_f32(fun(1e-126), -1e-126, &count);
      test_f32(fun(-1e-126), 1e-126, &count);
      test_f32(fun(-1.0), 1.0, &count);
      test_f32(fun(1.0), -1.0, &count);
    }
    {
      // f32_copysign
      printf("f32_copysign\n");
      int count = 0;
      func = module->funcs.data[2];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      float((*fun)(float, float)) = func->code;

      test_f32(fun(0, 0), 0, &count);
      test_f32(fun(0, -1e-149), -0.0, &count);
      test_f32(fun(0, 1e-149), 0, &count);
      test_f32(fun(0, -1e-1), -0.0, &count);
      test_f32(fun(0, 1e-1), 0, &count);
      test_f32(fun(-1e-149, -0.0), -1e-149, &count);
      test_f32(fun(-1e-149, 0.0), 1e-149, &count);
      test_f32(fun(1e-149, -0.0), -1e-149, &count);
      test_f32(fun(1e-149, 0.0), 1e-149, &count);
      test_f32(fun(-1e-149, -1e-149), -1e-149, &count);
      test_f32(fun(-1e-149, 1e-149), 1e-149, &count);
      test_f32(fun(1e-149, -1e-149), -1e-149, &count);
      test_f32(fun(1e-149, 1e-149), 1e-149, &count);
      test_f32(fun(-1, -1e-126), -1, &count);
      test_f32(fun(-1, 1e-126), 1, &count);
      test_f32(fun(1, -1e-126), -1, &count);
      test_f32(fun(1, 1e-126), 1, &count);
      test_f32(fun(-1, -1), -1, &count);
      test_f32(fun(-1, 1), 1, &count);
      test_f32(fun(1, -1), -1, &count);
      test_f32(fun(1, 1), 1, &count);
    }
  } else if (strcmp(filename, "test/spec/i32.0.wasm") == 0) {
    {
      // i32_div_s
      printf("i32_div_s\n");
      int count = 0;
      func = module->funcs.data[3];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      int32_t((*fun)(int32_t, int32_t)) = func->code;

      test_i32(fun(1, 1), 1, &count);
      test_i32(fun(0, 1), 0, &count);
      test_i32(fun(0, -1), 0, &count);
      test_i32(fun(-1, -1), 1, &count);
      test_i32(fun(0x80000000, 2), 0xc0000000, &count);
      test_i32(fun(0x80000001, 1000), 0xffdf3b65, &count);
      test_i32(fun(5, 2), 2, &count);
      test_i32(fun(-5, 2), -2, &count);
      test_i32(fun(5, -2), -2, &count);
      test_i32(fun(-5, -2), 2, &count);
      test_i32(fun(7, 3), 2, &count);
      test_i32(fun(-7, 3), -2, &count);
      test_i32(fun(7, -3), -2, &count);
      test_i32(fun(-7, -3), 2, &count);
      test_i32(fun(11, 5), 2, &count);
      test_i32(fun(17, 7), 2, &count);
    }
    {
      // i32_div_u
      printf("i32_div_u\n");
      int count = 0;
      func = module->funcs.data[4];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      int32_t((*fun)(int32_t, int32_t)) = func->code;

      test_i32(fun(1, 1), 1, &count);
      test_i32(fun(0, 1), 0, &count);
      test_i32(fun(0, -1), 0, &count);
      test_i32(fun(-1, -1), 1, &count);
      test_i32(fun(0x80000000, -1), 0, &count);
      test_i32(fun(0x80000000, 2), 0x40000000, &count);
      test_i32(fun(0x8ff00ff0, 0x10001), 0x8fef, &count);
      test_i32(fun(0x80000001, 1000), 0x20c49b, &count);
      test_i32(fun(5, 2), 2, &count);
      test_i32(fun(-5, 2), 0x7ffffffd, &count);
      test_i32(fun(5, -2), 0, &count);
      test_i32(fun(-5, -2), 0, &count);
      test_i32(fun(7, 3), 2, &count);
      test_i32(fun(11, 5), 2, &count);
      test_i32(fun(17, 7), 2, &count);
    }
    {
      // i32_rem_s
      printf("i32_rem_s\n");
      int count = 0;
      func = module->funcs.data[5];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      int32_t((*fun)(int32_t, int32_t)) = func->code;

      test_i32(fun(1, 1), 0, &count);
      test_i32(fun(0, 1), 0, &count);
      test_i32(fun(0, -1), 0, &count);
      test_i32(fun(-1, -1), 0, &count);
      test_i32(fun(0x80000000, -1), 0, &count);
      test_i32(fun(0x80000000, 2), 0, &count);
      test_i32(fun(0x80000001, 1000), -647, &count);
      test_i32(fun(5, 2), 1, &count);
      test_i32(fun(-5, 2), -1, &count);
      test_i32(fun(5, -2), 1, &count);
      test_i32(fun(-5, -2), -1, &count);
      test_i32(fun(7, 3), 1, &count);
      test_i32(fun(-7, 3), -1, &count);
      test_i32(fun(7, -3), 1, &count);
      test_i32(fun(-7, -3), -1, &count);
      test_i32(fun(11, 5), 1, &count);
      test_i32(fun(17, 7), 3, &count);
    }
    {
      // i32_rem_u
      printf("i32_rem_u\n");
      int count = 0;
      func = module->funcs.data[6];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      int32_t((*fun)(int32_t, int32_t)) = func->code;

      test_i32(fun(1, 1), 0, &count);
      test_i32(fun(0, 1), 0, &count);
      test_i32(fun(0, -1), 0, &count);
      test_i32(fun(-1, -1), 0, &count);
      test_i32(fun(0x80000000, -1), 0x80000000, &count);
      test_i32(fun(0x80000000, 2), 0, &count);
      test_i32(fun(0x8ff00ff0, 0x10001), 0x8001, &count);
      test_i32(fun(0x80000001, 1000), 649, &count);
      test_i32(fun(5, 2), 1, &count);
      test_i32(fun(-5, 2), 1, &count);
      test_i32(fun(5, -2), 5, &count);
      test_i32(fun(-5, -2), -5, &count);
      test_i32(fun(7, 3), 1, &count);
      test_i32(fun(11, 5), 1, &count);
      test_i32(fun(17, 7), 3, &count);
    }
  } else if (strcmp(filename, "test/spec/f64_bitwise.0.wasm") == 0) {
    {
      // f64_abs
      printf("f64_abs\n");
      int count = 0;
      func = module->funcs.data[0];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      double((*fun)(double)) = func->code;

      test_f64(fun(-0.0), 0.0, &count);
      test_f64(fun(0.0), 0.0, &count);
      // test_f64(fun(1e-1022), 1e-1022, &count);
      // test_f64(fun(-1e-1022), 1e-1022, &count);
      test_f64(fun(1e-126), 1e-126, &count);
      test_f64(fun(-1e-126), 1e-126, &count);
      test_f64(fun(-1.0), 1.0, &count);
      test_f64(fun(1.0), 1.0, &count);
    }
    {
      // f64_neg
      printf("f64_neg\n");
      int count = 0;
      func = module->funcs.data[1];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      double((*fun)(double)) = func->code;

      test_f64(fun(-0.0), 0.0, &count);
      test_f64(fun(0.0), -0.0, &count);
      // test_f64(fun(1e-1022), -1e-1022, &count);
      // test_f64(fun(-1e-1022), 1e-1022, &count);
      test_f64(fun(1e-126), -1e-126, &count);
      test_f64(fun(-1e-126), 1e-126, &count);
      test_f64(fun(-1.0), 1.0, &count);
      test_f64(fun(1.0), -1.0, &count);
    }
    {
      // f64_copysign
      printf("f64_copysign\n");
      int count = 0;
      func = module->funcs.data[2];
#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
#endif
      double((*fun)(double, double)) = func->code;

      test_f64(fun(0, 0), 0, &count);
      // test_f64(fun(0, -1e-1022), -0.0, &count);
      // test_f64(fun(0, 1e-1022), 0, &count);
      test_f64(fun(0, -1e-1), -0.0, &count);
      test_f64(fun(0, 1e-1), 0, &count);
      /*test_f64(fun(-1e-1022, -0.0), -1e-1022, &count);
      test_f64(fun(-1e-1022, 0.0), 1e-1022, &count);
      test_f64(fun(1e-1022, -0.0), -1e-1022, &count);
      test_f64(fun(1e-1022, 0.0), 1e-1022, &count);
      test_f64(fun(-1e-1022, -1e-1022), -1e-1022, &count);
      test_f64(fun(-1e-1022, 1e-1022), 1e-1022, &count);
      test_f64(fun(1e-1022, -1e-1022), -1e-1022, &count);
      test_f64(fun(1e-1022, 1e-1022), 1e-1022, &count);
      test_f64(fun(-1, -1e-1022), -1, &count);
      test_f64(fun(-1, 1e-1022), 1, &count);
      test_f64(fun(1, -1e-1022), -1, &count);
      test_f64(fun(1, 1e-1022), 1, &count);*/
      test_f64(fun(-1, -1), -1, &count);
      test_f64(fun(-1, 1), 1, &count);
      test_f64(fun(1, -1), -1, &count);
      test_f64(fun(1, 1), 1, &count);
    }
  } else if (strcmp(filename, "test/br_table.0.wasm") == 0) {
    test_br_table(module);
  } else if (strcmp(filename, "test/spec/conversions.0.wasm") == 0) {
    test_conversions(module);
  } else if (strcmp(filename, "test/test_call.wasm") == 0) {
    {
      // test_call
      printf("test_call\n");
      int count = 0;
      func = module->funcs.data[1];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      int((*fun)(int, int)) = func->code;

      test_i32(fun(1, 2), 3, &count);
    }
    {
      // test_call
      printf("test_call: parameter passing\n");
      int count = 0;
      func = module->funcs.data[3];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      int((*fun)(int, int, int, int, int, int, int, int, int)) = func->code;

      test_i32(fun(1, 2, 3, 4, 5, 6, 7, 8, 9), -1, &count);
      test_i32(fun(1, 2, 3, 4, 5, 6, 20, 8, 4), 4, &count);
      test_i32(fun(1, 2, 3, 4, 5, 6, 30, 20, 50), -30, &count);

      printf("test_call: parameter passing callee\n");
      func = module->funcs.data[2];
      dump_compile_code((uint8_t*)func->code, func->size);
    }
    {
      // br_table
      printf("br_table\n");
      int count = 0;
      func = module->funcs.data[4];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      int((*fun)(int)) = func->code;

      test_i32(fun(0), 0, &count);
      test_i32(fun(1), 1, &count);
      test_i32(fun(2), 0, &count);
      test_i32(fun(3), 1, &count);
      test_i32(fun(100), 0, &count);
      test_i32(fun(101), 1, &count);
    }
    {
      // br_table
      printf("load/store i32\n");
      int count = 0;
      func = module->funcs.data[5];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      int((*fun)(int, int)) = func->code;

      test_i32(fun(1, 999), 999, &count);
      test_i32(fun(10, 5), 5, &count);
      test_i32(fun(100, -123), -123, &count);
    }
    {
      // br_table
      printf("load/store i64\n");
      int count = 0;
      func = module->funcs.data[6];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      long((*fun)(int, long)) = func->code;

      test_i64(fun(1, 999), 999, &count);
      test_i64(fun(10, 5), 5, &count);
      test_i64(fun(100, -123), -123, &count);
    }
    {
      // br_table
      printf("load/store f32\n");
      int count = 0;
      func = module->funcs.data[6];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      float((*fun)(int, float)) = func->code;

      test_f32(fun(1, 0.0), 0.0, &count);
      test_f32(fun(10, 1.2345), 1.2345, &count);
      test_f32(fun(100, -0.0006), -0.0006, &count);
    }
    {
      // br_table
      printf("load/store f64\n");
      int count = 0;
      func = module->funcs.data[6];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      double((*fun)(int, double)) = func->code;

      test_f64(fun(1, 0.0), 0.0, &count);
      test_f64(fun(10, 1.2345), 1.2345, &count);
      test_f64(fun(100, -0.0006), -0.0006, &count);
    }
  } else if (strcmp(filename, "test/test_emscripten.wasm") == 0) {
    {
      printf("test_emscripten call\n");
      int count = 0;
      func = module->funcs.data[14];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      int((*fun)(int, int)) = func->code;

      test_i32(fun(1, 2), 56, &count);
    }
    {
      printf("test_emscripten get_global\n");
      int count = 0;
      func = module->funcs.data[15];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      int((*fun)()) = func->code;

      test_i32(fun(), 11056, &count);
    }
    {
      printf("test_emscripten get_global\n");
      int count = 0;
      func = module->funcs.data[16];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      double((*fun)()) = func->code;

      test_f64(fun(), 1.23456, &count);
    }
    {
      printf("test_emscripten call indirect\n");
      func = module->funcs.data[18];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      void((*fun)()) = func->code;

      fun();
    }
  } else if (strcmp(filename, "test/hello.wasm") == 0) {
    {
      // test_call
      printf("hello test fun 37\n");
      int count = 0;
      func = module->funcs.data[37];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      int((*fun)(uint32_t, uint32_t, uint32_t)) = func->code;

      test_i32(fun(3838243688, 12, 3412), 1, &count);
    }
    {
      // test_call
      printf("hello\n");
      int count = 0;
      func = module->funcs.data[21];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      int((*fun)(int, int, int, int)) = func->code;

      test_i32(fun(0, 0, 0, 0), 1, &count);
    }
  } else if (strcmp(filename, "test/debug/hello.wasm") == 0) {
#if 0
    {
      printf("_printf\n");
      int count = 0;
      func = module->funcs.data[66];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code,
                        func->size);
      //#endif
      int((*fun)(int, int)) = func->code;

      test_i32(fun(3784, 11056), 12, &count);
    }
#endif
#if 0
    {
      printf("_vfprintf\n");
      int count = 0;
      func = module->funcs.data[42];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code,
                        func->size);
      //#endif
      int((*fun)(int, int, int)) = func->code;

      test_i32(fun(3412, 3784, 11056), 12, &count);
    }
#endif
#if 0
    {
      printf("___fwritex\n");
      int count = 0;
      func = module->funcs.data[60];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code,
                        func->size);
      //#endif
      int((*fun)(int, int, int)) = func->code;

      test_i32(fun(3784, 12, 3412), 12, &count);
    }
#endif
#if 0
    {
      printf("___towrite\n");
      int count = 0;
      func = module->funcs.data[61];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code,
                        func->size);
      //#endif
      int((*fun)(int)) = func->code;

      test_i32(fun(3412), 0, &count);
    }
#endif
#if 0
    {
      printf("_main\n");
      int count = 0;
      func = module->funcs.data[21];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      int((*fun)()) = func->code;

      test_i32(fun(), 0, &count);
    }
#endif
    {
      printf("check global\n");
      int count = 0;
      func = module->funcs.data[78];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      int((*fun)()) = func->code;

      test_i32(fun(), 0, &count);
    }
  } else if (strcmp(filename, "test/main_args.wasm") == 0) {
    {
      printf("_main\n");
      int count = 0;
      struct Function* stack_alloc = module->funcs.data[14];
      func = module->funcs.data[21];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      // int((*fun)(uint32_t, uint32_t)) = func->code;

      char* argv[] = { "test", "hello", "world" };

      test_i32(emscripten_invoke_main(stack_alloc, func, 3, argv), 0, &count);
    }
  } else if (strcmp(filename, "test/emscripten2wasm/test.wasm") == 0) {
    {
      printf("_main\n");
      int count = 0;
      func = module->funcs.data[21];
      //#if DUMP_CODE
      dump_compile_code((uint8_t*)func->code, func->size);
      //#endif
      int((*fun)()) = func->code;

      test_i32(fun(), 0, &count);
    }
  }


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
