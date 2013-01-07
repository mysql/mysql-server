/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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
  Rename a table
*/

#include "heapdef.h"

int heap_rename(const char *old_name, const char *new_name)
{
  HP_SHARE *info;
  char *name_buff;
  DBUG_ENTER("heap_rename");

  mysql_mutex_lock(&THR_LOCK_heap);
  if ((info = hp_find_named_heap(old_name)))
  {
    if (!(name_buff=(char*) my_strdup(new_name,MYF(MY_WME))))
    {
      mysql_mutex_unlock(&THR_LOCK_heap);
      DBUG_RETURN(my_errno);
    }
    my_free(info->name);
    info->name=name_buff;
  }
  mysql_mutex_unlock(&THR_LOCK_heap);
  DBUG_RETURN(0);
}
