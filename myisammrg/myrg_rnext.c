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
  int err;
  MI_INFO *mi;

  /* at first, do rnext for the table found before */
  if ((err=mi_rnext(info->current_table->table,NULL,inx)))
  {
    if (err == HA_ERR_END_OF_FILE)
    {
      queue_remove(&(info->by_key),0);
      if (!info->by_key.elements)
	return HA_ERR_END_OF_FILE;
    }
    else
      return err;
  }
  else
  {
    /* Found here, adding to queue */
    queue_top(&(info->by_key))=(byte *)(info->current_table);
    queue_replaced(&(info->by_key));
  }

  /* next, let's finish myrg_rkey's initial scan */
  if ((err=_myrg_finish_scan(info, inx, HA_READ_KEY_OR_NEXT)))
    return err;

  /* now, mymerge's read_next is as simple as one queue_top */
  mi=(info->current_table=(MYRG_TABLE *)queue_top(&(info->by_key)))->table;
  return mi_rrnd(mi,buf,mi->lastpos);
}


/* let's finish myrg_rkey's initial scan */

int _myrg_finish_scan(MYRG_INFO *info, int inx, enum ha_rkey_function type)
{
  int err;
  MYRG_TABLE *table=info->last_used_table;
  if (table < info->end_table)
  {
    MI_INFO *mi= table[-1].table;
    byte *key_buff=(byte*) mi->lastkey+mi->s->base.max_key_length;
    uint pack_key_length=  mi->last_rkey_length;

    for (; table < info->end_table ; table++)
    {
      mi=table->table;
      if ((err=_mi_rkey(mi,NULL,inx,key_buff,pack_key_length,
			type,FALSE)))
      {
	if (err == HA_ERR_KEY_NOT_FOUND)	/* If end of file */
	  continue;
	return err;
      }
      /* Found here, adding to queue */
      queue_insert(&(info->by_key),(byte *) table);
    }
    /* All tables are now used */
    info->last_used_table=table;
  }
  return 0;
}
