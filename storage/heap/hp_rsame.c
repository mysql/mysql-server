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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/* re-read current record */

#include "heapdef.h"

	/* If inx != -1 the new record is read according to index
	   (for next/prev). Record must in this case point to last record
	   Returncodes:
	   0 = Ok.
	   HA_ERR_RECORD_DELETED = Record was removed
	   HA_ERR_KEY_NOT_FOUND = Record not found with key
	*/

int heap_rsame(HP_INFO *info, uchar *record, int inx)
{
  HP_SHARE *share=info->s;
  DBUG_ENTER("heap_rsame");

  test_active(info);
  if (info->current_ptr[share->reclength])
  {
    if (inx < -1 || inx >= (int) share->keys)
    {
      set_my_errno(HA_ERR_WRONG_INDEX);
      DBUG_RETURN(HA_ERR_WRONG_INDEX);
    }
    else if (inx != -1)
    {
      info->lastinx=inx;
      hp_make_key(share->keydef + inx, info->lastkey, record);
      if (!hp_search(info, share->keydef + inx, info->lastkey, 3))
      {
	info->update=0;
	DBUG_RETURN(my_errno());
      }
    }
    memcpy(record,info->current_ptr,(size_t) share->reclength);
    DBUG_RETURN(0);
  }
  info->update=0;

  set_my_errno(HA_ERR_RECORD_DELETED);
  DBUG_RETURN(HA_ERR_RECORD_DELETED);
}
