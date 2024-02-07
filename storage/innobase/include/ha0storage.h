/*****************************************************************************

Copyright (c) 2007, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ha0storage.h
 Hash storage.
 Provides a data structure that stores chunks of data in
 its own storage, avoiding duplicates.

 Created September 22, 2007 Vasil Dimov
 *******************************************************/

#ifndef ha0storage_h
#define ha0storage_h

#include "univ.i"

/** This value is used by default by ha_storage_create(). More memory
is allocated later when/if it is needed. */
constexpr uint32_t HA_STORAGE_DEFAULT_HEAP_BYTES = 1024;

/** This value is used by default by ha_storage_create(). It is a
constant per ha_storage's lifetime. */
constexpr uint32_t HA_STORAGE_DEFAULT_HASH_CELLS = 4096;

/** Hash storage */
struct ha_storage_t;

/** Creates a hash storage. If any of the parameters is 0, then a default value
is used.
@param[in]      initial_hash_cells      initial number of cells in the hash
                                        table
@param[in]      initial_heap_bytes      initial heap's size
@return own: hash storage */
static inline ha_storage_t *ha_storage_create(ulint initial_heap_bytes,
                                              ulint initial_hash_cells);

/** Copies data into the storage and returns a pointer to the copy. If the
 same data chunk is already present, then pointer to it is returned.
 Data chunks are considered to be equal if len1 == len2 and
 memcmp(data1, data2, len1) == 0. If "data" is not present (and thus
 data_len bytes need to be allocated) and the size of storage is going to
 become more than "memlim" then "data" is not added and NULL is returned.
 To disable this behavior "memlim" can be set to 0, which stands for
 "no limit".
 @return pointer to the copy */
const void *ha_storage_put_memlim(
    ha_storage_t *storage, /*!< in/out: hash storage */
    const void *data,      /*!< in: data to store */
    ulint data_len,        /*!< in: data length */
    ulint memlim);         /*!< in: memory limit to obey */

/** Same as ha_storage_put_memlim() but without memory limit.
 @param storage in/out: hash storage
 @param data in: data to store
 @param data_len in: data length
 @return pointer to the copy of the string */
static inline const void *ha_storage_put(ha_storage_t *storage,
                                         const void *data, ulint data_len) {
  return ha_storage_put_memlim(storage, data, data_len, 0);
}

/** Copies string into the storage and returns a pointer to the copy. If the
 same string is already present, then pointer to it is returned.
 Strings are considered to be equal if strcmp(str1, str2) == 0.
 @param storage in/out: hash storage
 @param str in: string to put
 @return pointer to the copy of the string */
static inline const char *ha_storage_put_str(ha_storage_t *storage,
                                             const char *str) {
  return (const char *)ha_storage_put(storage, str, strlen(str) + 1);
}

/** Copies string into the storage and returns a pointer to the copy obeying
 a memory limit.
 If the same string is already present, then pointer to it is returned.
 Strings are considered to be equal if strcmp(str1, str2) == 0.
 @param storage in/out: hash storage
 @param str in: string to put
 @param memlim in: memory limit to obey
 @return pointer to the copy of the string */
static inline const char *ha_storage_put_str_memlim(ha_storage_t *storage,
                                                    const char *str,
                                                    ulint memlim) {
  return (const char *)ha_storage_put_memlim(storage, str, strlen(str) + 1,
                                             memlim);
}

/** Empties a hash storage, freeing memory occupied by data chunks.
 This invalidates any pointers previously returned by ha_storage_put().
 The hash storage is not invalidated itself and can be used again. */
static inline void ha_storage_empty(
    ha_storage_t **storage); /*!< in/out: hash storage */

/** Frees a hash storage and everything it contains, it cannot be used after
 this call.
 This invalidates any pointers previously returned by ha_storage_put(). */
static inline void ha_storage_free(
    ha_storage_t *storage); /*!< in, own: hash storage */

/** Gets the size of the memory used by a storage.
 @return bytes used */
static inline ulint ha_storage_get_size(
    const ha_storage_t *storage); /*!< in: hash storage */

#include "ha0storage.ic"

#endif /* ha0storage_h */
