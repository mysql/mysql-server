/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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

/* close a heap-database */

#include "heapdef.h"

	/* Close a database open by hp_open() */
	/* Data is normally not deallocated */

int heap_close(HP_INFO *info)
{
  int tmp;
  DBUG_ENTER("heap_close");
  mysql_mutex_lock(&THR_LOCK_heap);
  tmp= hp_close(info);
  mysql_mutex_unlock(&THR_LOCK_heap);
  DBUG_RETURN(tmp);
}


int hp_close(HP_INFO *info)
{
  int error=0;
  DBUG_ENTER("hp_close");
#ifndef DBUG_OFF
  if (info->s->changed && heap_check_heap(info,0))
  {
    error= HA_ERR_CRASHED;
    set_my_errno(error);
  }
#endif
  info->s->changed=0;
  if (info->open_list.data)
    heap_open_list=list_delete(heap_open_list,&info->open_list);
  if (!--info->s->open_count && info->s->delete_on_close)
    hp_free(info->s);				/* Table was deleted */
  my_free(info);
  DBUG_RETURN(error);
}
