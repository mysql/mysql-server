/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* Dynamic hashing of record with different key-length */

#ifndef _hash_h
#define _hash_h
#ifdef	__cplusplus
extern "C" {
#endif

/*
  Overhead to store an element in hash
  Can be used to approximate memory consumption for a hash
 */
#define HASH_OVERHEAD (sizeof(char*)*2)

typedef byte *(*hash_get_key)(const byte *,uint*,my_bool);
typedef void (*hash_free_key)(void *);

typedef struct st_hash {
  uint key_offset,key_length;		/* Length of key if const length */
  uint records,blength,current_record;
  uint flags;
  DYNAMIC_ARRAY array;				/* Place for hash_keys */
  hash_get_key get_key;
  void (*free)(void *);
  CHARSET_INFO *charset;
} HASH;

#define hash_init(A,B,C,D,E,F,G,H) _hash_init(A,B,C,D,E,F,G, H CALLER_INFO)
my_bool _hash_init(HASH *hash, CHARSET_INFO *charset,
		   uint default_array_elements, uint key_offset,
		   uint key_length, hash_get_key get_key,
		   void (*free_element)(void*), uint flags CALLER_INFO_PROTO);
void hash_free(HASH *tree);
void hash_reset(HASH *hash);
byte *hash_element(HASH *hash,uint idx);
gptr hash_search(HASH *info,const byte *key,uint length);
gptr hash_next(HASH *info,const byte *key,uint length);
my_bool my_hash_insert(HASH *info,const byte *data);
my_bool hash_delete(HASH *hash,byte *record);
my_bool hash_update(HASH *hash,byte *record,byte *old_key,uint old_key_length);
void hash_replace(HASH *hash, uint idx, byte *new_row);
my_bool hash_check(HASH *hash);			/* Only in debug library */

#define hash_clear(H) bzero((char*) (H),sizeof(*(H)))
#define hash_inited(H) ((H)->array.buffer != 0)

#ifdef	__cplusplus
}
#endif
#endif
