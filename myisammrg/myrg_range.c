/* Copyright (C) 2002, 2004 MySQL AB

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

#include "myrg_def.h"

ha_rows myrg_records_in_range(MYRG_INFO *info, int inx,
                              key_range *min_key, key_range *max_key)
{
  ha_rows records=0, res;
  MYRG_TABLE *table;

  for (table=info->open_tables ; table != info->end_table ; table++)
  {
    res= mi_records_in_range(table->table, inx, min_key, max_key);
    if (res == HA_POS_ERROR)
      return HA_POS_ERROR; 
    if (records > HA_POS_ERROR - res)
      return HA_POS_ERROR-1;
    records+=res;
  }
  return records;
}

