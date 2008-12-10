/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "maria_def.h"

	/*
	   Read previous row with the same key as previous read
	   One may have done a write, update or delete of the previous row.
	   NOTE! Even if one changes the previous row, the next read is done
	   based on the position of the last used key!
	*/

int maria_rprev(MARIA_HA *info, uchar *buf, int inx)
{
  int error,changed;
  register uint flag;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo;
  DBUG_ENTER("maria_rprev");

  if ((inx = _ma_check_index(info,inx)) < 0)
    DBUG_RETURN(my_errno);
  flag=SEARCH_SMALLER;				/* Read previous */
  if (info->cur_row.lastpos == HA_OFFSET_ERROR &&
      info->update & HA_STATE_NEXT_FOUND)
    flag=0;					/* Read last */

  if (fast_ma_readinfo(info))
    DBUG_RETURN(my_errno);
  keyinfo= share->keyinfo + inx;
  changed= _ma_test_if_changed(info);
  if (share->lock_key_trees)
    rw_rdlock(&keyinfo->root_lock);
  if (!flag)
    error= _ma_search_last(info, keyinfo, share->state.key_root[inx]);
  else if (!changed)
    error= _ma_search_next(info, &info->last_key,
                           flag | info->last_key.flag,
                           share->state.key_root[inx]);
  else
    error= _ma_search(info, &info->last_key, flag | info->last_key.flag,
                      share->state.key_root[inx]);

  if (!error)
  {
    while (!(*share->row_is_visible)(info))
    {
      /* Skip rows that are inserted by other threads since we got a lock */
      if  ((error= _ma_search_next(info, &info->last_key,
                                   SEARCH_SMALLER,
                                   share->state.key_root[inx])))
        break;
    }
  }
  if (share->lock_key_trees)
    rw_unlock(&keyinfo->root_lock);
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  info->update|= HA_STATE_PREV_FOUND;
  if (error)
  {
    if (my_errno == HA_ERR_KEY_NOT_FOUND)
      my_errno=HA_ERR_END_OF_FILE;
  }
  else if (!buf)
  {
    DBUG_RETURN(info->cur_row.lastpos == HA_OFFSET_ERROR ? my_errno : 0);
  }
  else if (!(*info->read_record)(info, buf, info->cur_row.lastpos))
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    DBUG_RETURN(0);
  }
  DBUG_RETURN(my_errno);
} /* maria_rprev */
