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
	   Read next row with the same key as previous read
	   One may have done a write, update or delete of the previous row.
	   NOTE! Even if one changes the previous row, the next read is done
	   based on the position of the last used key!
	*/

int maria_rnext(MARIA_HA *info, uchar *buf, int inx)
{
  int error,changed;
  uint flag;
  MARIA_SHARE *share= info->s;
  MARIA_KEYDEF *keyinfo;
  ICP_RESULT icp_res= ICP_MATCH;
  uint update_mask= HA_STATE_NEXT_FOUND;
  DBUG_ENTER("maria_rnext");

  if ((inx = _ma_check_index(info,inx)) < 0)
    DBUG_RETURN(my_errno);
  flag=SEARCH_BIGGER;				/* Read next */
  if (info->cur_row.lastpos == HA_OFFSET_ERROR &&
      info->update & HA_STATE_PREV_FOUND)
    flag=0;					/* Read first */

  if (fast_ma_readinfo(info))
    DBUG_RETURN(my_errno);
  keyinfo= share->keyinfo + inx;
  if (share->lock_key_trees)
    mysql_rwlock_rdlock(&keyinfo->root_lock);
  changed= _ma_test_if_changed(info);
  if (!flag)
  {
    switch (keyinfo->key_alg){
#ifdef HAVE_RTREE_KEYS
    case HA_KEY_ALG_RTREE:
      error=maria_rtree_get_first(info, inx,
                                  info->last_key.data_length +
                                  info->last_key.ref_length);
                                  
      break;
#endif
    case HA_KEY_ALG_BTREE:
    default:
      error= _ma_search_first(info, keyinfo, share->state.key_root[inx]);
      break;
    }
    /*
      "search first" failed. This means we have no pivot for
      "search next", or in other words MI_INFO::lastkey is
      likely uninitialized.

      Normally SQL layer would never request "search next" if
      "search first" failed. But HANDLER may do anything.

      As mi_rnext() without preceeding mi_rkey()/mi_rfirst()
      equals to mi_rfirst(), we must restore original state
      as if failing mi_rfirst() was not called.
    */
    if (error)
      update_mask|= HA_STATE_PREV_FOUND;
  }
  else
  {
    switch (keyinfo->key_alg) {
#ifdef HAVE_RTREE_KEYS
    case HA_KEY_ALG_RTREE:
      /*
	Note that rtree doesn't support that the table
	may be changed since last call, so we do need
	to skip rows inserted by other threads like in btree
      */
      error= maria_rtree_get_next(info, inx, info->last_key.data_length +
                                  info->last_key.ref_length);
      break;
#endif
    case HA_KEY_ALG_BTREE:
    default:
      if (!changed)
	error= _ma_search_next(info, &info->last_key,
                               flag | info->last_key.flag,
			       share->state.key_root[inx]);
      else
	error= _ma_search(info, &info->last_key, flag | info->last_key.flag,
                          share->state.key_root[inx]);
    }
  }

  if (!error)
  {
    while (!(*share->row_is_visible)(info) ||
           ((icp_res= ma_check_index_cond(info, inx, buf)) == ICP_NO_MATCH))
    {
      /*
        If we are at the last key on the key page, allow writers to
        access the index.
      */
      if (info->int_keypos >= info->int_maxpos &&
          ma_yield_and_check_if_killed(info, inx))
      {
        /* my_errno is set by ma_yield_and_check_if_killed() */
        error= 1;
        break;
      }

      /* Skip rows inserted by other threads since we got a lock */
      if  ((error= _ma_search_next(info, &info->last_key,
                                   SEARCH_BIGGER,
                                   share->state.key_root[inx])))
        break;
    }
  }
  if (share->lock_key_trees)
    mysql_rwlock_unlock(&keyinfo->root_lock);

	/* Don't clear if database-changed */
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  info->update|= update_mask;
  
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
  DBUG_PRINT("error",("Got error: %d,  errno: %d",error, my_errno));
  DBUG_RETURN(my_errno);
} /* maria_rnext */
