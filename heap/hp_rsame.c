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

/* re-read current record */

#include "heapdef.h"

	/* If inx != -1 the new record is read according to index
	   (for next/prev). Record must in this case point to last record
	   Returncodes:
	   0 = Ok.
	   HA_ERR_RECORD_DELETED = Record was removed
	   HA_ERR_KEY_NOT_FOUND = Record not found with key
	*/

int heap_rsame(register HP_INFO *info, byte *record, int inx)
{
  HP_SHARE *share=info->s;
  DBUG_ENTER("heap_rsame");

  test_active(info);
  if (info->current_ptr[share->reclength])
  {
    if (inx < -1 || inx >= (int) share->keys)
    {
      DBUG_RETURN(my_errno=HA_ERR_WRONG_INDEX);
    }
    else if (inx != -1)
    {
      info->lastinx=inx;
      _hp_make_key(share->keydef+inx,info->lastkey,record);
      if (!_hp_search(info,share->keydef+inx,info->lastkey,3))
      {
	info->update=0;
	DBUG_RETURN(my_errno);
      }
    }
    memcpy(record,info->current_ptr,(size_t) share->reclength);
    DBUG_RETURN(0);
  }
  info->update=0;

  DBUG_RETURN(my_errno=HA_ERR_RECORD_DELETED);
}
