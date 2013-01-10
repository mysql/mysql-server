/* Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
   02110-1301 USA */

#include "rpl_gtid.h"

#include "mysqld_error.h"
#include "hash.h"


Owned_gtids::Owned_gtids(Checkable_rwlock *_sid_lock)
  : sid_lock(_sid_lock)
{
  my_init_dynamic_array(&sidno_to_hash, sizeof(HASH *), 0, 8);
  /*
  my_hash_init(&gtid_to_owner, &my_charset_bin, 20,
               offsetof(Node, group), sizeof(Group), NULL,
               my_free, 0);
  */
}


Owned_gtids::~Owned_gtids()
{
  // destructor should only be called when no other thread may access object
  //sid_lock->assert_no_lock();
  // need to hold lock before calling get_max_sidno
  sid_lock->rdlock();
  rpl_sidno max_sidno= get_max_sidno();
  for (int sidno= 1; sidno <= max_sidno; sidno++)
  {
    HASH *hash= get_hash(sidno);
    my_hash_free(hash);
    my_free(hash);
  }
  delete_dynamic(&sidno_to_hash);
  sid_lock->unlock();
  //sid_lock->assert_no_lock();
}


enum_return_status Owned_gtids::ensure_sidno(rpl_sidno sidno)
{
  DBUG_ENTER("Owned_gtids::ensure_sidno");
  sid_lock->assert_some_wrlock();
  rpl_sidno max_sidno= get_max_sidno();
  if (sidno > max_sidno || get_hash(sidno) == NULL)
  {
    if (allocate_dynamic(&sidno_to_hash, sidno))
      goto error;
    for (int i= max_sidno; i < sidno; i++)
    {
      HASH *hash= (HASH *)my_malloc(sizeof(HASH), MYF(MY_WME));
      if (hash == NULL)
        goto error;
      my_hash_init(hash, &my_charset_bin, 20,
                   offsetof(Node, gno), sizeof(rpl_gno), NULL,
                   my_free, 0);
      set_dynamic(&sidno_to_hash, &hash, i);
    }
  }
  RETURN_OK;
error:
  BINLOG_ERROR(("Out of memory."), (ER_OUT_OF_RESOURCES, MYF(0)));
  RETURN_REPORTED_ERROR;
}


enum_return_status Owned_gtids::add_gtid_owner(const Gtid &gtid,
                                               my_thread_id owner)
{
  DBUG_ENTER("Owned_gtids::add_gtid_owner(Gtid, my_thread_id)");
  DBUG_ASSERT(!contains_gtid(gtid));
  DBUG_ASSERT(gtid.sidno <= get_max_sidno());
  Node *n= (Node *)my_malloc(sizeof(Node), MYF(MY_WME));
  if (n == NULL)
    RETURN_REPORTED_ERROR;
  n->gno= gtid.gno;
  n->owner= owner;
  /*
  printf("Owned_gtids(%p)::add sidno=%d gno=%lld n=%p n->owner=%u\n",
         this, sidno, gno, n, n?n->owner:0);
  */
  if (my_hash_insert(get_hash(gtid.sidno), (const uchar *)n) != 0)
  {
    my_free(n);
    BINLOG_ERROR(("Out of memory."), (ER_OUT_OF_RESOURCES, MYF(0)));
    RETURN_REPORTED_ERROR;
  }
  RETURN_OK;
}


void Owned_gtids::remove_gtid(const Gtid &gtid)
{
  DBUG_ENTER("Owned_gtids::remove_gtid(Gtid)");
  //printf("Owned_gtids::remove(sidno=%d gno=%lld)\n", sidno, gno);
  //DBUG_ASSERT(contains_gtid(sidno, gno)); // allow group not owned
  HASH *hash= get_hash(gtid.sidno);
  DBUG_ASSERT(hash != NULL);
  Node *node= get_node(hash, gtid.gno);
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


my_thread_id Owned_gtids::get_owner(const Gtid &gtid) const
{
  Node *n= get_node(gtid);
  if (n != NULL)
    return n->owner;
  return 0;
}


bool Owned_gtids::is_intersection_nonempty(const Gtid_set *other) const
{
  DBUG_ENTER("Owned_gtids::is_intersection_nonempty(Gtid_set *)");
  if (sid_lock != NULL)
    sid_lock->assert_some_wrlock();
  Gtid_iterator git(this);
  Gtid g= git.get();
  while (g.sidno != 0)
  {
    if (other->contains_gtid(g.sidno, g.gno))
      DBUG_RETURN(true);
    git.next();
    g= git.get();
  }
  DBUG_RETURN(false);
}
