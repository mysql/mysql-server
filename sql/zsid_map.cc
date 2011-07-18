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
#include "hash.h"


#ifdef HAVE_UGID


Sid_map::Sid_map(Checkable_rwlock *_sid_lock)
  : sid_lock(_sid_lock)
{
  DBUG_ENTER("Sid_map::Sid_map");
  my_init_dynamic_array(&_sidno_to_sid, sizeof(Node *), 8, 8);
  my_init_dynamic_array(&_sorted, sizeof(rpl_sidno), 8, 8);
  my_hash_init(&_sid_to_sidno, &my_charset_bin, 20,
               offsetof(Node, sid.bytes), Uuid::BYTE_LENGTH, NULL,
               my_free, 0);
  
  DBUG_VOID_RETURN;
}


Sid_map::~Sid_map()
{
  DBUG_ENTER("Sid_map::~Sid_map");
  delete_dynamic(&_sidno_to_sid);
  delete_dynamic(&_sorted);
  my_hash_free(&_sid_to_sidno);
#ifndef NO_DBUG
  my_atomic_rwlock_init(&is_read_locked_lock);
#endif
  DBUG_VOID_RETURN;
}


rpl_sidno Sid_map::add_permanent(const rpl_sid *sid, bool _flush)
{
  DBUG_ENTER("Sid_map::add_permanent");
  sid_lock->assert_some_rdlock();
  Node *node= (Node *)my_hash_search(&_sid_to_sidno, sid->bytes,
                                     rpl_sid::BYTE_LENGTH);
  if (node != NULL)
    DBUG_RETURN(node->sidno);

  sid_lock->unlock();
  sid_lock->wrlock();
  node= (Node *)my_hash_search(&_sid_to_sidno, sid->bytes,
                               rpl_sid::BYTE_LENGTH);
  if (node != NULL)
  {
    sid_lock->unlock();
    sid_lock->rdlock();
    DBUG_RETURN(node->sidno);
  }

  enum_group_status status= GS_ERROR_OUT_OF_MEMORY;
  node= (Node *)malloc(sizeof(Node));
  if (node != NULL)
  {
    rpl_sidno sidno= get_max_sidno() + 1;
    node->sidno= sidno;
    node->sid= *sid;
    if (insert_dynamic(&_sidno_to_sid, &node) == 0)
    {
      if (insert_dynamic(&_sorted, &sidno) == 0)
      {
        if (my_hash_insert(&_sid_to_sidno, (uchar *)node) == 0)
        {
          status= _flush ? flush() : GS_SUCCESS;
          if (status == GS_SUCCESS)
          {
            // We have added one element to the end of _sorted.  Now we
            // bubble it down to the sorted position.
            int sorted_i= sidno - 1;
            rpl_sidno *prev_sorted_p= dynamic_element(&_sorted, sorted_i,
                                                      rpl_sidno *);
            sorted_i--;
            while (sorted_i >= 0)
            {
              rpl_sidno *sorted_p= dynamic_element(&_sorted, sorted_i,
                                                   rpl_sidno *);
              const rpl_sid *other_sid= sidno_to_sid(*sorted_p);
              if (memcmp(sid->bytes, other_sid->bytes,
                         rpl_sid::BYTE_LENGTH) >= 0)
                break;
              memcpy(prev_sorted_p, sorted_p, sizeof(rpl_sidno));
              sorted_i--;
              prev_sorted_p= sorted_p;
            }
            memcpy(prev_sorted_p, &sidno, sizeof(rpl_sidno));
            sid_lock->unlock();
            sid_lock->rdlock();
            sid_lock->assert_some_rdlock();
            DBUG_RETURN(sidno);
          }
          my_hash_delete(&_sid_to_sidno, (uchar *)node);
        }
        pop_dynamic(&_sorted);
      }
      pop_dynamic(&_sidno_to_sid);
    }
    free(node);
  }
  sid_lock->unlock();
  sid_lock->rdlock();
  DBUG_RETURN((rpl_sidno)status);
}


enum_group_status Sid_map::flush()
{
  DBUG_ENTER("Sid_map::flush()");
  /* do nothing for now */
  DBUG_RETURN(GS_SUCCESS);
}


#endif /* HAVE_UGID */
