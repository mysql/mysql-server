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

#include "mymrgdef.h"

ulonglong myrg_position(MYRG_INFO *info)
{
  MYRG_TABLE *current_table;

  if (!(current_table = info->current_table) &&
      info->open_tables != info->end_table)
    current_table = info->open_tables;
  return  current_table ?
    current_table->table->lastpos + current_table->file_offset :
    ~(ulonglong) 0;
}

	/* If flag != 0 one only gets pos of last record */

int myrg_status(MYRG_INFO *info,register MYMERGE_INFO *x,int flag)
{
  MYRG_TABLE *current_table;
  DBUG_ENTER("myrg_status");

  _myrg_find_table(info, HA_OFFSET_ERROR);

  if (!(current_table = info->current_table) &&
      info->open_tables != info->end_table)
    current_table = info->open_tables;
  x->recpos  = info->current_table ?
    info->current_table->table->lastpos + info->current_table->file_offset :
      (ulong) -1L;
  if (flag != HA_STATUS_POS)
  {
    x->records	 = info->records;
    x->deleted	 = info->del;
    x->data_file_length = info->data_file_length;
    x->reclength  = info->reclength;
    if (current_table)
      x->errkey  = current_table->table->errkey;
    else
    {						/* No tables in MRG */
      x->errkey=0;
    }
    x->options	 = info->options;
  }
  DBUG_RETURN(0);
}
