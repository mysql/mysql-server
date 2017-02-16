/* Copyright (c) 2000, 2001, 2004-2007 MySQL AB, 2009 Sun Microsystems, Inc.
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

	/*
	   Read previous row with the same key as previous read
	   One may have done a write, update or delete of the previous row.
	   NOTE! Even if one changes the previous row, the next read is done
	   based on the position of the last used key!
	*/

int mi_rprev(MI_INFO *info, uchar *buf, int inx)
{
  int error,changed;
  register uint flag;
  MYISAM_SHARE *share=info->s;
  ICP_RESULT icp_res= ICP_MATCH;
  DBUG_ENTER("mi_rprev");

  if ((inx = _mi_check_index(info,inx)) < 0)
    DBUG_RETURN(my_errno);
  flag=SEARCH_SMALLER;				/* Read previous */
  if (info->lastpos == HA_OFFSET_ERROR && info->update & HA_STATE_NEXT_FOUND)
    flag=0;					/* Read last */

  if (fast_mi_readinfo(info))
    DBUG_RETURN(my_errno);
  changed=_mi_test_if_changed(info);
  if (share->concurrent_insert)
    mysql_rwlock_rdlock(&share->key_root_lock[inx]);
  if (!flag)
    error=_mi_search_last(info, share->keyinfo+inx,
			  share->state.key_root[inx]);
  else if (!changed)
    error=_mi_search_next(info,share->keyinfo+inx,info->lastkey,
			  info->lastkey_length,flag,
			  share->state.key_root[inx]);
  else
    error=_mi_search(info,share->keyinfo+inx,info->lastkey,
		     USE_WHOLE_KEY, flag, share->state.key_root[inx]);

  if (!error)
  {
    my_off_t cur_keypage= info->last_keypage;
    while ((share->concurrent_insert && 
            info->lastpos >= info->state->data_file_length) ||
           (info->index_cond_func &&
            (icp_res= mi_check_index_cond(info, inx, buf)) == ICP_NO_MATCH))
    {
      /*
        If we are at the last (i.e. first?) key on the key page, 
        allow writers to access the index.
      */
      if (info->last_keypage != cur_keypage)
      {
        cur_keypage= info->last_keypage;
        if (mi_yield_and_check_if_killed(info, inx))
        {
          error= 1;
          break;
        }
      }

      /* 
         Skip rows that are either inserted by other threads since
         we got a lock or do not match pushed index conditions
      */
      if  ((error=_mi_search_next(info,share->keyinfo+inx,info->lastkey,
                                  info->lastkey_length,
                                  SEARCH_SMALLER,
                                  share->state.key_root[inx])))
        break;
    }
  }

  if (share->concurrent_insert)
    mysql_rwlock_unlock(&share->key_root_lock[inx]);

  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  info->update|= HA_STATE_PREV_FOUND;

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
} /* mi_rprev */
