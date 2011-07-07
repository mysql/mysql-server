/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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

/* Dynamic hashing of record with different key-length */

#ifndef _hash_h
#define _hash_h

#include "my_global.h"                          /* uchar */
#include "my_sys.h"                             /* DYNAMIC_ARRAY */

/*
  This forward declaration is used from C files where the real
  definition is included before.  Since C does not allow repeated
  typedef declarations, even when identical, the definition may not be
  repeated.
*/
#ifndef CHARSET_INFO_DEFINED
#define CHARSET_INFO_DEFINED
typedef struct charset_info_st CHARSET_INFO;
#endif  /* CHARSET_INFO_DEFINED */

#ifdef	__cplusplus
extern "C" {
#endif

/*
  Overhead to store an element in hash
  Can be used to approximate memory consumption for a hash
 */
#define HASH_OVERHEAD (sizeof(char*)*2)

/* flags for hash_init */
#define HASH_UNIQUE     1       /* hash_insert fails on duplicate key */

typedef uint my_hash_value_type;
typedef uchar *(*my_hash_get_key)(const uchar *,size_t*,my_bool);
typedef void (*my_hash_free_key)(void *);

typedef struct st_hash {
  size_t key_offset,key_length;		/* Length of key if const length */
  size_t blength;
  ulong records;
  uint flags;
  DYNAMIC_ARRAY array;				/* Place for hash_keys */
  my_hash_get_key get_key;
  void (*free)(void *);
  CHARSET_INFO *charset;
} HASH;

/* A search iterator state */
typedef uint HASH_SEARCH_STATE;

#define my_hash_init(A,B,C,D,E,F,G,H) \
          _my_hash_init(A,0,B,C,D,E,F,G,H)
#define my_hash_init2(A,B,C,D,E,F,G,H,I) \
          _my_hash_init(A,B,C,D,E,F,G,H,I)
my_bool _my_hash_init(HASH *hash, uint growth_size, CHARSET_INFO *charset,
                      ulong default_array_elements, size_t key_offset,
                      size_t key_length, my_hash_get_key get_key,
                      void (*free_element)(void*),
                      uint flags);
void my_hash_free(HASH *tree);
void my_hash_reset(HASH *hash);
uchar *my_hash_element(HASH *hash, ulong idx);
uchar *my_hash_search(const HASH *info, const uchar *key, size_t length);
uchar *my_hash_search_using_hash_value(const HASH *info,
                                       my_hash_value_type hash_value,
                                       const uchar *key, size_t length);
my_hash_value_type my_calc_hash(const HASH *info,
                                const uchar *key, size_t length);
uchar *my_hash_first(const HASH *info, const uchar *key, size_t length,
                     HASH_SEARCH_STATE *state);
uchar *my_hash_first_from_hash_value(const HASH *info,
                                     my_hash_value_type hash_value,
                                     const uchar *key,
                                     size_t length,
                                     HASH_SEARCH_STATE *state);
uchar *my_hash_next(const HASH *info, const uchar *key, size_t length,
                    HASH_SEARCH_STATE *state);
my_bool my_hash_insert(HASH *info, const uchar *data);
my_bool my_hash_delete(HASH *hash, uchar *record);
my_bool my_hash_update(HASH *hash, uchar *record, uchar *old_key,
                       size_t old_key_length);
void my_hash_replace(HASH *hash, HASH_SEARCH_STATE *state, uchar *new_row);
my_bool my_hash_check(HASH *hash); /* Only in debug library */

#define my_hash_clear(H) bzero((char*) (H), sizeof(*(H)))
#define my_hash_inited(H) ((H)->blength != 0)
#define my_hash_init_opt(A,B,C,D,E,F,G,H) \
          (!my_hash_inited(A) && _my_hash_init(A,0,B,C,D,E,F,G,H))

#ifdef	__cplusplus
}
#endif
#endif
