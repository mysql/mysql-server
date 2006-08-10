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

/*
  extensible hash

  TODO
     dummy nodes use element_size=0
     try to get rid of dummy nodes ?
*/
#include <my_global.h>
#include <my_sys.h>
#include <my_bit.h>
#include <lf.h>

LF_REQUIRE_PINS(3);

typedef struct {
  intptr volatile link;
  uint32 hashnr;
  const uchar *key;
  uint keylen;
} LF_SLIST;

typedef struct {
  intptr volatile *prev;
  LF_SLIST *curr, *next;
} CURSOR;

#define PTR(V)      (LF_SLIST *)((V) & (~(intptr)1))
#define DELETED(V)  ((V) & 1)

/*
  RETURN
    0 - not found
    1 - found

  NOTE
    cursor is positioned in either case
    pins[0..2] are used, they are NOT removed on return
*/
static int lfind(intptr volatile *head, uint32 hashnr,
                 const uchar *key, uint keylen, CURSOR *cursor, LF_PINS *pins)
{
  uint32       cur_hashnr;
  const uchar *cur_key;
  uint         cur_keylen;
  intptr       link;

retry:
  cursor->prev=head;
  do {
    cursor->curr=PTR(*cursor->prev);
    _lf_pin(pins,1,cursor->curr);
  } while(*cursor->prev != (intptr)cursor->curr && LF_BACKOFF);
  for (;;)
  {
    if (!cursor->curr)
      return 0;
    do { // XXX or goto retry ?
      link=cursor->curr->link;
      cursor->next=PTR(link);
      _lf_pin(pins, 0, cursor->next);
    } while(link != cursor->curr->link && LF_BACKOFF);
    cur_hashnr=cursor->curr->hashnr;
    cur_key=cursor->curr->key;
    cur_keylen=cursor->curr->keylen;
    if (*cursor->prev != (intptr)cursor->curr)
    {
      LF_BACKOFF;
      goto retry;
    }
    if (!DELETED(link))
    {
      if (cur_hashnr >= hashnr)
      {
        int r=1;
        if (cur_hashnr > hashnr || (r=memcmp(cur_key, key, keylen)) >= 0)
          return !r;
      }
      cursor->prev=&(cursor->curr->link);
      _lf_pin(pins, 2, cursor->curr);
    }
    else
    {
      if (my_atomic_casptr((void **)cursor->prev,
                           (void **)&cursor->curr, cursor->next))
        _lf_alloc_free(pins, cursor->curr);
      else
      {
        LF_BACKOFF;
        goto retry;
      }
    }
    cursor->curr=cursor->next;
    _lf_pin(pins, 1, cursor->curr);
  }
}

/*
  RETURN
    0     - inserted
    not 0 - a pointer to a conflict

  NOTE
    it uses pins[0..2], on return all pins are removed.
*/
static LF_SLIST *linsert(LF_SLIST * volatile *head, LF_SLIST *node,
                         LF_PINS *pins, uint flags)
{
  CURSOR         cursor;
  int            res=-1;

  do
  {
    if (lfind((intptr*)head, node->hashnr, node->key, node->keylen,
              &cursor, pins) &&
        (flags & LF_HASH_UNIQUE))
      res=0;
    else
    {
      node->link=(intptr)cursor.curr;
      assert(node->link != (intptr)node);
      assert(cursor.prev != &node->link);
      if (my_atomic_casptr((void **)cursor.prev, (void **)&cursor.curr, node))
        res=1;
    }
  } while (res == -1);
  _lf_unpin(pins, 0);
  _lf_unpin(pins, 1);
  _lf_unpin(pins, 2);
  return res ? 0 : cursor.curr;
}

/*
  RETURN
    0 - ok
    1 - not found
  NOTE
    it uses pins[0..2], on return all pins are removed.
*/
static int ldelete(LF_SLIST * volatile *head, uint32 hashnr,
                   const uchar *key, uint keylen, LF_PINS *pins)
{
  CURSOR cursor;
  int res=-1;

  do
  {
    if (!lfind((intptr *)head, hashnr, key, keylen, &cursor, pins))
      res= 1;
    else
      if (my_atomic_casptr((void **)&(cursor.curr->link),
                           (void **)&cursor.next, 1+(char *)cursor.next))
      {
        if (my_atomic_casptr((void **)cursor.prev,
                             (void **)&cursor.curr, cursor.next))
          _lf_alloc_free(pins, cursor.curr);
        else
          lfind((intptr *)head, hashnr, key, keylen, &cursor, pins);
        res= 0;
      }
  } while (res == -1);
  _lf_unpin(pins, 0);
  _lf_unpin(pins, 1);
  _lf_unpin(pins, 2);
  return res;
}

/*
  RETURN
    0 - not found
    node - found
  NOTE
    it uses pins[0..2], on return the pin[2] keeps the node found
    all other pins are removed.
*/
static LF_SLIST *lsearch(LF_SLIST * volatile *head, uint32 hashnr,
                         const uchar *key, uint keylen, LF_PINS *pins)
{
  CURSOR cursor;
  int res=lfind((intptr *)head, hashnr, key, keylen, &cursor, pins);
  if (res) _lf_pin(pins, 2, cursor.curr);
  _lf_unpin(pins, 0);
  _lf_unpin(pins, 1);
  return res ? cursor.curr : 0;
}

static inline const uchar* hash_key(const LF_HASH *hash,
                              const uchar *record, uint *length)
{
  if (hash->get_key)
    return (*hash->get_key)(record,length,0);
  *length=hash->key_length;
  return record + hash->key_offset;
}

static inline uint calc_hash(LF_HASH *hash, const uchar *key, uint keylen)
{
  ulong nr1=1, nr2=4;
  hash->charset->coll->hash_sort(hash->charset,key,keylen,&nr1,&nr2);
  return nr1 & INT_MAX32;
}

#define MAX_LOAD 1
static void initialize_bucket(LF_HASH *, LF_SLIST * volatile*, uint, LF_PINS *);

void lf_hash_init(LF_HASH *hash, uint element_size, uint flags,
                  uint key_offset, uint key_length, hash_get_key get_key,
                  CHARSET_INFO *charset)
{
  lf_alloc_init(&hash->alloc,sizeof(LF_SLIST)+element_size);
  lf_dynarray_init(&hash->array, sizeof(LF_SLIST **));
  hash->size=1;
  hash->count=0;
  hash->element_size=element_size;
  hash->flags=flags;
  hash->charset=charset;
  hash->key_offset=key_offset;
  hash->key_length=key_length;
  hash->get_key=get_key;
  DBUG_ASSERT(get_key ? !key_offset && !key_length : key_length);
}

void lf_hash_end(LF_HASH *hash)
{
  LF_SLIST *el=*(LF_SLIST **)_lf_dynarray_lvalue(&hash->array, 0);
  while (el)
  {
    intptr next=el->link;
    lf_alloc_real_free(&hash->alloc, el);
    el=(LF_SLIST *)next;
  }
  lf_alloc_end(&hash->alloc);
  lf_dynarray_end(&hash->array);
}

/*
  NOTE
   see linsert() for pin usage
*/
int lf_hash_insert(LF_HASH *hash, LF_PINS *pins, const void *data)
{
  uint csize, bucket, hashnr, keylen;
  LF_SLIST *node, * volatile *el;
  const uchar *key;

  key= hash_key(hash, data, &keylen);
  hashnr= calc_hash(hash, key, keylen);
  bucket= hashnr % hash->size;
  lf_lock_by_pins(pins);
  node=(LF_SLIST *)_lf_alloc_new(pins);
  memcpy(node+1, data, hash->element_size);
  el=_lf_dynarray_lvalue(&hash->array, bucket);
  if (*el == NULL)
    initialize_bucket(hash, el, bucket, pins);
  node->hashnr=my_reverse_bits(hashnr) | 1;
  node->key=((char *)(node+1))+(key-(uchar *)data);
  node->keylen=keylen;
  if (linsert(el, node, pins, hash->flags))
  {
    _lf_alloc_free(pins, node);
    lf_unlock_by_pins(pins);
    return 0;
  }
  csize= hash->size;
  if ((my_atomic_add32(&hash->count, 1)+1.0) / csize > MAX_LOAD)
    my_atomic_cas32(&hash->size, &csize, csize*2);
#if 0
  node=*(LF_SLIST **)_lf_dynarray_lvalue(&hash->array, 0);
  hashnr=0;
  while (node)
  {
    assert (node->hashnr >= hashnr);
    hashnr=node->hashnr;
    node=(LF_SLIST *)node->link;
  }
#endif
  lf_unlock_by_pins(pins);
  return 1;
}

/*
  NOTE
   see ldelete() for pin usage
*/
int lf_hash_delete(LF_HASH *hash, LF_PINS *pins, const uchar *key, uint keylen)
{
  LF_SLIST * volatile *el;
  uint bucket, hashnr=calc_hash(hash, key, keylen);

  bucket= hashnr % hash->size;
  lf_lock_by_pins(pins);
  el=_lf_dynarray_lvalue(&hash->array, bucket);
  if (*el == NULL)
    initialize_bucket(hash, el, bucket, pins);
  if (ldelete(el, my_reverse_bits(hashnr) | 1, key, keylen, pins))
  {
    lf_unlock_by_pins(pins);
    return 0;
  }
  my_atomic_add32(&hash->count, -1);
#if 0
  {
  LF_SLIST *node=*(LF_SLIST **)_lf_dynarray_lvalue(&hash->array, 0);
  hashnr=0;
  while (node)
  {
    assert (node->hashnr >= hashnr);
    hashnr=node->hashnr;
    node=(LF_SLIST *)node->link;
  }
  }
#endif
  lf_unlock_by_pins(pins);
  return 1;
}

/*
  NOTE
   see lsearch() for pin usage
*/
int lf_hash_search(LF_HASH *hash, LF_PINS *pins, const uchar *key, uint keylen)
{
  int res;
  LF_SLIST * volatile *el;
  uint bucket, hashnr=calc_hash(hash, key, keylen);

  bucket= hashnr % hash->size;
  lf_lock_by_pins(pins);
  el=_lf_dynarray_lvalue(&hash->array, bucket);
  if (*el == NULL)
    initialize_bucket(hash, el, bucket, pins);
  res=NULL != lsearch(el, my_reverse_bits(hashnr) | 1, key, keylen, pins);
  lf_unlock_by_pins(pins);
  return res;
}

static void initialize_bucket(LF_HASH *hash, LF_SLIST * volatile *node,
                              uint bucket, LF_PINS *pins)
{
  uint parent= my_clear_highest_bit(bucket);
  LF_SLIST *dummy=_lf_alloc_new(pins), **tmp=0, *cur;
  LF_SLIST * volatile *el=_lf_dynarray_lvalue(&hash->array, parent);
  if (*el == NULL && bucket)
    initialize_bucket(hash, el, parent, pins);
  dummy->hashnr=my_reverse_bits(bucket);
  LINT_INIT(dummy->key);
  LINT_INIT(dummy->keylen);
  if ((cur= linsert(el, dummy, pins, 0)))
  {
    _lf_alloc_free(pins, dummy);
    dummy= cur;
  }
  my_atomic_casptr((void **)node, (void **)&tmp, dummy);
}

