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
#include "ma_rt_index.h"

/*
  Read next row with the same key as previous read, but abort if
  the key changes.
  One may have done a write, update or delete of the previous row.

  NOTE! Even if one changes the previous row, the next read is done
  based on the position of the last used key!
*/

int maria_rnext_same(MARIA_HA *info, uchar *buf)
{
  int error;
  uint inx,not_used[2];
  MARIA_KEYDEF *keyinfo;
  ICP_RESULT icp_res= ICP_MATCH;
  DBUG_ENTER("maria_rnext_same");

  if ((int) (inx= info->lastinx) < 0 ||
      info->cur_row.lastpos == HA_OFFSET_ERROR)
    DBUG_RETURN(my_errno=HA_ERR_WRONG_INDEX);
  if (fast_ma_readinfo(info))
    DBUG_RETURN(my_errno);

  keyinfo= info->s->keyinfo+inx;
  if (info->s->lock_key_trees)
    mysql_rwlock_rdlock(&keyinfo->root_lock);

  switch (keyinfo->key_alg) {
#ifdef HAVE_RTREE_KEYS
    case HA_KEY_ALG_RTREE:
      if ((error=maria_rtree_find_next(info,inx,
				 maria_read_vec[info->last_key_func])))
      {
	error=1;
	my_errno=HA_ERR_END_OF_FILE;
	info->cur_row.lastpos= HA_OFFSET_ERROR;
	break;
      }
      break;
#endif
    case HA_KEY_ALG_BTREE:
    default:
      if (!(info->update & HA_STATE_RNEXT_SAME))
      {
        /* First rnext_same; Store old key */
        memcpy(info->lastkey_buff2, info->last_key.data,
               info->last_rkey_length);
      }
      for (;;)
      {
        if ((error= _ma_search_next(info, &info->last_key,
                                    SEARCH_BIGGER,
                                    info->s->state.key_root[inx])))
          break;
        if (ha_key_cmp(keyinfo->seg, info->last_key.data,
                       info->lastkey_buff2,
                       info->last_rkey_length, SEARCH_FIND,
                       not_used))
        {
          error=1;
          my_errno=HA_ERR_END_OF_FILE;
          info->cur_row.lastpos= HA_OFFSET_ERROR;
          break;
        }
        /*
          If we are at the last key on the key page, allow writers to
          access the index.
        */
        if (info->int_keypos >= info->int_maxpos &&
            ma_yield_and_check_if_killed(info, inx))
        {
          error= 1;
          break;
        }
        /* Skip rows that are inserted by other threads since we got a lock */
        if ((info->s->row_is_visible)(info) &&
            ((icp_res= ma_check_index_cond(info, inx, buf)) != ICP_NO_MATCH))
          break;
      }
  }
  if (info->s->lock_key_trees)
    mysql_rwlock_unlock(&keyinfo->root_lock);
	/* Don't clear if database-changed */
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  info->update|= HA_STATE_NEXT_FOUND | HA_STATE_RNEXT_SAME;

  if (error || icp_res != ICP_MATCH)
  {
    fast_ma_writeinfo(info);
    if (my_errno == HA_ERR_KEY_NOT_FOUND)
      my_errno= HA_ERR_END_OF_FILE;
  }
  else if (!buf)
  {
    fast_ma_writeinfo(info);
    DBUG_RETURN(info->cur_row.lastpos == HA_OFFSET_ERROR ? my_errno : 0);
  }
  else if (!(*info->read_record)(info, buf, info->cur_row.lastpos))
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    DBUG_RETURN(0);
  }
  DBUG_RETURN(my_errno);
} /* maria_rnext_same */
