/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "myisamdef.h"

	/*
	** Find current row with read on position or read on key
	** If inx >= 0 find record using key
	** Return values:
	** 0 = Ok.
	** HA_ERR_KEY_NOT_FOUND = Row is deleted
	** HA_ERR_END_OF_FILE   = End of file
	*/


int mi_rsame(MI_INFO *info, byte *record, int inx)
{
  DBUG_ENTER("mi_rsame");

  if (inx != -1 && ! (((ulonglong) 1 << inx) & info->s->state.key_map))
  {
    DBUG_RETURN(my_errno=HA_ERR_WRONG_INDEX);
  }
  if (info->lastpos == HA_OFFSET_ERROR || info->update & HA_STATE_DELETED)
  {
    DBUG_RETURN(my_errno=HA_ERR_KEY_NOT_FOUND);	/* No current record */
  }
  info->update&= (HA_STATE_CHANGED | HA_STATE_ROW_CHANGED);

  /* Read row from data file */
  if (fast_mi_readinfo(info))
    DBUG_RETURN(my_errno);

  if (inx >= 0)
  {
    info->lastinx=inx;
    info->lastkey_length=_mi_make_key(info,(uint) inx,info->lastkey,record,
				      info->lastpos);
    if (info->s->concurrent_insert)
      rw_rdlock(&info->s->key_root_lock[inx]);
    VOID(_mi_search(info,info->s->keyinfo+inx,info->lastkey, USE_WHOLE_KEY,
		    SEARCH_SAME,
		    info->s->state.key_root[inx]));
    if (info->s->concurrent_insert)
      rw_unlock(&info->s->key_root_lock[inx]);
  }

  if (!(*info->read_record)(info,info->lastpos,record))
    DBUG_RETURN(0);
  if (my_errno == HA_ERR_RECORD_DELETED)
    my_errno=HA_ERR_KEY_NOT_FOUND;
  DBUG_RETURN(my_errno);
} /* mi_rsame */
