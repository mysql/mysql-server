/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

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

#include "myrg_def.h"

int myrg_extra(MYRG_INFO *info,enum ha_extra_function function,
	       void *extra_arg)
{
  int error,save_error=0;
  MYRG_TABLE *file;
  DBUG_ENTER("myrg_extra");
  DBUG_PRINT("info",("function: %lu", (ulong) function));

  if (!info->children_attached)
    DBUG_RETURN(1);
  if (function == HA_EXTRA_CACHE)
  {
    info->cache_in_use=1;
    info->cache_size= (extra_arg ? *(ulong*) extra_arg :
		       my_default_record_cache_size);
  }
  else
  {
    if (function == HA_EXTRA_NO_CACHE ||
        function == HA_EXTRA_PREPARE_FOR_UPDATE)
      info->cache_in_use=0;
    if (function == HA_EXTRA_RESET_STATE)
    {
      info->current_table=0;
      info->last_used_table=info->open_tables;
    }
    for (file=info->open_tables ; file != info->end_table ; file++)
    {
      if ((error=mi_extra(file->table, function, extra_arg)))
	save_error=error;
    }
  }
  DBUG_RETURN(save_error);
}


void myrg_extrafunc(MYRG_INFO *info, invalidator_by_filename inv)
{
  MYRG_TABLE *file;
  DBUG_ENTER("myrg_extrafunc");

  for (file=info->open_tables ; file != info->end_table ; file++)
    file->table->s->invalidator = inv;

  DBUG_VOID_RETURN;
}


int myrg_reset(MYRG_INFO *info)
{
  int save_error= 0;
  MYRG_TABLE *file;
  DBUG_ENTER("myrg_reset");

  if (!info->children_attached)
    DBUG_RETURN(1);
  info->cache_in_use=0;
  info->current_table=0;
  info->last_used_table= info->open_tables;
  
  for (file=info->open_tables ; file != info->end_table ; file++)
  {
    int error;
    if ((error= mi_reset(file->table)))
      save_error=error;
  }
  DBUG_RETURN(save_error);
}
