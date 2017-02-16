/* Copyright (c) 2000-2007 MySQL AB, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

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
	   Read next row with the same key as previous read, but abort if
	   the key changes.
	   One may have done a write, update or delete of the previous row.
	   NOTE! Even if one changes the previous row, the next read is done
	   based on the position of the last used key!
	*/

int mi_rnext_same(MI_INFO *info, uchar *buf)
{
  int error;
  uint inx,not_used[2];
  MI_KEYDEF *keyinfo;
  ICP_RESULT icp_res= ICP_MATCH;
  DBUG_ENTER("mi_rnext_same");

  if ((int) (inx=info->lastinx) < 0 || info->lastpos == HA_OFFSET_ERROR)
    DBUG_RETURN(my_errno=HA_ERR_WRONG_INDEX);
  keyinfo=info->s->keyinfo+inx;
  if (fast_mi_readinfo(info))
    DBUG_RETURN(my_errno);

  if (info->s->concurrent_insert)
    mysql_rwlock_rdlock(&info->s->key_root_lock[inx]);

  switch (keyinfo->key_alg)
  {
#ifdef HAVE_RTREE_KEYS
    case HA_KEY_ALG_RTREE:
      if ((error=rtree_find_next(info,inx,
				 myisam_read_vec[info->last_key_func])))
      {
	error=1;
	my_errno=HA_ERR_END_OF_FILE;
	info->lastpos= HA_OFFSET_ERROR;
	break;
      }
      break;
#endif
    case HA_KEY_ALG_BTREE:
    default:
      if (!(info->update & HA_STATE_RNEXT_SAME))
      {
        /* First rnext_same; Store old key */
        memcpy(info->lastkey2,info->lastkey,info->last_rkey_length);
      }
      for (;;)
      {
        /*
          If we are at the last key on the key page, allow writers to
          access the index.
        */
        if (info->int_keypos >= info->int_maxpos &&
            mi_yield_and_check_if_killed(info, inx))
        {
          error=1;
          break;
        }

        if ((error=_mi_search_next(info,keyinfo,info->lastkey,
			       info->lastkey_length,SEARCH_BIGGER,
			       info->s->state.key_root[inx])))
          break;
        if (ha_key_cmp(keyinfo->seg, info->lastkey, info->lastkey2,
                       info->last_rkey_length, SEARCH_FIND, not_used))
        {
          error=1;
          my_errno=HA_ERR_END_OF_FILE;
          info->lastpos= HA_OFFSET_ERROR;
          break;
        }
        /* 
          Skip 
           - rows that are inserted by other threads since we got a lock 
           - rows that don't match index condition
        */
        if (info->lastpos < info->state->data_file_length && 
            (!info->index_cond_func || 
             (icp_res= mi_check_index_cond(info, inx, buf)) != ICP_NO_MATCH))
          break;
      }
  }
  if (info->s->concurrent_insert)
    mysql_rwlock_unlock(&info->s->key_root_lock[inx]);


	/* Don't clear if database-changed */
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  info->update|= HA_STATE_NEXT_FOUND | HA_STATE_RNEXT_SAME;

  if (error || icp_res != ICP_MATCH)
  {
    fast_mi_writeinfo(info);
    if (my_errno == HA_ERR_KEY_NOT_FOUND)
      my_errno=HA_ERR_END_OF_FILE;
  }
  else if (!buf)
  {
    fast_mi_writeinfo(info);
    DBUG_RETURN(info->lastpos==HA_OFFSET_ERROR ? my_errno : 0);
  }
  else if (!(*info->read_record)(info,info->lastpos,buf))
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    DBUG_RETURN(0);
  }
  DBUG_RETURN(my_errno);
} /* mi_rnext_same */
