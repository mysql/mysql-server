/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Extra functions we want to do with a database
  - All flags, except record-cache-flags, are set in all used databases
    record-cache-flags are set in myrg_rrnd when we are changing database.
*/

#include <sys/types.h>

#include "my_dbug.h"
#include "storage/myisammrg/myrg_def.h"

int myrg_extra(MYRG_INFO *info, enum ha_extra_function function,
               void *extra_arg) {
  int error, save_error = 0;
  MYRG_TABLE *file;
  DBUG_TRACE;
  DBUG_PRINT("info", ("function: %lu", (ulong)function));

  if (!info->children_attached) return 1;
  if (function == HA_EXTRA_RESET_STATE) {
    info->current_table = nullptr;
    info->last_used_table = info->open_tables;
  }
  for (file = info->open_tables; file != info->end_table; file++) {
    if ((error = mi_extra(file->table, function, extra_arg)))
      save_error = error;
  }
  return save_error;
}

int myrg_reset(MYRG_INFO *info) {
  int save_error = 0;
  MYRG_TABLE *file;
  DBUG_TRACE;

  info->current_table = nullptr;
  info->last_used_table = info->open_tables;

  /*
    This is normally called with detached children.
    Return OK as this is the normal case.
  */
  if (!info->children_attached) return 0;

  for (file = info->open_tables; file != info->end_table; file++) {
    int error;
    if ((error = mi_reset(file->table))) save_error = error;
  }
  return save_error;
}
