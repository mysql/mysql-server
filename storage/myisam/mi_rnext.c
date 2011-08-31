/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "myisamdef.h"

#include "rt_index.h"

	/*
	   Read next row with the same key as previous read
	   One may have done a write, update or delete of the previous row.
	   NOTE! Even if one changes the previous row, the next read is done
	   based on the position of the last used key!
	*/

int mi_rnext(MI_INFO *info, uchar *buf, int inx)
{
  int error,changed;
  uint flag;
  uint update_mask= HA_STATE_NEXT_FOUND;
  DBUG_ENTER("mi_rnext");

  if ((inx = _mi_check_index(info,inx)) < 0)
    DBUG_RETURN(my_errno);
  flag=SEARCH_BIGGER;				/* Read next */
  if (info->lastpos == HA_OFFSET_ERROR && info->update & HA_STATE_PREV_FOUND)
    flag=0;					/* Read first */

  if (fast_mi_readinfo(info))
    DBUG_RETURN(my_errno);
  if (info->s->concurrent_insert)
    mysql_rwlock_rdlock(&info->s->key_root_lock[inx]);
  changed=_mi_test_if_changed(info);
  if (!flag)
  {
    switch(info->s->keyinfo[inx].key_alg){
#ifdef HAVE_RTREE_KEYS
    case HA_KEY_ALG_RTREE:
      error=rtree_get_first(info,inx,info->lastkey_length);
      break;
#endif
    case HA_KEY_ALG_BTREE:
    default:
      error=_mi_search_first(info,info->s->keyinfo+inx,
			   info->s->state.key_root[inx]);
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
    switch (info->s->keyinfo[inx].key_alg) {
#ifdef HAVE_RTREE_KEYS
    case HA_KEY_ALG_RTREE:
      /*
	Note that rtree doesn't support that the table
	may be changed since last call, so we do need
	to skip rows inserted by other threads like in btree
      */
      error= rtree_get_next(info,inx,info->lastkey_length);
      break;
#endif
    case HA_KEY_ALG_BTREE:
    default:
      if (!changed)
	error= _mi_search_next(info,info->s->keyinfo+inx,info->lastkey,
			       info->lastkey_length,flag,
			       info->s->state.key_root[inx]);
      else
	error= _mi_search(info,info->s->keyinfo+inx,info->lastkey,
			  USE_WHOLE_KEY,flag, info->s->state.key_root[inx]);
    }
  }

  if (info->s->concurrent_insert)
  {
    if (!error)
    {
      while (info->lastpos >= info->state->data_file_length)
      {
	/* Skip rows inserted by other threads since we got a lock */
	if  ((error=_mi_search_next(info,info->s->keyinfo+inx,
				    info->lastkey,
				    info->lastkey_length,
				    SEARCH_BIGGER,
				    info->s->state.key_root[inx])))
	  break;
      }
    }
    mysql_rwlock_unlock(&info->s->key_root_lock[inx]);
  }
	/* Don't clear if database-changed */
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  info->update|= update_mask;

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
