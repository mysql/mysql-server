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

#include "myrg_def.h"

static int queue_key_cmp(void *keyseg, byte *a, byte *b)
{
  MYRG_TABLE *ma= (MYRG_TABLE *)a;
  MYRG_TABLE *mb= (MYRG_TABLE *)b;
  MI_INFO *aa= ma->table;
  MI_INFO *bb= mb->table;
  uint not_used[2];
  int ret= ha_key_cmp((HA_KEYSEG *)keyseg, aa->lastkey, bb->lastkey,
		       USE_WHOLE_KEY, SEARCH_FIND, not_used);
  if (ret < 0)
    return -1;
  if (ret > 0)
    return 1;
 
  /*
    If index tuples have the same values, let the record with least rowid
    value be "smaller", so index scans return records ordered by (keytuple,
    rowid). This is used by index_merge access method, grep for ROR in
    sql/opt_range.cc for details.
  */
  return (ma->file_offset < mb->file_offset)? -1 : (ma->file_offset > 
                                                    mb->file_offset) ? 1 : 0;
} /* queue_key_cmp */


int _myrg_init_queue(MYRG_INFO *info,int inx,enum ha_rkey_function search_flag)
{
  int error=0;
  QUEUE *q= &(info->by_key);

  if (inx < (int) info->keys)
  {
    if (!is_queue_inited(q))
    {
      if (init_queue(q,info->tables, 0,
		     (myisam_readnext_vec[search_flag] == SEARCH_SMALLER),
		     queue_key_cmp,
		     info->open_tables->table->s->keyinfo[inx].seg))
	error=my_errno;
    }
    else
    {
      if (reinit_queue(q,info->tables, 0,
		       (myisam_readnext_vec[search_flag] == SEARCH_SMALLER),
		       queue_key_cmp,
		       info->open_tables->table->s->keyinfo[inx].seg))
	error=my_errno;
    }
  }
  else
    my_errno= error= HA_ERR_WRONG_INDEX;
  return error;
}

int _myrg_mi_read_record(MI_INFO *info, byte *buf)
{
  if (!(*info->read_record)(info,info->lastpos,buf))
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    return 0;
  }
  return my_errno;
}
