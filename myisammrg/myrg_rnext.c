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

	/*
	   Read next row with the same key as previous read
	*/

int myrg_rnext(MYRG_INFO *info, byte *buf, int inx)
{
  MYRG_TABLE *table;
  MI_INFO *mi;
  uchar *key_buff;
  uint pack_key_length;
  int err;

  /* at first, do rnext for the table found before */
  err=mi_rnext(info->current_table->table,NULL,inx);
  if (err == HA_ERR_END_OF_FILE)
  {
    queue_remove(&(info->by_key),0);
    if (!info->by_key.elements)
      return HA_ERR_END_OF_FILE;
  }
  else if (err)
    return err;
  else
  {
    /* Found here, adding to queue */
    queue_top(&(info->by_key))=(byte *)(info->current_table);
    queue_replaced(&(info->by_key));
  }

  /* next, let's finish myrg_rkey's initial scan */
  table=info->last_used_table+1;
  if (table < info->end_table)
  {
    mi=info->last_used_table->table;
    key_buff=mi->lastkey+mi->s->base.max_key_length;
    pack_key_length=mi->last_rkey_length;
    for (; table < info->end_table ; table++)
    {
      mi=table->table;
      err=_mi_rkey(mi,NULL,inx,key_buff,pack_key_length,HA_READ_KEY_OR_NEXT,FALSE);
      info->last_used_table=table;

      if (err == HA_ERR_KEY_NOT_FOUND)
        continue;
      if (err)
        return err;

      /* Found here, adding to queue */
      queue_insert(&(info->by_key),(byte *)table);
    }
  }

  /* now, mymerge's read_next is as simple as one queue_top */
  mi=(info->current_table=(MYRG_TABLE *)queue_top(&(info->by_key)))->table;
  return mi_rrnd(mi,buf,mi->lastpos);
}

