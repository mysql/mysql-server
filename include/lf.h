/* Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include <my_atomic.h>

C_MODE_START

/*
  Helpers to define both func() and _func(), where
  func() is a _func() protected by my_atomic_rwlock_wrlock()
*/

#define lock_wrap(f, t, proto_args, args, lock) \
t _ ## f proto_args;                            \
static inline t f  proto_args                   \
{                                               \
  t ret;                                        \
  my_atomic_rwlock_wrlock(lock);                \
  ret= _ ## f args;                             \
  my_atomic_rwlock_wrunlock(lock);              \
  return ret;                                   \
}

#define lock_wrap_void(f, proto_args, args, lock) \
void _ ## f proto_args;                         \
static inline void f proto_args                 \
{                                               \
  my_atomic_rwlock_wrlock(lock);                \
  _ ## f args;                                  \
  my_atomic_rwlock_wrunlock(lock);              \
}

#define nolock_wrap(f, t, proto_args, args)     \
t _ ## f proto_args;                            \
static inline t f  proto_args                   \
{                                               \
  return _ ## f args;                           \
}

#define nolock_wrap_void(f, proto_args, args)   \
void _ ## f proto_args;                         \
static inline void f proto_args                 \
{                                               \
  _ ## f args;                                  \
}

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
  my_atomic_rwlock_t lock;
} LF_DYNARRAY;

typedef int (*lf_dynarray_func)(void *, void *);

void lf_dynarray_init(LF_DYNARRAY *array, uint element_size);
void lf_dynarray_destroy(LF_DYNARRAY *array);

nolock_wrap(lf_dynarray_value, void *,
            (LF_DYNARRAY *array, uint idx),
            (array, idx))
lock_wrap(lf_dynarray_lvalue, void *,
          (LF_DYNARRAY *array, uint idx),
          (array, idx),
          &array->lock)
nolock_wrap(lf_dynarray_iterate, int,
            (LF_DYNARRAY *array, lf_dynarray_func func, void *arg),
            (array, func, arg))

/*
  pin manager for memory allocator, lf_alloc-pin.c
*/

#define LF_PINBOX_PINS 4
#define LF_PURGATORY_SIZE 100

typedef void lf_pinbox_free_func(void *, void *, void*);

typedef struct {
  LF_DYNARRAY pinarray;
  lf_pinbox_free_func *free_func;
  void *free_func_arg;
  uint free_ptr_offset;
  uint32 volatile pinstack_top_ver;         /* this is a versioned pointer */
  uint32 volatile pins_in_array;            /* number of elements in array */
} LF_PINBOX;

typedef struct {
  void * volatile pin[LF_PINBOX_PINS];
  LF_PINBOX *pinbox;
  void  **stack_ends_here;
  void  *purgatory;
  uint32 purgatory_count;
  uint32 volatile link;
/* we want sizeof(LF_PINS) to be 128 to avoid false sharing */
  char pad[128-sizeof(uint32)*2
              -sizeof(LF_PINBOX *)
              -sizeof(void*)
              -sizeof(void *)*(LF_PINBOX_PINS+1)];
} LF_PINS;

/*
  shortcut macros to do an atomic_wrlock on a structure that uses pins
  (e.g. lf_hash).
*/
#define lf_rwlock_by_pins(PINS)   \
  my_atomic_rwlock_wrlock(&(PINS)->pinbox->pinarray.lock)
#define lf_rwunlock_by_pins(PINS) \
  my_atomic_rwlock_wrunlock(&(PINS)->pinbox->pinarray.lock)

/*
  compile-time assert, to require "no less than N" pins
  it's enough if it'll fail on at least one compiler, so
  we'll enable it on GCC only, which supports zero-length arrays.
*/
#if defined(__GNUC__) && defined(MY_LF_EXTRA_DEBUG)
#define LF_REQUIRE_PINS(N)                                      \
  static const char require_pins[LF_PINBOX_PINS-N]              \
                             __attribute__ ((unused));          \
  static const int LF_NUM_PINS_IN_THIS_FILE= N;
#define _lf_pin(PINS, PIN, ADDR)                                \
  (                                                             \
    assert(PIN < LF_NUM_PINS_IN_THIS_FILE),                     \
    my_atomic_storeptr(&(PINS)->pin[PIN], (ADDR))               \
  )
#else
#define LF_REQUIRE_PINS(N)
#define _lf_pin(PINS, PIN, ADDR)  my_atomic_storeptr(&(PINS)->pin[PIN], (ADDR))
#endif

#define _lf_unpin(PINS, PIN)      _lf_pin(PINS, PIN, NULL)
#define lf_pin(PINS, PIN, ADDR)   \
  do {                            \
    lf_rwlock_by_pins(PINS);      \
    _lf_pin(PINS, PIN, ADDR);     \
    lf_rwunlock_by_pins(PINS);    \
  } while (0)
#define lf_unpin(PINS, PIN)  lf_pin(PINS, PIN, NULL)
#define _lf_assert_pin(PINS, PIN) assert((PINS)->pin[PIN] != 0)
#define _lf_assert_unpin(PINS, PIN) assert((PINS)->pin[PIN] == 0)

void lf_pinbox_init(LF_PINBOX *pinbox, uint free_ptr_offset,
                    lf_pinbox_free_func *free_func, void * free_func_arg);
void lf_pinbox_destroy(LF_PINBOX *pinbox);

lock_wrap(lf_pinbox_get_pins, LF_PINS *,
          (LF_PINBOX *pinbox),
          (pinbox),
          &pinbox->pinarray.lock)
lock_wrap_void(lf_pinbox_put_pins,
               (LF_PINS *pins),
               (pins),
               &pins->pinbox->pinarray.lock)
lock_wrap_void(lf_pinbox_free,
               (LF_PINS *pins, void *addr),
               (pins, addr),
               &pins->pinbox->pinarray.lock)

/*
  memory allocator, lf_alloc-pin.c
*/

typedef struct st_lf_allocator {
  LF_PINBOX pinbox;
  uchar * volatile top;
  uint element_size;
  uint32 volatile mallocs;
  void (*constructor)(uchar *); /* called, when an object is malloc()'ed */
  void (*destructor)(uchar *);  /* called, when an object is free()'d    */
} LF_ALLOCATOR;

void lf_alloc_init(LF_ALLOCATOR *allocator, uint size, uint free_ptr_offset);
void lf_alloc_destroy(LF_ALLOCATOR *allocator);
uint lf_alloc_pool_count(LF_ALLOCATOR *allocator);
/*
  shortcut macros to access underlying pinbox functions from an LF_ALLOCATOR
  see _lf_pinbox_get_pins() and _lf_pinbox_put_pins()
*/
#define _lf_alloc_free(PINS, PTR)     _lf_pinbox_free((PINS), (PTR))
#define lf_alloc_free(PINS, PTR)       lf_pinbox_free((PINS), (PTR))
#define _lf_alloc_get_pins(A)         _lf_pinbox_get_pins(&(A)->pinbox)
#define lf_alloc_get_pins(A)           lf_pinbox_get_pins(&(A)->pinbox)
#define _lf_alloc_put_pins(PINS)      _lf_pinbox_put_pins(PINS)
#define lf_alloc_put_pins(PINS)        lf_pinbox_put_pins(PINS)
#define lf_alloc_direct_free(ALLOC, ADDR) my_free((ADDR))

lock_wrap(lf_alloc_new, void *,
          (LF_PINS *pins),
          (pins),
          &pins->pinbox->pinarray.lock)

C_MODE_END

/*
  extendible hash, lf_hash.c
*/
#include <hash.h>

C_MODE_START

#define LF_HASH_UNIQUE 1

/* lf_hash overhead per element (that is, sizeof(LF_SLIST) */
extern const int LF_HASH_OVERHEAD;

typedef struct {
  LF_DYNARRAY array;                    /* hash itself */
  LF_ALLOCATOR alloc;                   /* allocator for elements */
  my_hash_get_key get_key;              /* see HASH */
  CHARSET_INFO *charset;                /* see HASH */
  uint key_offset, key_length;          /* see HASH */
  uint element_size;                    /* size of memcpy'ed area on insert */
  uint flags;                           /* LF_HASH_UNIQUE, etc */
  int32 volatile size;                  /* size of array */
  int32 volatile count;                 /* number of elements in the hash */
} LF_HASH;

void lf_hash_init(LF_HASH *hash, uint element_size, uint flags,
                  uint key_offset, uint key_length, my_hash_get_key get_key,
                  CHARSET_INFO *charset);
void lf_hash_destroy(LF_HASH *hash);
int lf_hash_insert(LF_HASH *hash, LF_PINS *pins, const void *data);
void *lf_hash_search(LF_HASH *hash, LF_PINS *pins, const void *key, uint keylen);
int lf_hash_delete(LF_HASH *hash, LF_PINS *pins, const void *key, uint keylen);
/*
  shortcut macros to access underlying pinbox functions from an LF_HASH
  see _lf_pinbox_get_pins() and _lf_pinbox_put_pins()
*/
#define _lf_hash_get_pins(HASH)     _lf_alloc_get_pins(&(HASH)->alloc)
#define lf_hash_get_pins(HASH)       lf_alloc_get_pins(&(HASH)->alloc)
#define _lf_hash_put_pins(PINS)     _lf_pinbox_put_pins(PINS)
#define lf_hash_put_pins(PINS)       lf_pinbox_put_pins(PINS)
#define lf_hash_search_unpin(PINS)   lf_unpin((PINS), 2)
/*
  cleanup
*/

#undef lock_wrap_void
#undef lock_wrap
#undef nolock_wrap_void
#undef nolock_wrap

C_MODE_END

#endif

