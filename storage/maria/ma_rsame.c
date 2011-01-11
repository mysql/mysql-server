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

/**
  Find current row with read on position or read on key

  @notes
  If inx >= 0 find record using key else re-read row on last position

  @warning
  This function is not row version safe.
  This is not crtical as this function is not used by MySQL

  @return
  @retval 0                      Ok
  @retval HA_ERR_KEY_NOT_FOUND   Row is deleted
  @retval HA_ERR_END_OF_FILE     End of file
  @retval HA_ERR_WRONG_INDEX	 Wrong inx argument
*/


int maria_rsame(MARIA_HA *info, uchar *record, int inx)
{
  DBUG_ENTER("maria_rsame");

  if (inx >= 0 && _ma_check_index(info, inx) < 0)
  {
    DBUG_PRINT("error", ("wrong index usage"));
    DBUG_RETURN(my_errno);
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
    MARIA_KEYDEF *keyinfo= info->last_key.keyinfo;
    (*keyinfo->make_key)(info, &info->last_key, (uint) inx,
                         info->lastkey_buff, record,
                         info->cur_row.lastpos,
                         info->cur_row.trid);
    if (info->s->lock_key_trees)
      rw_rdlock(&keyinfo->root_lock);
    VOID(_ma_search(info, &info->last_key, SEARCH_SAME,
		    info->s->state.key_root[inx]));
    if (info->s->lock_key_trees)
      rw_unlock(&keyinfo->root_lock);
  }

  if (!(*info->read_record)(info, record, info->cur_row.lastpos))
    DBUG_RETURN(0);
  if (my_errno == HA_ERR_RECORD_DELETED)
    my_errno=HA_ERR_KEY_NOT_FOUND;
  DBUG_PRINT("error", ("my_errno: %d", my_errno));
  DBUG_RETURN(my_errno);
} /* maria_rsame */
