/* Copyright (c) 2000-2002, 2005-2007 MySQL AB
   Use is subject to license terms

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

/*
  Lock databases against read or write.
*/

#include "myrg_def.h"

int myrg_lock_database(MYRG_INFO *info, int lock_type)
{
  int error,new_error;
  MYRG_TABLE *file;

  error=0;
  for (file=info->open_tables ; file != info->end_table ; file++) 
  {
    DBUG_ASSERT(file->table->open_flag & HA_OPEN_MERGE_TABLE);

    if ((new_error=mi_lock_database(file->table,lock_type)))
    {
      error=new_error;
      if (lock_type != F_UNLCK)
      {
        while (--file >= info->open_tables)
          mi_lock_database(file->table, F_UNLCK);
        break;
      }
    }
  }
  return(error);
}
