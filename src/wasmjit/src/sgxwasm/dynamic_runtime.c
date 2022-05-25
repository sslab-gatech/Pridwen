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

#include <sgxwasm/config.h>
#include <sgxwasm/runtime.h>

#include <sgxwasm/sys.h>

#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>

#if __SGX__
static uint64_t sgxwasm_code_pointer = (int64_t)__SGXWASM_CODE_BASE;
#else
static uint64_t sgxwasm_code_pointer;
#endif

static uint64_t sgxwasm_code_base;
static uint64_t sgxwasm_code_end;
static uint64_t sgxwasm_code_size;
// Use bitmap to record the usage of the code pages.
// The size of bitmap is obtained by (total_size / unit_size) / 8,
// as we use uint8.
#define TotalUnits CodeSize / UnitSize
#define BitmapSize TotalUnits / 8
static uint8_t sgxwasm_code_bitmap[BitmapSize];

void
sgxwasm_init_code_region(size_t size)
{
#if __SGX__
  size_t bitmap_size = BitmapSize;
#else
  void* code_region;
  time_t t;
  size_t bitmap_size = BitmapSize;
  code_region = mmap(
    NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (code_region == MAP_FAILED)
    return;
  sgxwasm_code_pointer = (uint64_t)code_region;

  // Initialize random number generator.
  srand((unsigned)time(&t));
#endif

  sgxwasm_code_base = sgxwasm_code_pointer;
  sgxwasm_code_end = sgxwasm_code_base + size;
  sgxwasm_code_size = size;
  // Initialize bitmap.
  memset(sgxwasm_code_bitmap, 0, bitmap_size);
}

int
sgxwasm_commit_code_region()
{
#if !__SGX__
  assert(sgxwasm_code_base != 0);
  return !mprotect(
    (void*)sgxwasm_code_base, sgxwasm_code_size, PROT_READ | PROT_EXEC);
#endif
}

uint64_t
sgxwasm_get_code_base()
{
  return sgxwasm_code_base;
}

// Bitmap operations
enum
{
  BITS_PER_WORD = sizeof(uint8_t) * 8
};
#define WORD_OFFSET(b) ((b) / BITS_PER_WORD)
#define BIT_OFFSET(b) ((b) % BITS_PER_WORD)

__attribute__((unused)) static void
set_bit(uint8_t* bitmap, int n)
{
  bitmap[WORD_OFFSET(n)] |= (1 << BIT_OFFSET(n));
}

__attribute__((unused)) static void
clear_bit(uint8_t* bitmap, int n)
{
  bitmap[WORD_OFFSET(n)] &= ~(1 << BIT_OFFSET(n));
}

__attribute__((unused)) static int
get_bit(uint8_t* bitmap, int n)
{
  uint8_t bit = bitmap[WORD_OFFSET(n)] & (1 << BIT_OFFSET(n));
  return bit != 0;
}

__attribute__((unused)) static int
check_and_set(uint8_t* bitmap, int base, size_t size)
{
  int range = base + size / UnitSize + 1;
  int is_available = 1;
  int i;
  if (range >= TotalUnits) {
    return 0;
  }
  for (i = base; i < range; i++) {
    if (get_bit(bitmap, i)) {
      is_available = 0;
      break;
    }
  }
  if (!is_available) {
    return 0;
  }
  for (i = base; i < range; i++) {
    set_bit(bitmap, i);
  }
  return 1;
}

__attribute__((unused)) static uint64_t
generate_rand()
{
#if __SGX__
#else
  return rand();
#endif
}

__attribute__((unused)) static int
generate_rand_index(int align)
{
  assert(align % 2 == 0);
  int factor = align / UnitSize;
  int bound = TotalUnits / factor;
  int index = (generate_rand() % bound) * factor;
  return index;
}

void*
sgxwasm_allocate_code(size_t code_size, int align, int rand)
{
  assert(align % 2 == 0);
  uint64_t code_start;
  int index;

  if (rand == 0) {
    code_start = sgxwasm_code_pointer;
    index = (code_start - sgxwasm_code_base) / UnitSize;
    if (code_size < (size_t)align) {
      code_size = align;
    }
    if (!check_and_set(sgxwasm_code_bitmap, index, code_size)) {
      assert(0); // sgxwasm_code is full.
      return NULL;
    }
    //sgxwasm_code_pointer = code_start + (code_size / UnitSize + 1) * UnitSize;
    sgxwasm_code_pointer += (code_size / UnitSize + 1) * UnitSize;
  } else {
#if __DEBUG_ASLR__
    int t = 0;
#endif
    do {
      index = generate_rand_index(align);
#if __DEBUG_ASLR__
      printf("check #%d: %d\n", t, index);
      t++;
#endif
    } while (!check_and_set(sgxwasm_code_bitmap, index, code_size));

    code_start = index * UnitSize + sgxwasm_code_base;
    assert(code_start + code_size < sgxwasm_code_end);
  }

#if __DEBUG_ASLR__
#if __linux__
  printf("[sgxwasm_allocate_code] base: %lx (+%lu)\n", code_start, code_size);
#else
  printf("[sgxwasm_allocate_code] base: %llx (+%lu)\n", code_start, code_size);
#endif
#endif

  return (void*)code_start;
}

int
sgxwasm_mark_code_segment_executable(void* code, size_t code_size)
{
  return !mprotect(code, code_size, PROT_READ | PROT_EXEC);
}

int
sgxwasm_unmap_code_segment(void* code, size_t code_size)
{
  return !munmap(code, code_size);
}

void
sgxwasm_trap(int reason)
{
  assert(reason);
}
