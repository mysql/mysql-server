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
  Extra functions we want to do with a database
  - All flags, exept record-cache-flags, are set in all used databases
    record-cache-flags are set in myrg_rrnd when we are changing database.
*/

#include "mymrgdef.h"

int myrg_extra(MYRG_INFO *info,enum ha_extra_function function)
{
  int error,save_error=0;
  MYRG_TABLE *file;

  if (function == HA_EXTRA_CACHE)
    info->cache_in_use=1;
  else
  {
    if (function == HA_EXTRA_NO_CACHE || function == HA_EXTRA_RESET)
      info->cache_in_use=0;
    if (function == HA_EXTRA_RESET || function == HA_EXTRA_RESET_STATE)
    {
      info->current_table=0;
      info->last_used_table=info->open_tables;
    }
    for (file=info->open_tables ; file != info->end_table ; file++)
      if ((error=mi_extra(file->table,function)))
	save_error=error;
  }
  return save_error;
}
