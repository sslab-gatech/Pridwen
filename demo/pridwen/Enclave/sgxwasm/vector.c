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

#include <sgxwasm/vector.h>

#if SGXWASM_LOADTIME_MEMORY
uint64_t alloc_counter = 0;
uint64_t dealloc_counter = 0;
uint64_t
sgxwasm_get_alloc_counter()
{
  return alloc_counter;
}
uint64_t
sgxwasm_get_dealloc_counter()
{
  return dealloc_counter;
}
void
sgxwasm_reset_alloc_counters()
{
  alloc_counter = 0;
  dealloc_counter = 0;
}
void
sgxwasm_increase_alloc_counter(uint64_t n)
{
  alloc_counter += n;
#if SGXWASM_DEBUG_MEMORY_USAGE
  printf("[increase alloc] %zu (+%zu)\n", alloc_counter, n);
#endif
}
void
sgxwasm_increase_dealloc_counter(uint64_t n)
{
  dealloc_counter += n;
#if SGXWASM_DEBUG_MEMORY_USAGE
  printf("[increase dealloc] %zu (+%zu)\n", dealloc_counter, n);
#endif
}
#endif

void*
sgxwasm_malloc(size_t size)
{
  void *p = malloc(size);
  if (!p) {
    printf("malloc failed - %zu\n", size);
    return NULL;
  }
#if SGXWASM_LOADTIME_MEMORY
  alloc_counter += size;
#if SGXWASM_DEBUG_MEMORY_USAGE
  printf("[malloc] %zu (+%zu)\n", alloc_counter, size);
#endif
#endif
  return p;
}

void*
sgxwasm_calloc(size_t num, size_t size) {
  void *p = calloc(num, size);
  if (!p) {
    printf("calloc failed - %zu, %zu\n", num, size);
    return NULL;
  }
#if SGXWASM_LOADTIME_MEMORY
  alloc_counter += num * size;
#if SGXWASM_DEBUG_MEMORY_USAGE
  printf("[calloc] %zu (+%zu)\n", alloc_counter, num * size);
#endif
#endif
  return p;
}

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

#if SGXWASM_LOADTIME_MEMORY
  alloc_counter += data_size;
#if SGXWASM_DEBUG_MEMORY_USAGE
  printf("[grow alloc] %zu (+%zu)\n", alloc_counter, data_size);
#endif
#endif

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
#if SGXWASM_LOADTIME_MEMORY
  dealloc_counter += data_size;
#if SGXWASM_DEBUG_MEMORY_USAGE
  printf("[shrink dealloc] %zu (+%zu)\n", dealloc_counter, data_size);
#endif
#endif
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

#if SGXWASM_LOADTIME_MEMORY
  int64_t diff = (new_size - size) * data_size;
  if (diff > 0) {
      alloc_counter += diff;
#if SGXWASM_DEBUG_MEMORY_USAGE
  printf("[resize alloc] %zu (+%lu)\n", alloc_counter, diff);
#endif
  } else {
      dealloc_counter += -diff;
#if SGXWASM_DEBUG_MEMORY_USAGE
  printf("[resize dealloc] %zu (+%lu)\n", dealloc_counter, -diff);
#endif
  }
  //alloc_counter -= size * data_size;
  //alloc_counter += new_size * data_size;
#endif

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
#if __SGX__ // Check if heap size is sufficient.
      printf("[resize] realloc failed: %zu\n", total_data_size);
#endif
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
sgxwasm_vector_free(size_t* capacity_ptr, size_t* size_ptr, void* data_ptr, size_t data_size)
{
  void** data = (void**)data_ptr;
  (void) data_size;
  if (data_ptr == NULL || *data == NULL) {
    return;
  }

#if SGXWASM_LOADTIME_MEMORY
  dealloc_counter += *size_ptr * data_size; 
#if SGXWASM_DEBUG_MEMORY_USAGE
  printf("[free dealloc] %zu (+%zu = %zu * %zu)\n",
         dealloc_counter, *size_ptr * data_size, *size_ptr, data_size);
#endif
#endif
  free(*data);
  *capacity_ptr = 0;
  *size_ptr = 0;
  *data = NULL;
}
