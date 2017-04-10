/* Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <string.h>

#include "control_events.h"
#include "hash.h"
#include "m_ctype.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/psi/psi_memory.h"
#include "mysql/service_mysql_alloc.h"
#include "mysqld_error.h"    // ER_*
#include "prealloced_array.h"
#include "rpl_gtid.h"

#ifndef MYSQL_SERVER
#include "mysqlbinlog.h"
#endif

extern "C" {
PSI_memory_key key_memory_Sid_map_Node;
}

Sid_map::Sid_map(Checkable_rwlock *_sid_lock)
  : sid_lock(_sid_lock),
    _sidno_to_sid(key_memory_Sid_map_Node), _sorted(key_memory_Sid_map_Node)
{
  DBUG_ENTER("Sid_map::Sid_map");
  my_hash_init(&_sid_to_sidno, &my_charset_bin, 20, 0,
               sid_map_get_key,
               my_free, 0,
               key_memory_Sid_map_Node);
  DBUG_VOID_RETURN;
}


Sid_map::~Sid_map()
{
  DBUG_ENTER("Sid_map::~Sid_map");
  my_hash_free(&_sid_to_sidno);
  DBUG_VOID_RETURN;
}


enum_return_status Sid_map::clear()
{
  DBUG_ENTER("Sid_map::clear");
  my_hash_free(&_sid_to_sidno);
  my_hash_init(&_sid_to_sidno, &my_charset_bin, 20, 0,
               sid_map_get_key,
               my_free, 0, PSI_INSTRUMENT_ME);
  _sidno_to_sid.clear();
  _sorted.clear();
  RETURN_OK;
}

rpl_sidno Sid_map::add_sid(const rpl_sid &sid)
{
  DBUG_ENTER("Sid_map::add_sid(const rpl_sid *)");
#ifndef DBUG_OFF
  char buf[binary_log::Uuid::TEXT_LENGTH + 1];
  sid.to_string(buf);
  DBUG_PRINT("info", ("SID=%s", buf));
#endif
  if (sid_lock)
    sid_lock->assert_some_lock();
  Node *node= (Node *)my_hash_search(&_sid_to_sidno, sid.bytes,
                                     binary_log::Uuid::BYTE_LENGTH);
  if (node != NULL)
  {
    DBUG_PRINT("info", ("existed as sidno=%d", node->sidno));
    DBUG_RETURN(node->sidno);
  }

  bool is_wrlock= false;
  if (sid_lock)
  {
    is_wrlock= sid_lock->is_wrlock();
    if (!is_wrlock)
    {
      sid_lock->unlock();
      sid_lock->wrlock();
    }
  }
  DBUG_PRINT("info", ("is_wrlock=%d sid_lock=%p", is_wrlock, sid_lock));
  rpl_sidno sidno;
  node= (Node *)my_hash_search(&_sid_to_sidno, sid.bytes,
                               binary_log::Uuid::BYTE_LENGTH);
  if (node != NULL)
    sidno= node->sidno;
  else
  {
    sidno= get_max_sidno() + 1;
    if (add_node(sidno, sid) != RETURN_STATUS_OK)
      sidno= -1;
  }

  if (sid_lock)
  {
    if (!is_wrlock)
    {
      sid_lock->unlock();
      sid_lock->rdlock();
    }
  }
  DBUG_RETURN(sidno);
}

enum_return_status Sid_map::add_node(rpl_sidno sidno, const rpl_sid &sid)
{
  DBUG_ENTER("Sid_map::add_node(rpl_sidno, const rpl_sid *)");
  if (sid_lock)
    sid_lock->assert_some_wrlock();
  Node *node= (Node *)my_malloc(key_memory_Sid_map_Node,
                                sizeof(Node), MYF(MY_WME));
  if (node == NULL)
    RETURN_REPORTED_ERROR;

  node->sidno= sidno;
  node->sid= sid;
  if (!_sidno_to_sid.push_back(node))
  {
    if (!_sorted.push_back(sidno))
    {
      if (my_hash_insert(&_sid_to_sidno, (uchar *)node) == 0)
      {
#ifdef MYSQL_SERVER
        /*
          If this is the global_sid_map, we take the opportunity to
          resize all arrays in gtid_state while holding the wrlock.
        */
        if (this != global_sid_map ||
            gtid_state->ensure_sidno() == RETURN_STATUS_OK)
#endif
        {
          // We have added one element to the end of _sorted.  Now we
          // bubble it down to the sorted position.
          int sorted_i= sidno - 1;
          rpl_sidno *prev_sorted_p= &_sorted[sorted_i];
          sorted_i--;
          while (sorted_i >= 0)
          {
            rpl_sidno *sorted_p= &_sorted[sorted_i];
            const rpl_sid &other_sid= sidno_to_sid(*sorted_p);
            if (memcmp(sid.bytes, other_sid.bytes,
                       binary_log::Uuid::BYTE_LENGTH) >= 0)
              break;
            memcpy(prev_sorted_p, sorted_p, sizeof(rpl_sidno));
            sorted_i--;
            prev_sorted_p= sorted_p;
          }
          memcpy(prev_sorted_p, &sidno, sizeof(rpl_sidno));
          RETURN_OK;
        }
      }
      _sorted.pop_back();
    }
    _sidno_to_sid.pop_back();
  }
  my_free(node);

  BINLOG_ERROR(("Out of memory."), (ER_OUT_OF_RESOURCES, MYF(0)));
  RETURN_REPORTED_ERROR;
}


enum_return_status Sid_map::copy(Sid_map *dest)
{
  DBUG_ENTER("Sid_map::copy(Sid_map)");
  enum_return_status return_status= RETURN_STATUS_OK;

  rpl_sidno max_sidno= get_max_sidno();
  for (rpl_sidno sidno= 1;
       sidno <= max_sidno && return_status == RETURN_STATUS_OK;
       sidno++)
  {
    rpl_sid sid;
    sid.copy_from(sidno_to_sid(sidno));
    return_status= dest->add_node(sidno, sid);
  }

  DBUG_RETURN(return_status);
}
