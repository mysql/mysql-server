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
#include "sql_class.h"


#ifdef HAVE_UGID


void Rpl_owner_id::copy_from(const THD *thd)
{
  if (thd->system_thread == SYSTEM_THREAD_SLAVE_SQL)
  {
    owner_type= 1;
    thread_id= 0;
  }
  else
  {
    owner_type= 0;
    thread_id= thd->thread_id;
  }
}


bool Rpl_owner_id::equals(const THD *thd) const
{
  return owner_type == 0 ? thread_id == thd->thread_id :
    thd->system_thread == SYSTEM_THREAD_SLAVE_SQL;
}


bool Rpl_owner_id::is_live_client() const
{
  bool ret= false;
  if (owner_type == 0 && thread_id > 0)
  {
    // check if thread exists
    mysql_mutex_lock(&LOCK_thread_count);
    THD *thd;
    I_List_iterator<THD> it(threads);
    while ((thd= it++))
    {
      if (thd->thread_id == thread_id)
      {
        ret= true;
        break;
      }
    }
    mysql_mutex_unlock(&LOCK_thread_count);
  }
  return ret;
}


#endif
