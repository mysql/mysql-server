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

int heap_rkey(HP_INFO *info, byte *record, int inx, const byte *key)
{
  byte *pos;
  HP_SHARE *share=info->s;
  DBUG_ENTER("hp_rkey");
  DBUG_PRINT("enter",("base: %lx  inx: %d",info,inx));

  if ((uint) inx >= share->keys)
  {
    DBUG_RETURN(my_errno=HA_ERR_WRONG_INDEX);
  }
  info->lastinx=inx;
  info->current_record = (ulong) ~0L;		/* For heap_rrnd() */

  if (!(pos=_hp_search(info,share->keydef+inx,key,0)))
  {
    info->update=0;
    DBUG_RETURN(my_errno);
  }
  memcpy(record,pos,(size_t) share->reclength);
  info->update=HA_STATE_AKTIV;
  if (!(share->keydef[inx].flag & HA_NOSAME))
    memcpy(info->lastkey,key,(size_t) share->keydef[inx].length);
  DBUG_RETURN(0);
}


	/* Quick find of record */

gptr heap_find(HP_INFO *info, int inx, const byte *key)
{
  return _hp_search(info,info->s->keydef+inx,key,0);
}
