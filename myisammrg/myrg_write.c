/* Copyright (C) 2001 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
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

/* Write a row to a MyISAM MERGE table */

#include "myrg_def.h"

int myrg_write(register MYRG_INFO *info, byte *rec)
{
  /* [phi] MERGE_WRITE_DISABLED is handled by the else case */
  if (info->merge_insert_method == MERGE_INSERT_TO_FIRST)
    return mi_write(info->open_tables[0].table,rec);
  else if (info->merge_insert_method == MERGE_INSERT_TO_LAST)
    return mi_write(info->end_table[-1].table,rec);
  else /* unsupported insertion method */
    return (my_errno=HA_ERR_WRONG_COMMAND); 
}
