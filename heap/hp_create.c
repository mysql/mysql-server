/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Create is done by simply remove the database from memory if it exists.
  Open creates the database when neaded
*/

#include "heapdef.h"

int heap_create(const char *name)
{
  DBUG_ENTER("heap_create");
  (void) heap_delete_all(name);
  DBUG_RETURN(0);
}

int heap_delete_all(const char *name)
{
  reg1 HP_SHARE *info;
  int found;
  DBUG_ENTER("heap_delete_all");
  pthread_mutex_lock(&THR_LOCK_heap);
  if ((info=_hp_find_named_heap(name)))
  {
    if (info->open_count == 0)
      _hp_free(info);
    found=0;
  }
  else
  {
    found=my_errno=ENOENT;
  }
  pthread_mutex_unlock(&THR_LOCK_heap);
  DBUG_RETURN(found);
}

void _hp_free(HP_SHARE *share)
{
  heap_share_list=list_delete(heap_share_list,&share->open_list);
  _hp_clear(share);			/* Remove blocks from memory */
#ifdef THREAD
  thr_lock_delete(&share->lock);
  VOID(pthread_mutex_destroy(&share->intern_lock));
#endif
  my_free((gptr) share->name,MYF(0));
  my_free((gptr) share,MYF(0));
  return;
}
