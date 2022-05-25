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

#include <sgxwasm/util.h>

#include <sgxwasm/vector.h>

#include <sgxwasm/sys.h>

DEFINE_VECTOR_GROW(buffer, struct SizedBuffer);
DEFINE_VECTOR_RESIZE(buffer, struct SizedBuffer);

int
output_buf_reset(struct SizedBuffer* sstack)
{
  if (!buffer_resize(sstack, 0))
    return 0;
  return 1;
}

int
output_buf(struct SizedBuffer* sstack, const void* buf, size_t size)
{
  if (!buffer_resize(sstack, sstack->size + size))
    return 0;
  memcpy(sstack->data + sstack->size - size,
         buf,
         size * sizeof(sstack->data[0]));
  return 1;
}

int
output_buf_shrink(struct SizedBuffer* sstack, size_t size)
{
  if (!buffer_resize(sstack, sstack->size - size))
    return 0;
  return 1;
}

__attribute__((unused)) uint32_t
count_population(uint64_t value, uint8_t byte)
{
  uint64_t mask[] = { 0x5555555555555555, 0x3333333333333333,
                      0x0f0f0f0f0f0f0f0f, 0x00ff00ff00ff00ff,
                      0x0000ffff0000ffff, 0x00000000ffffffff };
  value = ((value >> 1) & mask[0]) + (value & mask[0]);
  value = ((value >> 2) & mask[1]) + (value & mask[1]);
  value = ((value >> 4) & mask[2]) + (value & mask[2]);
  if (byte > 1)
    value = ((value >> (byte > 1 ? 8 : 0)) & mask[3]) + (value & mask[3]);
  if (byte > 2)
    value = ((value >> (byte > 2 ? 16 : 0)) & mask[4]) + (value & mask[4]);
  if (byte > 4)
    value = ((value >> (byte > 4 ? 32 : 0)) & mask[5]) + (value & mask[5]);
  return (uint32_t)value;
}

// count_leading_zeros(value, bits) returns the number of zero bits following
// the most significant 1 bit in |value| if |value| is non-zero, otherwise it
// returns {sizeof(T) * 8}.
__attribute__((unused)) uint32_t
count_leading_zeros(uint64_t value, uint8_t bits)
{
  // Binary search algorithm taken from "Hacker's Delight" (by Henry S. Warren,
  // Jr.), figures 5-11 and 5-12.
  if (bits == 1)
    return (uint32_t)value ^ 1;
  uint64_t upper_half = value >> (bits / 2);
  uint64_t next_value = upper_half != 0 ? upper_half : value;
  uint32_t add = upper_half != 0 ? 0 : bits / 2;
  uint32_t next_bits = bits == 1 ? 1 : bits / 2;
  return count_leading_zeros(next_value, next_bits) + add;
}

// CountTrailingZeros(value) returns the number of zero bits preceding the
// least significant 1 bit in |value| if |value| is non-zero, otherwise it
// returns {sizeof(T) * 8}.
__attribute__((unused)) uint32_t
count_trailing_zeros(uint64_t value, uint8_t byte)
{
  // Fall back to popcount (see "Hacker's Delight" by Henry S. Warren, Jr.),
  // chapter 5-4. On x64, since is faster than counting in a loop and faster
  // than doing binary search.
  return count_population(~value & (value - 1u), byte);
}

__attribute__((unused)) uint8_t
is_intn(int64_t x, unsigned n)
{
  assert((0 < n) && (n < 64));
  int64_t limit = 1 << (n - 1);
  return (-limit <= x) && (x < limit);
}

__attribute__((unused)) uint8_t
is_uintn(int64_t x, unsigned n)
{
  assert((0 < n) && (n < sizeof(x) * 8));
  return !(x >> n);
}

#ifndef __KERNEL__

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

char*
sgxwasm_load_file(const char* filename, size_t* size)
{
  FILE* f = NULL;
  char* input = NULL;
  int fd = -1, ret;
  struct stat st;
  size_t rets;

  fd = open(filename, O_RDONLY);
  if (fd < 0) {
    goto error_exit;
  }

  ret = fstat(fd, &st);
  if (ret < 0) {
    goto error_exit;
  }

  f = fdopen(fd, "r");
  if (!f) {
    goto error_exit;
  }
  fd = -1;

  *size = st.st_size;
  input = malloc(st.st_size);
  if (!input) {
    goto error_exit;
  }

  rets = fread(input, sizeof(char), st.st_size, f);
  if (rets != (size_t)st.st_size) {
    goto error_exit;
  }

  goto success_exit;

error_exit:
  if (input) {
    free(input);
  }

success_exit:
  if (f) {
    fclose(f);
  }

  if (fd >= 0) {
    close(fd);
  }

  return input;
}

void
sgxwasm_unload_file(char* buf, size_t size)
{
  free(buf);
  (void)size;
}

#else

#include <linux/fs.h>

char*
sgxwasm_load_file(const char* file_name, size_t* size)
{
  void* buf;
  loff_t offsize;
  int ret;
  ret = kernel_read_file_from_path(
    file_name, &buf, &offsize, INT_MAX, READING_UNKNOWN);
  if (ret < 0)
    return NULL;

  *size = offsize;
  return buf;
}

void
sgxwasm_unload_file(char* buf, size_t size)
{
  vfree(buf);
  (void)size;
}

#endif
