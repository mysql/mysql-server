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

#include "myrg_def.h"

ha_rows myrg_records_in_range(MYRG_INFO *info, int inx, const byte *start_key,
                              uint start_key_len,
                              enum ha_rkey_function start_search_flag,
                              const byte *end_key, uint end_key_len,
                              enum ha_rkey_function end_search_flag)
{
  ha_rows records=0, res;
  MYRG_TABLE *table;

  for (table=info->open_tables ; table != info->end_table ; table++)
  {
    res=mi_records_in_range(table->table, inx,
                start_key, start_key_len, start_search_flag,
                  end_key,   end_key_len,   end_search_flag);
    if (res == HA_POS_ERROR)
      return HA_POS_ERROR; 
    if (records > HA_POS_ERROR - res)
      return HA_POS_ERROR-1;
    records+=res;
  }
  return records;
}

