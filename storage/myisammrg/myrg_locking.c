/* Copyright (C) 2000-2002 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

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
#ifdef __WIN__
    /*
      Make sure this table is marked as owned by a merge table.
      The semaphore is never released as long as table remains
      in memory. This should be refactored into a more generic
      approach (observer pattern)
     */
    (file->table)->owned_by_merge = TRUE;
#endif
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
