/* Copyright (c) 2006, 2010, Oracle and/or its affiliates.
   Copyright (c) 2009, 2016, MariaDB

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

/*
  extensible hash

  TODO
     try to get rid of dummy nodes ?
     for non-unique hash, count only _distinct_ values
     (but how to do it in lf_hash_delete ?)
*/
#include <my_global.h>
#include <m_string.h>
#include <my_sys.h>
#include <my_bit.h>
#include <lf.h>

LF_REQUIRE_PINS(3)

/* An element of the list */
typedef struct {
  intptr volatile link; /* a pointer to the next element in a listand a flag */
  uint32 hashnr;        /* reversed hash number, for sorting                 */
  const uchar *key;
  size_t keylen;
  /*
    data is stored here, directly after the keylen.
    thus the pointer to data is (void*)(slist_element_ptr+1)
  */
} LF_SLIST;

const int LF_HASH_OVERHEAD= sizeof(LF_SLIST);

/*
  a structure to pass the context (pointers two the three successive elements
  in a list) from l_find to l_insert/l_delete
*/
typedef struct {
  intptr volatile *prev;
  LF_SLIST *curr, *next;
} CURSOR;

/*
  the last bit in LF_SLIST::link is a "deleted" flag.
  the helper macros below convert it to a pure pointer or a pure flag
*/
#define PTR(V)      (LF_SLIST *)((V) & (~(intptr)1))
#define DELETED(V)  ((V) & 1)

/*
  DESCRIPTION
    Search for hashnr/key/keylen in the list starting from 'head' and
    position the cursor. The list is ORDER BY hashnr, key

  RETURN
    0 - not found
    1 - found

  NOTE
    cursor is positioned in either case
    pins[0..2] are used, they are NOT removed on return
*/
static int l_find(LF_SLIST * volatile *head, CHARSET_INFO *cs, uint32 hashnr,
                 const uchar *key, uint keylen, CURSOR *cursor, LF_PINS *pins)
{
  uint32       cur_hashnr;
  const uchar  *cur_key;
  uint         cur_keylen;
  intptr       link;

retry:
  cursor->prev= (intptr *)head;
  do { /* PTR() isn't necessary below, head is a dummy node */
    cursor->curr= (LF_SLIST *)(*cursor->prev);
    _lf_pin(pins, 1, cursor->curr);
  } while (*cursor->prev != (intptr)cursor->curr && LF_BACKOFF);
  for (;;)
  {
    if (unlikely(!cursor->curr))
      return 0; /* end of the list */
    do {
      /* QQ: XXX or goto retry ? */
      link= cursor->curr->link;
      cursor->next= PTR(link);
      _lf_pin(pins, 0, cursor->next);
    } while (link != cursor->curr->link && LF_BACKOFF);
    cur_hashnr= cursor->curr->hashnr;
    cur_key= cursor->curr->key;
    cur_keylen= cursor->curr->keylen;
    if (*cursor->prev != (intptr)cursor->curr)
    {
      (void)LF_BACKOFF;
      goto retry;
    }
    if (!DELETED(link))
    {
      if (cur_hashnr >= hashnr)
      {
        int r= 1;
        if (cur_hashnr > hashnr ||
            (r= my_strnncoll(cs, (uchar*) cur_key, cur_keylen, (uchar*) key,
                             keylen)) >= 0)
          return !r;
      }
      cursor->prev= &(cursor->curr->link);
      _lf_pin(pins, 2, cursor->curr);
    }
    else
    {
      /*
        we found a deleted node - be nice, help the other thread
        and remove this deleted node
      */
      if (my_atomic_casptr((void **) cursor->prev,
                           (void **)(char*) &cursor->curr, cursor->next))
        _lf_alloc_free(pins, cursor->curr);
      else
      {
        (void)LF_BACKOFF;
        goto retry;
      }
    }
    cursor->curr= cursor->next;
    _lf_pin(pins, 1, cursor->curr);
  }
}

/*
  DESCRIPTION
    insert a 'node' in the list that starts from 'head' in the correct
    position (as found by l_find)

  RETURN
    0     - inserted
    not 0 - a pointer to a duplicate (not pinned and thus unusable)

  NOTE
    it uses pins[0..2], on return all pins are removed.
    if there're nodes with the same key value, a new node is added before them.
*/
static LF_SLIST *l_insert(LF_SLIST * volatile *head, CHARSET_INFO *cs,
                         LF_SLIST *node, LF_PINS *pins, uint flags)
{
  CURSOR         cursor;
  int            res;

  for (;;)
  {
    if (l_find(head, cs, node->hashnr, node->key, node->keylen,
              &cursor, pins) &&
        (flags & LF_HASH_UNIQUE))
    {
      res= 0; /* duplicate found */
      break;
    }
    else
    {
      node->link= (intptr)cursor.curr;
      DBUG_ASSERT(node->link != (intptr)node); /* no circular references */
      DBUG_ASSERT(cursor.prev != &node->link); /* no circular references */
      if (my_atomic_casptr((void **) cursor.prev,
                           (void **)(char*) &cursor.curr, node))
      {
        res= 1; /* inserted ok */
        break;
      }
    }
  }
  _lf_unpin(pins, 0);
  _lf_unpin(pins, 1);
  _lf_unpin(pins, 2);
  /*
    Note that cursor.curr is not pinned here and the pointer is unreliable,
    the object may dissapear anytime. But if it points to a dummy node, the
    pointer is safe, because dummy nodes are never freed - initialize_bucket()
    uses this fact.
  */
  return res ? 0 : cursor.curr;
}

/*
  DESCRIPTION
    deletes a node as identified by hashnr/keey/keylen from the list
    that starts from 'head'

  RETURN
    0 - ok
    1 - not found

  NOTE
    it uses pins[0..2], on return all pins are removed.
*/
static int l_delete(LF_SLIST * volatile *head, CHARSET_INFO *cs, uint32 hashnr,
                   const uchar *key, uint keylen, LF_PINS *pins)
{
  CURSOR cursor;
  int res;

  for (;;)
  {
    if (!l_find(head, cs, hashnr, key, keylen, &cursor, pins))
    {
      res= 1; /* not found */
      break;
    }
    else
    {
      /* mark the node deleted */
      if (my_atomic_casptr((void **) (char*) &(cursor.curr->link),
                           (void **) (char*) &cursor.next,
                           (void *)(((intptr)cursor.next) | 1)))
      {
        /* and remove it from the list */
        if (my_atomic_casptr((void **)cursor.prev,
                             (void **)(char*)&cursor.curr, cursor.next))
          _lf_alloc_free(pins, cursor.curr);
        else
        {
          /*
            somebody already "helped" us and removed the node ?
            Let's check if we need to help that someone too!
            (to ensure the number of "set DELETED flag" actions
            is equal to the number of "remove from the list" actions)
          */
          l_find(head, cs, hashnr, key, keylen, &cursor, pins);
        }
        res= 0;
        break;
      }
    }
  }
  _lf_unpin(pins, 0);
  _lf_unpin(pins, 1);
  _lf_unpin(pins, 2);
  return res;
}

/*
  DESCRIPTION
    searches for a node as identified by hashnr/keey/keylen in the list
    that starts from 'head'

  RETURN
    0 - not found
    node - found

  NOTE
    it uses pins[0..2], on return the pin[2] keeps the node found
    all other pins are removed.
*/
static LF_SLIST *l_search(LF_SLIST * volatile *head, CHARSET_INFO *cs,
                         uint32 hashnr, const uchar *key, uint keylen,
                         LF_PINS *pins)
{
  CURSOR cursor;
  int res= l_find(head, cs, hashnr, key, keylen, &cursor, pins);
  if (res)
    _lf_pin(pins, 2, cursor.curr);
  else
    _lf_unpin(pins, 2);
  _lf_unpin(pins, 1);
  _lf_unpin(pins, 0);
  return res ? cursor.curr : 0;
}

static inline const uchar* hash_key(const LF_HASH *hash,
                                    const uchar *record, size_t *length)
{
  if (hash->get_key)
    return (*hash->get_key)(record, length, 0);
  *length= hash->key_length;
  return record + hash->key_offset;
}

/*
  Compute the hash key value from the raw key.

  @note, that the hash value is limited to 2^31, because we need one
  bit to distinguish between normal and dummy nodes.
*/
static inline uint calc_hash(LF_HASH *hash, const uchar *key, uint keylen)
{
  ulong nr1= 1, nr2= 4;
  hash->charset->coll->hash_sort(hash->charset, (uchar*) key, keylen,
                                 &nr1, &nr2);
  return nr1 & INT_MAX32;
}

#define MAX_LOAD 1.0    /* average number of elements in a bucket */

static int initialize_bucket(LF_HASH *, LF_SLIST * volatile*, uint, LF_PINS *);

/*
  Initializes lf_hash, the arguments are compatible with hash_init

  @note element_size sets both the size of allocated memory block for
  lf_alloc and a size of memcpy'ed block size in lf_hash_insert. Typically
  they are the same, indeed. But LF_HASH::element_size can be decreased
  after lf_hash_init, and then lf_alloc will allocate larger block that
  lf_hash_insert will copy over. It is desireable if part of the element
  is expensive to initialize - for example if there is a mutex or
  DYNAMIC_ARRAY. In this case they should be initialize in the
  LF_ALLOCATOR::constructor, and lf_hash_insert should not overwrite them.
  See wt_init() for example.
*/
void lf_hash_init(LF_HASH *hash, uint element_size, uint flags,
                  uint key_offset, uint key_length, my_hash_get_key get_key,
                  CHARSET_INFO *charset)
{
  lf_alloc_init(&hash->alloc, sizeof(LF_SLIST)+element_size,
                offsetof(LF_SLIST, key));
  lf_dynarray_init(&hash->array, sizeof(LF_SLIST *));
  hash->size= 1;
  hash->count= 0;
  hash->element_size= element_size;
  hash->flags= flags;
  hash->charset= charset ? charset : &my_charset_bin;
  hash->key_offset= key_offset;
  hash->key_length= key_length;
  hash->get_key= get_key;
  DBUG_ASSERT(get_key ? !key_offset && !key_length : key_length);
}

void lf_hash_destroy(LF_HASH *hash)
{
  LF_SLIST *el, **head= (LF_SLIST **)_lf_dynarray_value(&hash->array, 0);

  if (head)
  {
    el= *head;
    while (el)
    {
      intptr next= el->link;
      if (el->hashnr & 1)
        lf_alloc_direct_free(&hash->alloc, el); /* normal node */
      else
        my_free(el); /* dummy node */
      el= (LF_SLIST *)next;
    }
  }
  lf_alloc_destroy(&hash->alloc);
  lf_dynarray_destroy(&hash->array);
}

/*
  DESCRIPTION
    inserts a new element to a hash. it will have a _copy_ of
    data, not a pointer to it.

  RETURN
    0 - inserted
    1 - didn't (unique key conflict)
   -1 - out of memory

  NOTE
    see l_insert() for pin usage notes
*/
int lf_hash_insert(LF_HASH *hash, LF_PINS *pins, const void *data)
{
  int csize, bucket, hashnr;
  LF_SLIST *node, * volatile *el;

  lf_rwlock_by_pins(pins);
  node= (LF_SLIST *)_lf_alloc_new(pins);
  if (unlikely(!node))
    return -1;
  memcpy(node+1, data, hash->element_size);
  node->key= hash_key(hash, (uchar *)(node+1), &node->keylen);
  hashnr= calc_hash(hash, node->key, node->keylen);
  bucket= hashnr % hash->size;
  el= _lf_dynarray_lvalue(&hash->array, bucket);
  if (unlikely(!el))
    return -1;
  if (*el == NULL && unlikely(initialize_bucket(hash, el, bucket, pins)))
    return -1;
  node->hashnr= my_reverse_bits(hashnr) | 1; /* normal node */
  if (l_insert(el, hash->charset, node, pins, hash->flags))
  {
    _lf_alloc_free(pins, node);
    lf_rwunlock_by_pins(pins);
    return 1;
  }
  csize= hash->size;
  if ((my_atomic_add32(&hash->count, 1)+1.0) / csize > MAX_LOAD)
    my_atomic_cas32(&hash->size, &csize, csize*2);
  lf_rwunlock_by_pins(pins);
  return 0;
}

/*
  DESCRIPTION
    deletes an element with the given key from the hash (if a hash is
    not unique and there're many elements with this key - the "first"
    matching element is deleted)
  RETURN
    0 - deleted
    1 - didn't (not found)
   -1 - out of memory
  NOTE
    see l_delete() for pin usage notes
*/
int lf_hash_delete(LF_HASH *hash, LF_PINS *pins, const void *key, uint keylen)
{
  LF_SLIST * volatile *el;
  uint bucket, hashnr= calc_hash(hash, (uchar *)key, keylen);

  bucket= hashnr % hash->size;
  lf_rwlock_by_pins(pins);
  el= _lf_dynarray_lvalue(&hash->array, bucket);
  if (unlikely(!el))
    return -1;
  /*
    note that we still need to initialize_bucket here,
    we cannot return "node not found", because an old bucket of that
    node may've been split and the node was assigned to a new bucket
    that was never accessed before and thus is not initialized.
  */
  if (*el == NULL && unlikely(initialize_bucket(hash, el, bucket, pins)))
    return -1;
  if (l_delete(el, hash->charset, my_reverse_bits(hashnr) | 1,
              (uchar *)key, keylen, pins))
  {
    lf_rwunlock_by_pins(pins);
    return 1;
  }
  my_atomic_add32(&hash->count, -1);
  lf_rwunlock_by_pins(pins);
  return 0;
}

/*
  RETURN
    a pointer to an element with the given key (if a hash is not unique and
    there're many elements with this key - the "first" matching element)
    NULL         if nothing is found
    MY_ERRPTR    if OOM

  NOTE
    see l_search() for pin usage notes
*/
void *lf_hash_search(LF_HASH *hash, LF_PINS *pins, const void *key, uint keylen)
{
  LF_SLIST * volatile *el, *found;
  uint bucket, hashnr= calc_hash(hash, (uchar *)key, keylen);

  bucket= hashnr % hash->size;
  lf_rwlock_by_pins(pins);
  el= _lf_dynarray_lvalue(&hash->array, bucket);
  if (unlikely(!el))
    return MY_ERRPTR;
  if (*el == NULL && unlikely(initialize_bucket(hash, el, bucket, pins)))
    return MY_ERRPTR;
  found= l_search(el, hash->charset, my_reverse_bits(hashnr) | 1,
                 (uchar *)key, keylen, pins);
  lf_rwunlock_by_pins(pins);
  return found ? found+1 : 0;
}

static const uchar *dummy_key= (uchar*)"";

/*
  RETURN
    0 - ok
   -1 - out of memory
*/
static int initialize_bucket(LF_HASH *hash, LF_SLIST * volatile *node,
                              uint bucket, LF_PINS *pins)
{
  uint parent= my_clear_highest_bit(bucket);
  LF_SLIST *dummy= (LF_SLIST *)my_malloc(sizeof(LF_SLIST), MYF(MY_WME));
  LF_SLIST **tmp= 0, *cur;
  LF_SLIST * volatile *el= _lf_dynarray_lvalue(&hash->array, parent);
  if (unlikely(!el || !dummy))
    return -1;
  if (*el == NULL && bucket &&
      unlikely(initialize_bucket(hash, el, parent, pins)))
    return -1;
  dummy->hashnr= my_reverse_bits(bucket) | 0; /* dummy node */
  dummy->key= dummy_key;
  dummy->keylen= 0;
  if ((cur= l_insert(el, hash->charset, dummy, pins, LF_HASH_UNIQUE)))
  {
    my_free(dummy);
    dummy= cur;
  }
  my_atomic_casptr((void **)node, (void **)(char*) &tmp, dummy);
  /*
    note that if the CAS above failed (after l_insert() succeeded),
    it would mean that some other thread has executed l_insert() for
    the same dummy node, its l_insert() failed, it picked up our
    dummy node (in "dummy= cur") and executed the same CAS as above.
    Which means that even if CAS above failed we don't need to retry,
    and we should not free(dummy) - there's no memory leak here
  */
  return 0;
}
