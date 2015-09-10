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

/* Read a record from a random position */

#include "heapdef.h"

/*
	   Returns one of following values:
	   0 = Ok.
	   HA_ERR_RECORD_DELETED = Record is deleted.
	   HA_ERR_END_OF_FILE = EOF.
*/

int heap_rrnd(HP_INFO *info, uchar *record, uchar *pos)
{
  HP_SHARE *share=info->s;
  DBUG_ENTER("heap_rrnd");
  DBUG_PRINT("enter",("info: 0x%lx  pos: %lx",(long) info, (long) pos));

  info->lastinx= -1;
  if (!(info->current_ptr= pos))
  {
    info->update= 0;
    set_my_errno(HA_ERR_END_OF_FILE);
    DBUG_RETURN(HA_ERR_END_OF_FILE);
  }
  if (!info->current_ptr[share->reclength])
  {
    info->update= HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND;
    set_my_errno(HA_ERR_RECORD_DELETED);
    DBUG_RETURN(HA_ERR_RECORD_DELETED);
  }
  info->update=HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND | HA_STATE_AKTIV;
  memcpy(record,info->current_ptr,(size_t) share->reclength);
  DBUG_PRINT("exit", ("found record at 0x%lx", (long) info->current_ptr));
  info->current_hash_ptr=0;			/* Can't use rnext */
  DBUG_RETURN(0);
} /* heap_rrnd */
