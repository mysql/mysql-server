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

#include "myisamdef.h"

	/*
	   Read next row with the same key as previous read, but abort if
	   the key changes.
	   One may have done a write, update or delete of the previous row.
	   NOTE! Even if one changes the previous row, the next read is done
	   based on the position of the last used key!
	*/

int mi_rnext_same(MI_INFO *info, byte *buf)
{
  int error;
  uint inx,flag,not_used;
  MI_KEYDEF *keyinfo;
  DBUG_ENTER("mi_rnext_same");

  if ((int) (inx=info->lastinx) < 0 || info->lastpos == HA_OFFSET_ERROR)
    DBUG_RETURN(my_errno=HA_ERR_WRONG_INDEX);
  keyinfo=info->s->keyinfo+inx;
  flag=SEARCH_BIGGER;				/* Read next */
  if (fast_mi_readinfo(info))
    DBUG_RETURN(my_errno);

  memcpy(info->lastkey2,info->lastkey,info->last_rkey_length);
  if (info->s->concurrent_insert)
    rw_rdlock(&info->s->key_root_lock[inx]);
  for (;;)
  {
    if ((error=_mi_search_next(info,keyinfo,info->lastkey,
			       info->lastkey_length,flag,
			       info->s->state.key_root[inx])))
      break;
    if (_mi_key_cmp(keyinfo->seg,info->lastkey2,info->lastkey,
		    info->last_rkey_length, SEARCH_FIND, &not_used))
    {
      error=1;
      my_errno=HA_ERR_END_OF_FILE;
      info->lastpos= HA_OFFSET_ERROR;
      break;
    }
    /* Skip rows that are inserted by other threads since we got a lock */
    if (info->lastpos < info->state->data_file_length)
      break;
  }
  if (info->s->concurrent_insert)
    rw_unlock(&info->s->key_root_lock[inx]);
	/* Don't clear if database-changed */
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  info->update|= HA_STATE_NEXT_FOUND;

  if (error)
  {
    if (my_errno == HA_ERR_KEY_NOT_FOUND)
      my_errno=HA_ERR_END_OF_FILE;
  }
  else if (!(*info->read_record)(info,info->lastpos,buf))
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    DBUG_RETURN(0);
  }
  DBUG_RETURN(my_errno);
} /* mi_rnext */
