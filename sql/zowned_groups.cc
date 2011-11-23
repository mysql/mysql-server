/* Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


#include "zgtids.h"


#ifdef HAVE_GTID


#include "mysqld_error.h"
#include "hash.h"


Owned_groups::Owned_groups(Checkable_rwlock *_sid_lock)
  : sid_lock(_sid_lock)
{
  my_init_dynamic_array(&sidno_to_hash, sizeof(HASH *), 0, 8);
  /*
  my_hash_init(&gtid_to_owner, &my_charset_bin, 20,
               offsetof(Node, group), sizeof(Group), NULL,
               my_free, 0);
  */
}


Owned_groups::~Owned_groups()
{
  // destructor should only be called when no other thread may access object
  sid_lock->assert_no_lock();
  // need to hold lock before calling get_max_sidno
  sid_lock->rdlock();
  rpl_sidno max_sidno= get_max_sidno();
  for (int sidno= 1; sidno <= max_sidno; sidno++)
  {
    HASH *hash= get_hash(sidno);
    my_hash_free(hash);
    free(hash);
  }
  delete_dynamic(&sidno_to_hash);
  sid_lock->unlock();
  sid_lock->assert_no_lock();
}


enum_return_status Owned_groups::ensure_sidno(rpl_sidno sidno)
{
  DBUG_ENTER("Owned_groups::ensure_sidno");
  sid_lock->assert_some_rdlock();
  rpl_sidno max_sidno= get_max_sidno();
  if (sidno > max_sidno || get_hash(sidno) == NULL)
  {
    sid_lock->unlock();
    sid_lock->wrlock();
    if (sidno > max_sidno || get_hash(sidno) == NULL)
    {
      if (allocate_dynamic(&sidno_to_hash, sidno))
        goto error;
      for (int i= max_sidno; i < sidno; i++)
      {
        HASH *hash= (HASH *)malloc(sizeof(HASH));
        if (hash == NULL)
          goto error;
        my_hash_init(hash, &my_charset_bin, 20,
                     offsetof(Node, gno), sizeof(rpl_gno), NULL,
                     my_free, 0);
        set_dynamic(&sidno_to_hash, &hash, i);
      }
    }
    sid_lock->unlock();
    sid_lock->rdlock();
  }
  RETURN_OK;
error:
  sid_lock->unlock();
  sid_lock->rdlock();
  BINLOG_ERROR(("Out of memory."), (ER_OUT_OF_RESOURCES, MYF(0)));
  RETURN_REPORTED_ERROR;
}


enum_return_status
Owned_groups::add(rpl_sidno sidno, rpl_gno gno, my_thread_id owner)
{
  DBUG_ENTER("Owned_groups::add");
  DBUG_ASSERT(!contains_gtid(sidno, gno));
  DBUG_ASSERT(sidno <= get_max_sidno());
  Node *n= (Node *)malloc(sizeof(Node));
  if (n == NULL)
    goto error;
  n->gno= gno;
  n->owner= owner;
  /*
  printf("Owned_groups(%p)::add sidno=%d gno=%lld n=%p n->owner=%u\n",
         this, sidno, gno, n, n?n->owner:0);
  */
  if (my_hash_insert(get_hash(sidno), (const uchar *)n))
  {
    free(n);
    goto error;
  }
  RETURN_OK;
error:
  BINLOG_ERROR(("Out of memory."), (ER_OUT_OF_RESOURCES, MYF(0)));
  RETURN_REPORTED_ERROR;
}


void Owned_groups::remove(rpl_sidno sidno, rpl_gno gno)
{
  DBUG_ENTER("Owned_groups::remove");
  //printf("Owned_groups::remove(sidno=%d gno=%lld)\n", sidno, gno);
  //DBUG_ASSERT(contains_gtid(sidno, gno)); // allow group not owned
  HASH *hash= get_hash(sidno);
  DBUG_ASSERT(hash != NULL);
  Node *node= get_node(hash, gno);
  if (node != NULL)
  {
#ifdef DBUG_OFF
    my_hash_delete(hash, (uchar *)node);
#else
    // my_hash_delete returns nonzero if the element does not exist
    DBUG_ASSERT(my_hash_delete(hash, (uchar *)node) == 0);
#endif
  }
  DBUG_VOID_RETURN;
}


my_thread_id Owned_groups::get_owner(rpl_sidno sidno, rpl_gno gno) const
{
  Node *n= get_node(sidno, gno);
  if (n != NULL)
    return n->owner;
  return 0;
}


#endif /* HAVE_GTID */
