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
  Rename a table
*/

#include "heapdef.h"

int heap_rename(const char *old_name, const char *new_name)
{
  reg1 HP_SHARE *info;
  char *name_buff;
  DBUG_ENTER("heap_rename");

  pthread_mutex_lock(&THR_LOCK_heap);
  if ((info=_hp_find_named_heap(old_name)))
  {
    if (!(name_buff=(char*) my_strdup(new_name,MYF(MY_WME))))
    {
      pthread_mutex_unlock(&THR_LOCK_heap);
      DBUG_RETURN(my_errno);
    }
    my_free(info->name,MYF(0));
    info->name=name_buff;
  }
  pthread_mutex_unlock(&THR_LOCK_heap);
  DBUG_RETURN(0);
}
