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

#include <sgxwasm/sys.h>
#include <sgxwasm/vector.h>

void
sgxwasm_vector_init(size_t* capacity_ptr, size_t* size_ptr, void* data_ptr)
{
  void** data = (void**)data_ptr;
  *capacity_ptr = 0;
  *size_ptr = 0;
  *data = NULL;
}

// Increase the current size by one.
int
sgxwasm_vector_grow(size_t* capacity_ptr,
                    size_t* size_ptr,
                    void* data_ptr,
                    size_t data_size)
{
  void** data = (void**)data_ptr;
  size_t capacity = *capacity_ptr;
  size_t size = *size_ptr;
  size_t total_data_size;
  size_t ret = 0;

  if (capacity == 0) { // Initialize if the capacity is not set.
    //assert(size == 0 && *data == NULL);
    assert(size == 0);
    assert(*data == NULL);
    *capacity_ptr = INIT_VECTOR_CAPACITY;
    if (__builtin_umull_overflow(*capacity_ptr, data_size, &total_data_size)) {
      goto error;
    }
    *data = malloc(total_data_size);
    if (!*data) {
      goto error;
    }
  } else if (size == capacity) { // Double the capacity if the stack is full.
    assert(capacity > 0);
    capacity *= 2;
    if (__builtin_umull_overflow(capacity, data_size, &total_data_size)) {
      goto error;
    }
    *data = realloc(*data, total_data_size);
    if (!*data) {
      goto error;
    }
    *capacity_ptr = capacity;
  }
  *size_ptr += 1;
  ret = 1;
error:
  return ret;
}

// Decrease the current size by one.
void
sgxwasm_vector_shrink(size_t* size_ptr, void* data_ptr, size_t data_size)
{
  // void** data = (void**)data_ptr;
  assert(*size_ptr > 0);
  (void)data_size;
  (void)data_ptr;
  // Zero out the current content.
  // memset(&data[*size_ptr - 1], 0, data_size);
  *size_ptr -= 1;
}

int
sgxwasm_vector_resize(size_t* capacity_ptr,
                      size_t* size_ptr,
                      void* data_ptr,
                      size_t data_size,
                      size_t new_size)
{
  void** data = (void**)data_ptr;
  size_t capacity = *capacity_ptr;
  size_t size = *size_ptr;
  size_t total_data_size = 0;
  int ret = 0;

  if (new_size < capacity) { // Case of new_size is less than capacity.
    if (new_size != size) {
      *size_ptr = new_size;
    }
  } else { // Case of new_size is larger than capacity.
    if (capacity == 0) {
      capacity = INIT_VECTOR_CAPACITY;
    }
    // XXX: Check if this causes over-allocate the heap.
    while (capacity < new_size) {
      capacity *= 2;
    }
    if (__builtin_umull_overflow(capacity, data_size, &total_data_size)) {
      goto error;
    }
    *data = realloc(*data, total_data_size);
    if (!*data) {
      goto error;
    }
    *capacity_ptr = capacity;
    *size_ptr = new_size;
  }
  ret = 1;
error:
  return ret;
}

void
sgxwasm_vector_free(size_t* capacity_ptr, size_t* size_ptr, void* data_ptr)
{
  void** data = (void**)data_ptr;
  assert(data_ptr != NULL && *data != NULL);
  free(*data);
  *capacity_ptr = 0;
  *size_ptr = 0;
}
