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
  Lock databases against read or write.
*/

#include "mymrgdef.h"

int myrg_lock_database(
MYRG_INFO *info,
int lock_type)
{
  int error,new_error;
  MYRG_TABLE *file;

  error=0;
  for (file=info->open_tables ; file != info->end_table ; file++)
    if ((new_error=mi_lock_database(file->table,lock_type)))
      error=new_error;
  return(error);
}
