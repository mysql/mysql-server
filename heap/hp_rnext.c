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

#include "heapdef.h"

/* Read next record with the same key */

int heap_rnext(HP_INFO *info, byte *record)
{
  byte *pos;
  HP_SHARE *share=info->s;
  DBUG_ENTER("heap_rnext");

  if (info->lastinx < 0)
    DBUG_RETURN(my_errno=HA_ERR_WRONG_INDEX);

  if (info->current_hash_ptr)
    pos= _hp_search_next(info,share->keydef+info->lastinx, info->lastkey,
			 info->current_hash_ptr);
  else
  {
    if (!info->current_ptr && (info->update & HA_STATE_NEXT_FOUND))
    {
      pos=0;					/* Read next after last */
      my_errno=HA_ERR_KEY_NOT_FOUND;
    }
    else if (!info->current_ptr && (info->update & HA_STATE_PREV_FOUND))
      pos= _hp_search(info,share->keydef+info->lastinx, info->lastkey, 0);
    else
      pos= _hp_search(info,share->keydef+info->lastinx, info->lastkey, 1);
  }

  if (!pos)
  {
    info->update=HA_STATE_NEXT_FOUND;		/* For heap_rprev */
    if (my_errno == HA_ERR_KEY_NOT_FOUND)
      my_errno=HA_ERR_END_OF_FILE;
    DBUG_RETURN(my_errno);
  }
  memcpy(record,pos,(size_t) share->reclength);
  info->update=HA_STATE_AKTIV | HA_STATE_NEXT_FOUND;
  DBUG_RETURN(0);
}
