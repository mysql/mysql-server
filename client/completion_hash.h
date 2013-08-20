/* Copyright (c) 2000-2002, 2006 MySQL AB
   Use is subject to license terms

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; version 2
   of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
   MA 02110-1301, USA */

#ifndef _HASH_
#define _HASH_

#define SUCCESS 0
#define FAILURE 1

#include <sys/types.h>
#include <my_sys.h>

typedef struct _entry {
	char *str;
	struct _entry *pNext;
} entry;

typedef struct bucket
{
  uint h;					/* Used for numeric indexing */
  char *arKey;
  uint nKeyLength;
  uint count;
  entry *pData;
  struct bucket *pNext;
} Bucket;

typedef struct hashtable {
  uint nTableSize;
  uint initialized;
  MEM_ROOT mem_root;
  uint(*pHashFunction) (const char *arKey, uint nKeyLength);
  Bucket **arBuckets;
} HashTable;

extern int completion_hash_init(HashTable *ht, uint nSize);
extern int completion_hash_update(HashTable *ht, char *arKey, uint nKeyLength, char *str);
extern int hash_exists(HashTable *ht, char *arKey);
extern Bucket *find_all_matches(HashTable *ht, const char *str, uint length, uint *res_length);
extern Bucket *find_longest_match(HashTable *ht, char *str, uint length, uint *res_length);
extern void add_word(HashTable *ht,char *str);
extern void completion_hash_clean(HashTable *ht);
extern int completion_hash_exists(HashTable *ht, char *arKey, uint nKeyLength);
extern void completion_hash_free(HashTable *ht);

#endif /* _HASH_ */
