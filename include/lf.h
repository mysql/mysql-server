/* Copyright (c) 2007, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef _lf_h
#define _lf_h

#include "my_global.h"
#include "my_atomic.h"
#include "my_sys.h"
#include "hash.h"

C_MODE_START

/*
  wait-free dynamic array, see lf_dynarray.c

  4 levels of 256 elements each mean 4311810304 elements in an array - it
  should be enough for a while
*/
#define LF_DYNARRAY_LEVEL_LENGTH 256
#define LF_DYNARRAY_LEVELS       4

typedef struct {
  void * volatile level[LF_DYNARRAY_LEVELS];
  uint size_of_element;
} LF_DYNARRAY;

typedef int (*lf_dynarray_func)(void *, void *);

void lf_dynarray_init(LF_DYNARRAY *array, uint element_size);
void lf_dynarray_destroy(LF_DYNARRAY *array);
void *lf_dynarray_value(LF_DYNARRAY *array, uint idx);
void *lf_dynarray_lvalue(LF_DYNARRAY *array, uint idx);
int lf_dynarray_iterate(LF_DYNARRAY *array, lf_dynarray_func func, void *arg);

/*
  pin manager for memory allocator, lf_alloc-pin.c
*/

#define LF_PINBOX_PINS 4
#define LF_PURGATORY_SIZE 10

typedef void lf_pinbox_free_func(void *, void *, void*);

typedef struct {
  LF_DYNARRAY pinarray;
  lf_pinbox_free_func *free_func;
  void *free_func_arg;
  uint free_ptr_offset;
  uint32 volatile pinstack_top_ver;         /* this is a versioned pointer */
  uint32 volatile pins_in_array;            /* number of elements in array */
} LF_PINBOX;

typedef struct st_lf_pins {
  void * volatile pin[LF_PINBOX_PINS];
  LF_PINBOX *pinbox;
  void  *purgatory;
  uint32 purgatory_count;
  uint32 volatile link;
/* we want sizeof(LF_PINS) to be 64 to avoid false sharing */
#if SIZEOF_INT*2+SIZEOF_CHARP*(LF_PINBOX_PINS+2) != 64
  char pad[64-sizeof(uint32)*2-sizeof(void*)*(LF_PINBOX_PINS+2)];
#endif
} LF_PINS;

/*
  compile-time assert, to require "no less than N" pins
  it's enough if it'll fail on at least one compiler, so
  we'll enable it on GCC only, which supports zero-length arrays.
*/
#if defined(__GNUC__) && defined(MY_LF_EXTRA_DEBUG)
#define LF_REQUIRE_PINS(N)                                      \
  static const char require_pins[LF_PINBOX_PINS-N]              \
                             MY_ATTRIBUTE ((unused));          \
  static const int LF_NUM_PINS_IN_THIS_FILE= N;
#else
#define LF_REQUIRE_PINS(N)
#endif

static inline void lf_pin(LF_PINS *pins, int pin, void *addr)
{
#if defined(__GNUC__) && defined(MY_LF_EXTRA_DEBUG)
  assert(pin < LF_NUM_PINS_IN_THIS_FILE);
#endif
  my_atomic_storeptr(&pins->pin[pin], addr);
}

static inline void lf_unpin(LF_PINS *pins, int pin)
{
#if defined(__GNUC__) && defined(MY_LF_EXTRA_DEBUG)
  assert(pin < LF_NUM_PINS_IN_THIS_FILE);
#endif
  my_atomic_storeptr(&pins->pin[pin], NULL);
}

void lf_pinbox_init(LF_PINBOX *pinbox, uint free_ptr_offset,
                    lf_pinbox_free_func *free_func, void * free_func_arg);
void lf_pinbox_destroy(LF_PINBOX *pinbox);
LF_PINS *lf_pinbox_get_pins(LF_PINBOX *pinbox);
void lf_pinbox_put_pins(LF_PINS *pins);
void lf_pinbox_free(LF_PINS *pins, void *addr);


/*
  memory allocator, lf_alloc-pin.c
*/
typedef void lf_allocator_func(uchar *);

typedef struct st_lf_allocator {
  LF_PINBOX pinbox;
  uchar * volatile top;
  uint element_size;
  uint32 volatile mallocs;
  lf_allocator_func *constructor; /* called, when an object is malloc()'ed */
  lf_allocator_func *destructor;  /* called, when an object is free()'d    */
} LF_ALLOCATOR;

#define lf_alloc_init(A, B, C) lf_alloc_init2(A, B, C, NULL, NULL)
void lf_alloc_init2(LF_ALLOCATOR *allocator, uint size, uint free_ptr_offset,
                    lf_allocator_func *ctor, lf_allocator_func *dtor);
void lf_alloc_destroy(LF_ALLOCATOR *allocator);
uint lf_alloc_pool_count(LF_ALLOCATOR *allocator);

static inline void lf_alloc_direct_free(LF_ALLOCATOR *allocator, void *addr)
{
  if (allocator->destructor)
    allocator->destructor((uchar *)addr);
  my_free(addr);
}

void *lf_alloc_new(LF_PINS *pins);

struct st_lf_hash;
typedef uint lf_hash_func(const struct st_lf_hash *, const uchar *, size_t);
typedef void lf_hash_init_func(uchar *dst, const uchar* src);

#define LF_HASH_UNIQUE 1

/* lf_hash overhead per element (that is, sizeof(LF_SLIST) */
extern const int LF_HASH_OVERHEAD;

typedef struct st_lf_hash {
  LF_DYNARRAY array;                    /* hash itself */
  LF_ALLOCATOR alloc;                   /* allocator for elements */
  my_hash_get_key get_key;              /* see HASH */
  CHARSET_INFO *charset;                /* see HASH */
  lf_hash_func *hash_function;          /* see HASH */
  uint key_offset, key_length;          /* see HASH */
  uint element_size;                    /* size of memcpy'ed area on insert */
  uint flags;                           /* LF_HASH_UNIQUE, etc */
  int32 volatile size;                  /* size of array */
  int32 volatile count;                 /* number of elements in the hash */
  /**
    "Initialize" hook - called to finish initialization of object provided by
     LF_ALLOCATOR (which is pointed by "dst" parameter) and set element key
     from object passed as parameter to lf_hash_insert (pointed by "src"
     parameter). Allows to use LF_HASH with objects which are not "trivially
     copyable".
     NULL value means that element initialization is carried out by copying
     first element_size bytes from object which provided as parameter to
     lf_hash_insert.
  */
  lf_hash_init_func *initialize;
} LF_HASH;

#define lf_hash_init(A, B, C, D, E, F, G) \
          lf_hash_init2(A, B, C, D, E, F, G, NULL, NULL, NULL, NULL)
void lf_hash_init2(LF_HASH *hash, uint element_size, uint flags,
                   uint key_offset, uint key_length, my_hash_get_key get_key,
                   CHARSET_INFO *charset, lf_hash_func *hash_function,
                   lf_allocator_func *ctor, lf_allocator_func *dtor,
                   lf_hash_init_func *init);
void lf_hash_destroy(LF_HASH *hash);
int lf_hash_insert(LF_HASH *hash, LF_PINS *pins, const void *data);
void *lf_hash_search(LF_HASH *hash, LF_PINS *pins, const void *key, uint keylen);
int lf_hash_delete(LF_HASH *hash, LF_PINS *pins, const void *key, uint keylen);

static inline LF_PINS *lf_hash_get_pins(LF_HASH *hash)
{
  return lf_pinbox_get_pins(&hash->alloc.pinbox);
}

static inline void lf_hash_put_pins(LF_PINS *pins)
{
  lf_pinbox_put_pins(pins);
}

static inline void lf_hash_search_unpin(LF_PINS *pins)
{
  lf_unpin(pins, 2);
}

typedef int lf_hash_match_func(const uchar *el);
void *lf_hash_random_match(LF_HASH *hash, LF_PINS *pins,
                           lf_hash_match_func *match, uint rand_val);

C_MODE_END

#endif

