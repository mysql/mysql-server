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


#include "zgroups.h"


#ifdef HAVE_UGID


#include "mysqld_error.h"
#include "hash.h"


Owned_groups::Owned_groups(Checkable_rwlock *_sid_lock)
  : sid_lock(_sid_lock)
{
  my_init_dynamic_array(&sidno_to_hash, sizeof(HASH *), 0, 8);
  /*
  my_hash_init(&ugid_to_owner, &my_charset_bin, 20,
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


void Owned_groups::clear()
{
  sid_lock->assert_some_rdlock();
  rpl_sidno max_sidno= get_max_sidno();
  for (rpl_sidno sidno= 1; sidno <= max_sidno; sidno++)
  {
    HASH *hash= get_hash(sidno);
    for (uint i= 0; i < hash->records; i++)
    {
      Node *node= (Node *)my_hash_element(hash, i);
      DBUG_ASSERT(node != NULL);
      node->is_partial= false;
    }
  }
}


enum_return_status
Owned_groups::add(rpl_sidno sidno, rpl_gno gno, Rpl_owner_id owner)
{
  DBUG_ENTER("Owned_groups::add");
  DBUG_ASSERT(!contains_group(sidno, gno));
  DBUG_ASSERT(sidno <= get_max_sidno());
  Node *n= (Node *)malloc(sizeof(Node));
  if (n == NULL)
    goto error;
  n->gno= gno;
  n->owner= owner;
  n->is_partial= false;
  /*
  printf("Owned_groups(%p)::add sidno=%d gno=%lld n=%p n->is_partial=%d n->owner=%d:%u\n",
         this, sidno, gno, n,
         n?n->is_partial:-1, n?n->owner.owner_type:0, n?n->owner.thread_id:0);
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
  //DBUG_ASSERT(contains_group(sidno, gno)); // allow group not owned
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


bool Owned_groups::mark_partial(rpl_sidno sidno, rpl_gno gno)
{
  DBUG_ENTER("Owned_groups::mark_partial");
  Node *n= get_node(sidno, gno);
  /*
  printf("Owned_groups(%p)::mark_partial sidno=%d gno=%lld n=%pn->is_partial=%d  n->owner=%d:%u\n",
         this,
         sidno, gno, n,
         n?n->is_partial:-1, n?n->owner.owner_type:0, n?n->owner.thread_id:0);
  */
  DBUG_ASSERT(n != NULL);
  bool old_partial= n->is_partial;
  n->is_partial= true;
  DBUG_RETURN(old_partial);
}


Rpl_owner_id Owned_groups::get_owner(rpl_sidno sidno, rpl_gno gno) const
{
  Node *n= get_node(sidno, gno);
  if (n != NULL)
    return n->owner;
  Rpl_owner_id ret;
  ret.set_to_none();
  return ret;
}


void Owned_groups::change_owner(rpl_sidno sidno, rpl_gno gno,
                                Rpl_owner_id owner_id) const
{
  DBUG_ENTER("Owned_groups::change_owner");
  Node *n= get_node(sidno, gno);
  DBUG_ASSERT(n != NULL);
  n->owner= owner_id;
  DBUG_VOID_RETURN;
}


bool Owned_groups::is_partial(rpl_sidno sidno, rpl_gno gno) const
{
  DBUG_ENTER("Owned_groups::is_partial");
  Node *n= get_node(sidno, gno);
  DBUG_ASSERT(n != NULL);
  DBUG_RETURN(n->is_partial);
}


enum_return_status Owned_groups::get_partial_groups(Group_set *gs) const
{
  DBUG_ENTER("Owned_groups::get_partial_groups");
  rpl_sidno max_sidno= get_max_sidno();
  PROPAGATE_REPORTED_ERROR(gs->ensure_sidno(max_sidno));
  for (int sidno= 1; sidno <= max_sidno; sidno++)
  {
    HASH *hash= get_hash(sidno);
    int hash_size= hash->records;
    for (int i= 0; i < hash_size; i++)
    {
      Node *node= (Node *)my_hash_element(hash, i);
      if (node->is_partial)
        PROPAGATE_REPORTED_ERROR(gs->_add(sidno, node->gno));
    }
  }
  RETURN_OK;
}


#endif /* HAVE_UGID */
