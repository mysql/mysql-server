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

/* Read record based on a key */

#include "mymrgdef.h"

static int queue_key_cmp(void *keyseg, byte *a, byte *b)
{
  MI_INFO *aa=((MYRG_TABLE *)a)->table;
  MI_INFO *bb=((MYRG_TABLE *)b)->table;
  uint not_used;
  int ret= _mi_key_cmp((MI_KEYSEG *)keyseg, aa->lastkey, bb->lastkey,
		       USE_WHOLE_KEY, SEARCH_FIND, &not_used);
  return ret < 0 ? -1 : ret > 0 ? 1 : 0;
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
  return error;
}
