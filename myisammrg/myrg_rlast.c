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

	/* Read last row with the same key as the previous read. */

int myrg_rlast(MYRG_INFO *info, byte *buf, int inx)
{
  MYRG_TABLE *table;
  MI_INFO *mi;
  int err;

  if (_myrg_init_queue(info,inx, HA_READ_KEY_OR_PREV))
    return my_errno;

  for (table=info->open_tables ; table < info->end_table ; table++)
  {
    if ((err=mi_rlast(table->table,NULL,inx)))
    {
      if (err == HA_ERR_END_OF_FILE)
	continue;
      return err;
    }
    /* adding to queue */
    queue_insert(&(info->by_key),(byte *)table);
  }
  /* We have done a read in all tables */
  info->last_used_table=table;

  if (!info->by_key.elements)
    return HA_ERR_END_OF_FILE;

  mi=(info->current_table=(MYRG_TABLE *)queue_top(&(info->by_key)))->table;
  return mi_rrnd(mi,buf,mi->lastpos);
}

