/* Copyright (c) 2001, 2017, Oracle and/or its affiliates. All rights reserved.

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

/* Write a row to a MyISAM MERGE table */

#include "my_inttypes.h"
#include "storage/myisammrg/myrg_def.h"

int myrg_write(MYRG_INFO *info, uchar *rec)
{
  /* [phi] MERGE_WRITE_DISABLED is handled by the else case */
  if (info->merge_insert_method == MERGE_INSERT_TO_FIRST)
    return mi_write((info->current_table=info->open_tables)->table,rec);
  else if (info->merge_insert_method == MERGE_INSERT_TO_LAST)
    return mi_write((info->current_table=info->end_table-1)->table,rec);
  else /* unsupported insertion method */
  {
    set_my_errno(HA_ERR_WRONG_COMMAND);
    return HA_ERR_WRONG_COMMAND;
  }
}
