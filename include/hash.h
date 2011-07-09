/*
   Copyright (c) 2000-2008 MySQL AB, 2008, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/* Dynamic hashing of record with different key-length */

#ifndef _hash_h
#define _hash_h
#ifdef	__cplusplus
extern "C" {
#endif

/*
  There was a problem on MacOSX with a shared object ha_example.so.
  It used hash_search(). During build of ha_example.so no libmysys
  was specified. Since MacOSX had a hash_search() in the system
  library, it built the shared object so that the dynamic linker
  linked hash_search() to the system library, which caused a crash
  when called. To come around this, we renamed hash_search() to
  my_hash_search(), as we did long ago with hash_insert() and
  hash_reset(). However, this time we made the move complete with
  all names. To keep compatibility, we redefine the old names.
  Since every C and C++ file, that uses HASH, needs to include
  this file, the change is complete. Both names could be used
  in the code, but the my_* versions are recommended now.
*/
#define hash_get_key    my_hash_get_key
#define hash_free_key   my_hash_free_key
#define hash_init       my_hash_init
#define hash_init2      my_hash_init2
#define _hash_init      _my_hash_init
#define hash_free       my_hash_free
#define hash_reset      my_hash_reset
#define hash_element    my_hash_element
#define hash_search     my_hash_search
#define hash_first      my_hash_first
#define hash_next       my_hash_next
#define hash_insert     my_hash_insert
#define hash_delete     my_hash_delete
#define hash_update     my_hash_update
#define hash_replace    my_hash_replace
#define hash_check      my_hash_check
#define hash_clear      my_hash_clear
#define hash_inited     my_hash_inited
#define hash_init_opt   my_hash_init_opt

/*
  Overhead to store an element in hash
  Can be used to approximate memory consumption for a hash
 */
#define HASH_OVERHEAD (sizeof(char*)*2)

/* flags for hash_init */
#define HASH_UNIQUE     1       /* hash_insert fails on duplicate key */

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
          _my_hash_init(A,0,B,C,D,E,F,G,H CALLER_INFO)
#define my_hash_init2(A,B,C,D,E,F,G,H,I) \
          _my_hash_init(A,B,C,D,E,F,G,H,I CALLER_INFO)
my_bool _my_hash_init(HASH *hash, uint growth_size, CHARSET_INFO *charset,
                      ulong default_array_elements, size_t key_offset,
                      size_t key_length, my_hash_get_key get_key,
                      void (*free_element)(void*),
                      uint flags CALLER_INFO_PROTO);
void my_hash_free(HASH *tree);
void my_hash_reset(HASH *hash);
uchar *my_hash_element(HASH *hash, ulong idx);
uchar *my_hash_search(const HASH *info, const uchar *key, size_t length);
uchar *my_hash_first(const HASH *info, const uchar *key, size_t length,
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
          (!my_hash_inited(A) && _my_hash_init(A,0,B,C,D,E,F,G, H CALLER_INFO))

#ifdef	__cplusplus
}
#endif
#endif
