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
	   Read next row with the same key as previous read
	   One may have done a write, update or delete of the previous row.
	   NOTE! Even if one changes the previous row, the next read is done
	   based on the position of the last used key!
	*/

int mi_rnext(MI_INFO *info, byte *buf, int inx)
{
  int error,changed;
  uint flag;
  DBUG_ENTER("mi_rnext");

  if ((inx = _mi_check_index(info,inx)) < 0)
    DBUG_RETURN(my_errno);
  flag=SEARCH_BIGGER;				/* Read next */
  if (info->lastpos == HA_OFFSET_ERROR && info->update & HA_STATE_PREV_FOUND)
    flag=0;					/* Read first */

  if (fast_mi_readinfo(info))
    DBUG_RETURN(my_errno);
  if (info->s->concurrent_insert)
    rw_rdlock(&info->s->key_root_lock[inx]);
  changed=_mi_test_if_changed(info);
  if (!flag)
  {
    error=_mi_search_first(info,info->s->keyinfo+inx,
			   info->s->state.key_root[inx]);
  }
  else if (!changed)
    error=_mi_search_next(info,info->s->keyinfo+inx,info->lastkey,
			  info->lastkey_length,flag,
			  info->s->state.key_root[inx]);
  else
    error=_mi_search(info,info->s->keyinfo+inx,info->lastkey,
		     USE_WHOLE_KEY,flag, info->s->state.key_root[inx]);

  if (!error)
  {
    while (info->lastpos >= info->state->data_file_length)
    {
      /* Skip rows that are inserted by other threads since we got a lock */
      if  ((error=_mi_search_next(info,info->s->keyinfo+inx,info->lastkey,
				  info->lastkey_length,
				  SEARCH_BIGGER,
				  info->s->state.key_root[inx])))
	break;
    }
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
  else if (!buf)
  {
    DBUG_RETURN(info->lastpos==HA_OFFSET_ERROR ? my_errno : 0);
  }
  else if (!(*info->read_record)(info,info->lastpos,buf))
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    DBUG_RETURN(0);
  }
  DBUG_PRINT("error",("Got error: %d,  errno: %d",error, my_errno));
  DBUG_RETURN(my_errno);
} /* mi_rnext */
