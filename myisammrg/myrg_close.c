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

/* close a isam-database */

#include "mymrgdef.h"

int myrg_close(MYRG_INFO *info)
{
  int error=0,new_error;
  MYRG_TABLE *file;
  DBUG_ENTER("myrg_close");

  for (file=info->open_tables ; file != info->end_table ; file++)
    if ((new_error=mi_close(file->table)))
      error=new_error;
  delete_queue(&info->by_key);
  pthread_mutex_lock(&THR_LOCK_open);
  myrg_open_list=list_delete(myrg_open_list,&info->open_list);
  pthread_mutex_unlock(&THR_LOCK_open);
  my_free((gptr) info,MYF(0));
  if (error)
  {
    DBUG_RETURN(my_errno=error);
  }
  DBUG_RETURN(0);
}
