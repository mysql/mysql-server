/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Lock databases against read or write.
*/

#include <fcntl.h>

#include "storage/myisammrg/myrg_def.h"

#ifdef _WIN32
#include "storage/myisam/myisamdef.h"
#endif

int myrg_lock_database(MYRG_INFO *info, int lock_type) {
  int error, new_error;
  MYRG_TABLE *file;

  error = 0;
  for (file = info->open_tables; file != info->end_table; file++) {
#ifdef _WIN32
    /*
      Make sure this table is marked as owned by a merge table.
      The semaphore is never released as long as table remains
      in memory. This should be refactored into a more generic
      approach (observer pattern)
     */
    (file->table)->owned_by_merge = true;
#endif
    if ((new_error = mi_lock_database(file->table, lock_type))) {
      error = new_error;
      if (lock_type != F_UNLCK) {
        while (--file >= info->open_tables)
          mi_lock_database(file->table, F_UNLCK);
        break;
      }
    }
  }
  return (error);
}
