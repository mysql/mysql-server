/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "myrg_def.h"

	/*  Read first row according to specific key */

int myrg_rfirst(MYRG_INFO *info, uchar *buf, int inx)
{
  MYRG_TABLE *table;
  MI_INFO *mi;
  int err;

  if (_myrg_init_queue(info,inx,HA_READ_KEY_OR_NEXT))
    return my_errno();

  for (table=info->open_tables ; table != info->end_table ; table++)
  {
    if ((err=mi_rfirst(table->table,NULL,inx)))
    {
      if (err == HA_ERR_END_OF_FILE)
	continue;
      return err;
    }
    /* adding to queue */
    queue_insert(&(info->by_key),(uchar *)table);
  }
  /* We have done a read in all tables */
  info->last_used_table=table;

  if (!info->by_key.elements)
    return HA_ERR_END_OF_FILE;

  mi=(info->current_table=(MYRG_TABLE *)queue_top(&(info->by_key)))->table;
  return _myrg_mi_read_record(mi,buf);
}
