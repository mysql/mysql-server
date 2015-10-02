/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

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
	** Find current row with read on position or read on key
	** If inx >= 0 find record using key
	** Return values:
	** 0 = Ok.
	** HA_ERR_KEY_NOT_FOUND = Row is deleted
	** HA_ERR_END_OF_FILE   = End of file
	*/


int mi_rsame(MI_INFO *info, uchar *record, int inx)
{
  DBUG_ENTER("mi_rsame");

  if (inx != -1 && ! mi_is_key_active(info->s->state.key_map, inx))
  {
    set_my_errno(HA_ERR_WRONG_INDEX);
    DBUG_RETURN(HA_ERR_WRONG_INDEX);
  }
  if (info->lastpos == HA_OFFSET_ERROR || info->update & HA_STATE_DELETED)
  {
    set_my_errno(HA_ERR_KEY_NOT_FOUND);
    DBUG_RETURN(HA_ERR_KEY_NOT_FOUND);	/* No current record */
  }
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

  /* Read row from data file */
  if (fast_mi_readinfo(info))
    DBUG_RETURN(my_errno());

  if (inx >= 0)
  {
    info->lastinx=inx;
    info->lastkey_length=_mi_make_key(info,(uint) inx,info->lastkey,record,
				      info->lastpos);
    if (info->s->concurrent_insert)
      mysql_rwlock_rdlock(&info->s->key_root_lock[inx]);
    (void) _mi_search(info,info->s->keyinfo+inx,info->lastkey, USE_WHOLE_KEY,
		    SEARCH_SAME,
		    info->s->state.key_root[inx]);
    if (info->s->concurrent_insert)
      mysql_rwlock_unlock(&info->s->key_root_lock[inx]);
  }

  if (!(*info->read_record)(info,info->lastpos,record))
    DBUG_RETURN(0);
  if (my_errno() == HA_ERR_RECORD_DELETED)
    set_my_errno(HA_ERR_KEY_NOT_FOUND);
  DBUG_RETURN(my_errno());
} /* mi_rsame */
