/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

/* Quick & light hash implementation for tab completion purposes
 *
 * by  Andi Gutmans <andi@zend.com>
 * and Zeev Suraski <zeev@zend.com>
 * Small portability changes by Monty. Changed also to use my_malloc/my_free
 */

#include <my_global.h>
#include <m_string.h>
#undef SAFEMALLOC				// Speed things up
#include <my_sys.h>
#include "completion_hash.h"

uint hashpjw(char *arKey, uint nKeyLength)
{
  uint h = 0, g, i;

  for (i = 0; i < nKeyLength; i++) {
    h = (h << 4) + arKey[i];
    if ((g = (h & 0xF0000000))) {
      h = h ^ (g >> 24);
      h = h ^ g;
    }
  }
  return h;
}

int completion_hash_init(HashTable *ht, uint nSize)
{
  ht->arBuckets = (Bucket **) my_malloc(nSize* sizeof(Bucket *),
					MYF(MY_ZEROFILL | MY_WME));

  if (!ht->arBuckets) {
    ht->initialized = 0;
    return FAILURE;
  }
  ht->pHashFunction = hashpjw;
  ht->nTableSize = nSize;
  ht->initialized = 1;
  return SUCCESS;
}


int completion_hash_update(HashTable *ht, char *arKey, uint nKeyLength,
			   char *str)
{
  uint h, nIndex;

  Bucket *p;

  h = ht->pHashFunction(arKey, nKeyLength);
  nIndex = h % ht->nTableSize;

  if (nKeyLength <= 0) {
    return FAILURE;
  }
  p = ht->arBuckets[nIndex];
  while (p)
  {
    if ((p->h == h) && (p->nKeyLength == nKeyLength)) {
      if (!memcmp(p->arKey, arKey, nKeyLength)) {
	entry *n;

	n = (entry *) my_malloc(sizeof(entry),
				MYF(MY_WME));
	n->pNext = p->pData;
	n->str = str;
	p->pData = n;
	p->count++;

	return SUCCESS;
      }
    }
    p = p->pNext;
  }

  p = (Bucket *) my_malloc(sizeof(Bucket),MYF(MY_WME));

  if (!p) {
    return FAILURE;
  }
  p->arKey = arKey;
  p->nKeyLength = nKeyLength;
  p->h = h;

  p->pData = (entry*) my_malloc(sizeof(entry),MYF(MY_WME));
  if (!p->pData) {
    my_free((gptr) p,MYF(0));
    return FAILURE;
  }
  p->pData->str = str;
  p->pData->pNext = 0;
  p->count = 1;

  p->pNext = ht->arBuckets[nIndex];
  ht->arBuckets[nIndex] = p;

  return SUCCESS;
}

static Bucket *completion_hash_find(HashTable *ht, char *arKey,
				    uint nKeyLength)
{
  uint h, nIndex;
  Bucket *p;

  h = ht->pHashFunction(arKey, nKeyLength);
  nIndex = h % ht->nTableSize;

  p = ht->arBuckets[nIndex];
  while (p)
  {
    if ((p->h == h) && (p->nKeyLength == nKeyLength)) {
      if (!memcmp(p->arKey, arKey, nKeyLength)) {
	return p;
      }
    }
    p = p->pNext;
  }
  return (Bucket*) 0;
}


int completion_hash_exists(HashTable *ht, char *arKey, uint nKeyLength)
{
  uint h, nIndex;
  Bucket *p;

  h = ht->pHashFunction(arKey, nKeyLength);
  nIndex = h % ht->nTableSize;

  p = ht->arBuckets[nIndex];
  while (p)
  {
    if ((p->h == h) && (p->nKeyLength == nKeyLength))
    {
      if (!strcmp(p->arKey, arKey)) {
	return 1;
      }
    }
    p = p->pNext;
  }
  return 0;
}

Bucket *find_all_matches(HashTable *ht, char *str, uint length,
			 uint *res_length)
{
  Bucket *b;

  b = completion_hash_find(ht,str,length);
  if (!b) {
    *res_length = 0;
    return (Bucket*) 0;
  } else {
    *res_length = length;
    return b;
  }
}

Bucket *find_longest_match(HashTable *ht, char *str, uint length,
			   uint *res_length)
{
  Bucket *b,*return_b;
  char *s;
  uint count;
  uint lm;

  b = completion_hash_find(ht,str,length);
  if (!b) {
    *res_length = 0;
    return (Bucket*) 0;
  }

  count = b->count;
  lm = length;
  s = b->pData->str;

  return_b = b;
  while (s[lm]!=0 && (b=completion_hash_find(ht,s,lm+1))) {
    if (b->count<count) {
      *res_length=lm;
      return return_b;
    }
    return_b=b;
    lm++;
  }
  *res_length=lm;
  return return_b;
}


void completion_hash_clean(HashTable *ht)
{
  uint i;
  entry *e, *t;
  Bucket *b, *tmp;

  for (i=0; i<ht->nTableSize; i++) {
    b = ht->arBuckets[i];
    while (b) {
      e =  b->pData;
      while (e) {
	t = e;
	e = e->pNext;
	my_free((gptr) t,MYF(0));
      }
      tmp = b;
      b = b->pNext;
      my_free((gptr) tmp,MYF(0));
    }
  }
  bzero((char*) ht->arBuckets,ht->nTableSize*sizeof(Bucket *));
}


void completion_hash_free(HashTable *ht)
{
  completion_hash_clean(ht);
  my_free((gptr) ht->arBuckets,MYF(0));
}


void add_word(HashTable *ht,char *str)
{
  int i;
  int length= (int) strlen(str);

  for (i=1; i<=length; i++) {
    completion_hash_update(ht, str, i, str);
  }
}
