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

#ifndef __SGXWASM__VECTOR_H__
#define __SGXWASM__VECTOR_H__

#include <sgxwasm/sys.h>

#define INIT_VECTOR_CAPACITY 10

int sgxwasm_vector_set_size(void*, size_t*, size_t, size_t);

void *sgxwasm_malloc(size_t);
void *sgxwasm_calloc(size_t, size_t);

void sgxwasm_vector_init(size_t*, size_t*, void*);
int sgxwasm_vector_grow(size_t*, size_t*, void*, size_t);
void sgxwasm_vector_shrink(size_t*, void*, size_t);
int sgxwasm_vector_resize(size_t*, size_t*, void*, size_t, size_t);
void sgxwasm_vector_free(size_t*, size_t*, void*, size_t);

#if SGXWASM_LOADTIME_MEMORY
uint64_t sgxwasm_get_alloc_counter();
uint64_t sgxwasm_get_dealloc_counter();
void sgxwasm_reset_alloc_counters();
void sgxwasm_increase_alloc_counter(uint64_t n);
void sgxwasm_increase_dealloc_counter(uint64_t n);
#endif

#define DEFINE_ANON_VECTOR(type)                                               \
  struct                                                                       \
  {                                                                            \
    size_t capacity;                                                           \
    size_t size;                                                               \
    type* data;                                                                \
  }

#define VECTOR_INIT(_vector)                                                   \
  sgxwasm_vector_init(&(_vector)->capacity, &(_vector)->size, &(_vector)->data)

#define VECTOR_GROW(_vector)                                                   \
  sgxwasm_vector_grow(&(_vector)->capacity, &(_vector)->size,                  \
                      &(_vector)->data, sizeof((_vector)->data[0]))

#define DECLARE_VECTOR_INIT(_name, _type) void _name##_init(_type* _vector)
#define DECLARE_VECTOR_GROW(_name, _type) int _name##_grow(_type* _vector)
#define DECLARE_VECTOR_SHRINK(_name, _type) int _name##_shrink(_type* _vector)
#define DECLARE_VECTOR_FREE(_name, _type) void _name##free(_type* _vector)

#define DEFINE_VECTOR_INIT(_name, _type)                                       \
  void _name##_init(_type* _vector)                                            \
  {                                                                            \
    return sgxwasm_vector_init(&_vector->capacity, &_vector->size,             \
                               &_vector->data);                                \
  }

#define DEFINE_VECTOR_GROW(_name, _type)                                       \
  int _name##_grow(_type* _vector)                                             \
  {                                                                            \
    return sgxwasm_vector_grow(&_vector->capacity, &_vector->size,             \
                               &_vector->data, sizeof(_vector->data[0]));      \
  }

#define DEFINE_VECTOR_SHRINK(_name, _type)                                     \
  void _name##_shrink(_type* _vector)                                          \
  {                                                                            \
    sgxwasm_vector_shrink(&_vector->size, _vector->data,                       \
                          sizeof(_vector->data[0]));                           \
  }

#define DEFINE_VECTOR_RESIZE(_name, _type)                                     \
  int _name##_resize(_type* _vector, size_t _new_size)                         \
  {                                                                            \
    return sgxwasm_vector_resize(&_vector->capacity, &_vector->size,           \
                                 &_vector->data, sizeof(_vector->data[0]),     \
                                 _new_size);                                   \
  }

#define DEFINE_VECTOR_FREE(_name, _type)                                       \
  void _name##_free(_type* _vector)                                            \
  {                                                                            \
    sgxwasm_vector_free(&_vector->capacity, &_vector->size,                    \
                        &_vector->data, sizeof(_vector->data[0]));             \
  }

#endif
