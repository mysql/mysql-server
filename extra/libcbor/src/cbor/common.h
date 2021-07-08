/*
 * Copyright (c) 2014-2020 Pavel Kalvoda <me@pavelkalvoda.com>
 *
 * libcbor is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef LIBCBOR_COMMON_H
#define LIBCBOR_COMMON_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "cbor/cbor_export.h"
#include "cbor/configuration.h"
#include "data.h"

#ifdef __cplusplus
extern "C" {

/**
 * C++ is not a subset of C99 -- 'restrict' qualifier is not a part of the
 * language. This is a workaround to keep it in C headers -- compilers allow
 * linking non-restrict signatures with restrict implementations.
 *
 * If you know a nicer way, please do let me know.
 */
#define CBOR_RESTRICT_POINTER

#else

// MSVC + C++ workaround
#define CBOR_RESTRICT_POINTER CBOR_RESTRICT_SPECIFIER

#endif

static const uint8_t cbor_major_version = CBOR_MAJOR_VERSION;
static const uint8_t cbor_minor_version = CBOR_MINOR_VERSION;
static const uint8_t cbor_patch_version = CBOR_PATCH_VERSION;

#define CBOR_VERSION         \
  TO_STR(CBOR_MAJOR_VERSION) \
  "." TO_STR(CBOR_MINOR_VERSION) "." TO_STR(CBOR_PATCH_VERSION)
#define CBOR_HEX_VERSION \
  ((CBOR_MAJOR_VERSION << 16) | (CBOR_MINOR_VERSION << 8) | CBOR_PATCH_VERSION)

/* http://stackoverflow.com/questions/1644868/c-define-macro-for-debug-printing
 */
#ifdef DEBUG
#include <stdio.h>
#define debug_print(fmt, ...)                                           \
  do {                                                                  \
    if (DEBUG)                                                          \
      fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, \
              __VA_ARGS__);                                             \
  } while (0)
#else
#define debug_print(fmt, ...) \
  do {                        \
  } while (0)
#endif

#define TO_STR_(x) #x
#define TO_STR(x) TO_STR_(x) /* enables proper double expansion */

// Macro to short-circuit builder functions when memory allocation fails
#define _CBOR_NOTNULL(cbor_item) \
  do {                           \
    if (cbor_item == NULL) {     \
      return NULL;               \
    }                            \
  } while (0)

// Macro to short-circuit builders when memory allocation of nested data fails
#define _CBOR_DEPENDENT_NOTNULL(cbor_item, pointer) \
  do {                                              \
    if (pointer == NULL) {                          \
      _CBOR_FREE(cbor_item);                        \
      return NULL;                                  \
    }                                               \
  } while (0)

#if CBOR_CUSTOM_ALLOC

typedef void *(*_cbor_malloc_t)(size_t);
typedef void *(*_cbor_realloc_t)(void *, size_t);
typedef void (*_cbor_free_t)(void *);

CBOR_EXPORT extern _cbor_malloc_t _cbor_malloc;
CBOR_EXPORT extern _cbor_realloc_t _cbor_realloc;
CBOR_EXPORT extern _cbor_free_t _cbor_free;

/** Sets the memory management routines to use.
 *
 * Only available when `CBOR_CUSTOM_ALLOC` is truthy
 *
 * \rst
 * .. warning:: This function modifies the global state and should therefore be
 * used accordingly. Changing the memory handlers while allocated items exist
 * will result in a ``free``/``malloc`` mismatch. This function is not thread
 * safe with respect to both itself and all the other *libcbor* functions that
 * work with the heap.
 * .. note:: `realloc` implementation must correctly support `NULL` reallocation
 * (see e.g. http://en.cppreference.com/w/c/memory/realloc) \endrst
 *
 * @param custom_malloc malloc implementation
 * @param custom_realloc realloc implementation
 * @param custom_free free implementation
 */
CBOR_EXPORT void cbor_set_allocs(_cbor_malloc_t custom_malloc,
                                 _cbor_realloc_t custom_realloc,
                                 _cbor_free_t custom_free);

#define _CBOR_MALLOC _cbor_malloc
#define _CBOR_REALLOC _cbor_realloc
#define _CBOR_FREE _cbor_free

#else

#define _CBOR_MALLOC malloc
#define _CBOR_REALLOC realloc
#define _CBOR_FREE free

#endif

/*
 * ============================================================================
 * Type manipulation
 * ============================================================================
 */

/** Get the type of the item
 *
 * @param item[borrow]
 * @return The type
 */
CBOR_EXPORT cbor_type cbor_typeof(
    const cbor_item_t *item); /* Will be inlined iff link-time opt is enabled */

/* Standard item types as described by the RFC */

/** Does the item have the appropriate major type?
 * @param item[borrow] the item
 * @return Is the item an #CBOR_TYPE_UINT?
 */
CBOR_EXPORT bool cbor_isa_uint(const cbor_item_t *item);

/** Does the item have the appropriate major type?
 * @param item[borrow] the item
 * @return Is the item a #CBOR_TYPE_NEGINT?
 */
CBOR_EXPORT bool cbor_isa_negint(const cbor_item_t *item);

/** Does the item have the appropriate major type?
 * @param item[borrow] the item
 * @return Is the item a #CBOR_TYPE_BYTESTRING?
 */
CBOR_EXPORT bool cbor_isa_bytestring(const cbor_item_t *item);

/** Does the item have the appropriate major type?
 * @param item[borrow] the item
 * @return Is the item a #CBOR_TYPE_STRING?
 */
CBOR_EXPORT bool cbor_isa_string(const cbor_item_t *item);

/** Does the item have the appropriate major type?
 * @param item[borrow] the item
 * @return Is the item an #CBOR_TYPE_ARRAY?
 */
CBOR_EXPORT bool cbor_isa_array(const cbor_item_t *item);

/** Does the item have the appropriate major type?
 * @param item[borrow] the item
 * @return Is the item a #CBOR_TYPE_MAP?
 */
CBOR_EXPORT bool cbor_isa_map(const cbor_item_t *item);

/** Does the item have the appropriate major type?
 * @param item[borrow] the item
 * @return Is the item a #CBOR_TYPE_TAG?
 */
CBOR_EXPORT bool cbor_isa_tag(const cbor_item_t *item);

/** Does the item have the appropriate major type?
 * @param item[borrow] the item
 * @return Is the item a #CBOR_TYPE_FLOAT_CTRL?
 */
CBOR_EXPORT bool cbor_isa_float_ctrl(const cbor_item_t *item);

/* Practical types with respect to their semantics (but not tag values) */

/** Is the item an integer, either positive or negative?
 * @param item[borrow] the item
 * @return  Is the item an integer, either positive or negative?
 */
CBOR_EXPORT bool cbor_is_int(const cbor_item_t *item);

/** Is the item an a floating point number?
 * @param item[borrow] the item
 * @return  Is the item a floating point number?
 */
CBOR_EXPORT bool cbor_is_float(const cbor_item_t *item);

/** Is the item an a boolean?
 * @param item[borrow] the item
 * @return  Is the item a boolean?
 */
CBOR_EXPORT bool cbor_is_bool(const cbor_item_t *item);

/** Does this item represent `null`
 * \rst
 * .. warning:: This is in no way related to the value of the pointer. Passing a
 * null pointer will most likely result in a crash. \endrst
 * @param item[borrow] the item
 * @return  Is the item (CBOR logical) null?
 */
CBOR_EXPORT bool cbor_is_null(const cbor_item_t *item);

/** Does this item represent `undefined`
 * \rst
 * .. warning:: Care must be taken to distinguish nulls and undefined values in
 * C. \endrst
 * @param item[borrow] the item
 * @return Is the item (CBOR logical) undefined?
 */
CBOR_EXPORT bool cbor_is_undef(const cbor_item_t *item);

/*
 * ============================================================================
 * Memory management
 * ============================================================================
 */

/** Increases the reference count by one
 *
 * No dependent items are affected.
 *
 * @param item[incref] item the item
 * @return the input reference
 */
CBOR_EXPORT cbor_item_t *cbor_incref(cbor_item_t *item);

/** Decreases the reference count by one, deallocating the item if needed
 *
 * In case the item is deallocated, the reference count of any dependent items
 * is adjusted accordingly in a recursive manner.
 *
 * @param item[take] the item. Set to `NULL` if deallocated
 */
CBOR_EXPORT void cbor_decref(cbor_item_t **item);

/** Decreases the reference count by one, deallocating the item if needed
 *
 * Convenience wrapper for #cbor_decref when its set-to-null behavior is not
 * needed
 *
 * @param item[take] the item
 */
CBOR_EXPORT void cbor_intermediate_decref(cbor_item_t *item);

/** Get the reference count
 *
 * \rst
 * .. warning:: This does *not* account for transitive references.
 * \endrst
 *
 * @param item[borrow] the item
 * @return the reference count
 */
CBOR_EXPORT size_t cbor_refcount(const cbor_item_t *item);

/** Provides CPP-like move construct
 *
 * Decreases the reference count by one, but does not deallocate the item even
 * if its refcount reaches zero. This is useful for passing intermediate values
 * to functions that increase reference count. Should only be used with
 * functions that `incref` their arguments.
 *
 * \rst
 * .. warning:: If the item is moved without correctly increasing the reference
 * count afterwards, the memory will be leaked. \endrst
 *
 * @param item[take] the item
 * @return the item with reference count decreased by one
 */
CBOR_EXPORT cbor_item_t *cbor_move(cbor_item_t *item);

#ifdef __cplusplus
}
#endif

#endif  // LIBCBOR_COMMON_H
