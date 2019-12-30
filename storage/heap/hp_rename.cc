/* Copyright (c) 2000, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Rename a table
*/

#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysql/service_mysql_alloc.h"
#include "storage/heap/heapdef.h"

int heap_rename(const char *old_name, const char *new_name) {
  HP_SHARE *info;
  char *name_buff;
  DBUG_TRACE;

  mysql_mutex_lock(&THR_LOCK_heap);
  if ((info = hp_find_named_heap(old_name))) {
    if (!(name_buff = (char *)my_strdup(hp_key_memory_HP_SHARE, new_name,
                                        MYF(MY_WME)))) {
      mysql_mutex_unlock(&THR_LOCK_heap);
      return my_errno();
    }
    my_free(info->name);
    info->name = name_buff;
  }
  mysql_mutex_unlock(&THR_LOCK_heap);
  return 0;
}
