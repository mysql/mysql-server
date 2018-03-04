/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

/* close a isam-database */

#include <stddef.h>

#include "my_dbug.h"
#include "storage/myisammrg/myrg_def.h"

int myrg_close(MYRG_INFO *info)
{
  int error=0,new_error;
  MYRG_TABLE *file;
  DBUG_ENTER("myrg_close");

  /*
    Assume that info->children_attached means that this is called from
    direct use of MERGE, not from a MySQL server. In this case the
    children must be closed and info->rec_per_key_part is part of the
    'info' multi_alloc.
    If info->children_attached is false, this is called from a MySQL
    server. Children are closed independently but info->rec_per_key_part
    must be freed.
    Just in case of a server panic (myrg_panic()) info->children_attached
    might be true. We would close the children though they should be
    closed independently and info->rec_per_key_part is not freed.
    This should be acceptable for a panic.
    In case of a MySQL server and no children, children_attached is
    always true. In this case no rec_per_key_part has been allocated.
    So it is correct to use the branch where an empty list of tables is
    (not) closed.
  */
  if (info->children_attached)
  {
    for (file= info->open_tables; file != info->end_table; file++)
    {
      /* purecov: begin inspected */
      if ((new_error= mi_close(file->table)))
        error= new_error;
      else
        file->table= NULL;
      /* purecov: end */
    }
  }
  else
    my_free(info->rec_per_key_part);
  delete_queue(&info->by_key);
  mysql_mutex_lock(&THR_LOCK_open);
  myrg_open_list=list_delete(myrg_open_list,&info->open_list);
  mysql_mutex_unlock(&THR_LOCK_open);
  mysql_mutex_destroy(&info->mutex);
  my_free(info);
  if (error)
  {
    set_my_errno(error);
    DBUG_RETURN(error);
  }
  DBUG_RETURN(0);
}
