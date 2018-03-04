/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/myisam/myisamdef.h"
#include "storage/myisam/rt_index.h"

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
  DBUG_ENTER("mi_rnext_same");

  if ((int) (inx=info->lastinx) < 0 || info->lastpos == HA_OFFSET_ERROR)
  {
    set_my_errno(HA_ERR_WRONG_INDEX);
    DBUG_RETURN(HA_ERR_WRONG_INDEX);
  }
  keyinfo=info->s->keyinfo+inx;
  if (fast_mi_readinfo(info))
    DBUG_RETURN(my_errno());

  if (info->s->concurrent_insert)
    mysql_rwlock_rdlock(&info->s->key_root_lock[inx]);

  switch (keyinfo->key_alg)
  {
    case HA_KEY_ALG_RTREE:
      if ((error=rtree_find_next(info,inx,
				 myisam_read_vec[info->last_key_func])))
      {
	error=1;
	set_my_errno(HA_ERR_END_OF_FILE);
	info->lastpos= HA_OFFSET_ERROR;
	break;
      }
      break;
    case HA_KEY_ALG_BTREE:
    default:

      if (info->set_rnext_same_key)
      {
        /* First rnext_same and lastkey is filled in mi_rkey */
        memcpy(info->rnext_same_key, info->lastkey, info->last_rkey_length);
        info->set_rnext_same_key= FALSE;
      }
      for (;;)
      {
        if ((error=_mi_search_next(info,keyinfo,info->lastkey,
			       info->lastkey_length,SEARCH_BIGGER,
			       info->s->state.key_root[inx])))
          break;
        if (ha_key_cmp(keyinfo->seg, info->lastkey, info->rnext_same_key,
                       info->last_rkey_length, SEARCH_FIND, not_used))
        {
          error=1;
          set_my_errno(HA_ERR_END_OF_FILE);
          info->lastpos= HA_OFFSET_ERROR;
          break;
        }
        /* Skip rows that are inserted by other threads since we got a lock */
        if (info->lastpos < info->state->data_file_length && 
            (!info->index_cond_func || mi_check_index_cond(info, inx, buf)))
          break;
      }
  }
  if (info->s->concurrent_insert)
    mysql_rwlock_unlock(&info->s->key_root_lock[inx]);
	/* Don't clear if database-changed */
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  info->update|= HA_STATE_NEXT_FOUND;

  if (error)
  {
    if (my_errno() == HA_ERR_KEY_NOT_FOUND)
      set_my_errno(HA_ERR_END_OF_FILE);
  }
  else if (!buf)
  {
    DBUG_RETURN(info->lastpos==HA_OFFSET_ERROR ? my_errno() : 0);
  }
  else if (!(*info->read_record)(info,info->lastpos,buf))
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    DBUG_RETURN(0);
  }
  DBUG_RETURN(my_errno());
} /* mi_rnext_same */
