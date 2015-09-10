/* Copyright (c) 2001, 2015, Oracle and/or its affiliates. All rights reserved.

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

/* Write a row to a MyISAM MERGE table */

#include "myrg_def.h"

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
