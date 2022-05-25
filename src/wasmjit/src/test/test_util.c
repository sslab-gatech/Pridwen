#include <wasmjit/assembler.h>

size_t
dump_output(struct SizedBuffer* output, size_t start)
{
  size_t i;
  for (i = start; i < output->n_elts; i++) {
    uint8_t byte = (uint8_t)output->elts[i];
    if (byte < 16) {
      printf("0%x ", byte);
      continue;
    }
    printf("%x ", byte);
  }
  printf("\n");

  return i;
}

void
dump_fp(uint8_t* val, uint8_t byte)
{
  int i;
  printf("----dump_fp----\n");
  for (i = (int)byte - 1; i >= 0; i--) {
    uint8_t b = val[i];
    if (b == 0) {
      printf("00 ");
      continue;
    }
    printf("%x ", b);
  }
  printf("\n----------------\n");
}

void
dump_bit(reglist_t list)
{
  int i;
  reglist_t mask = 1 << 31;
  printf("dump_bit:");
  for (i = 0; i < 32; i++) {
    if (mask & list) {
      printf(" 1");
    } else {
      printf(" 0");
    }
    mask = mask >> 1;
  }
  printf("\n");
}

void test(int lv, int rv)
{
  if (lv == rv) {
    printf("pass\n");
  } else {
    printf("fail, expected: %d, result: %d\n", lv, rv);
  }
}

void
dump_output_to_file(struct SizedBuffer* output)
{
  FILE* f;
  f = fopen("code.bin", "wb");
  printf("write to file...\n");
  fwrite(output->elts, output->n_elts, 1, f);
  fclose(f);
}

int
main()
{
  int64_t n = -1;

  printf("test: %llx (%lld)\n", n, n);
  if (is_int8(n)) {
    printf("%lld is int8\n", n);
  } else {
    printf("%lld is not int8\n", n);    
  }

  if (is_int16(n)) {
    printf("%lld is int16\n", n);
  } else {
    printf("%lld is not int16\n", n);    
  }

  if (is_int32(n)) {
    printf("%lld is int32\n", n);
  } else {
    printf("%lld is not int32\n", n);    
  }

  return 0;
}