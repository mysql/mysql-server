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

/* read record through position and fix key-position */
/* As maria_rsame but supply a position */

#include "maria_def.h"


/*
  Read row based on postion

  @param inx      If inx >= 0 postion the given index on found row

  @return
  @retval  0                    Ok
  @retval HA_ERR_KEY_NOT_FOUND  Row is deleted
  @retval HA_ERR_END_OF_FILE   End of file
*/

int maria_rsame_with_pos(MARIA_HA *info, uchar *record, int inx,
                         MARIA_RECORD_POS filepos)
{
  DBUG_ENTER("maria_rsame_with_pos");
  DBUG_PRINT("enter",("index: %d  filepos: %ld", inx, (long) filepos));

  if (inx < -1 ||
      (inx >= 0 && ! maria_is_key_active(info->s->state.key_map, inx)))
  {
    DBUG_RETURN(my_errno=HA_ERR_WRONG_INDEX);
  }

  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);
  if ((*info->s->read_record)(info, record, filepos))
  {
    if (my_errno == HA_ERR_RECORD_DELETED)
      my_errno=HA_ERR_KEY_NOT_FOUND;
    DBUG_RETURN(my_errno);
  }
  info->cur_row.lastpos= filepos;
  info->lastinx= inx;
  if (inx >= 0)
  {
    (*info->s->keyinfo[inx].make_key)(info, &info->last_key, (uint) inx,
                                      info->lastkey_buff,
                                      record, info->cur_row.lastpos,
                                      info->cur_row.trid);
    info->update|=HA_STATE_KEY_CHANGED;		/* Don't use indexposition */
  }
  DBUG_RETURN(0);
} /* maria_rsame_pos */
