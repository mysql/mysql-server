/* Copyright (C) 2001-2002, 2004 MySQL AB

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

/* Write a row to a MyISAM MERGE table */

#include "myrg_def.h"

int myrg_write(register MYRG_INFO *info, uchar *rec)
{
  /* [phi] MERGE_WRITE_DISABLED is handled by the else case */
  if (info->merge_insert_method == MERGE_INSERT_TO_FIRST)
    return mi_write((info->current_table=info->open_tables)->table,rec);
  else if (info->merge_insert_method == MERGE_INSERT_TO_LAST)
    return mi_write((info->current_table=info->end_table-1)->table,rec);
  else /* unsupported insertion method */
    return (my_errno= HA_ERR_WRONG_COMMAND);
}
