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
  Find current row with read on position or read on key

  NOTES
    If inx >= 0 find record using key

  RETURN
    0                      Ok
    HA_ERR_KEY_NOT_FOUND   Row is deleted
    HA_ERR_END_OF_FILE     End of file
*/


int maria_rsame(MARIA_HA *info, uchar *record, int inx)
{
  DBUG_ENTER("maria_rsame");

  if (inx != -1 && ! maria_is_key_active(info->s->state.key_map, inx))
  {
    DBUG_PRINT("error", ("wrong index usage"));
    DBUG_RETURN(my_errno=HA_ERR_WRONG_INDEX);
  }
  if (info->cur_row.lastpos == HA_OFFSET_ERROR ||
      info->update & HA_STATE_DELETED)
  {
    DBUG_PRINT("error", ("no current record"));
    DBUG_RETURN(my_errno=HA_ERR_KEY_NOT_FOUND);	/* No current record */
  }
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

  /* Read row from data file */
  if (fast_ma_readinfo(info))
    DBUG_RETURN(my_errno);

  if (inx >= 0)
  {
    info->lastinx=inx;
    info->lastkey_length= _ma_make_key(info,(uint) inx,info->lastkey,record,
				      info->cur_row.lastpos);
    if (info->s->lock_key_trees)
      rw_rdlock(&info->s->key_root_lock[inx]);
    VOID(_ma_search(info,info->s->keyinfo+inx,info->lastkey, USE_WHOLE_KEY,
		    SEARCH_SAME,
		    info->s->state.key_root[inx]));
    if (info->s->lock_key_trees)
      rw_unlock(&info->s->key_root_lock[inx]);
  }

  if (!(*info->read_record)(info, record, info->cur_row.lastpos))
    DBUG_RETURN(0);
  if (my_errno == HA_ERR_RECORD_DELETED)
    my_errno=HA_ERR_KEY_NOT_FOUND;
  DBUG_PRINT("error", ("my_errno: %d", my_errno));
  DBUG_RETURN(my_errno);
} /* maria_rsame */
