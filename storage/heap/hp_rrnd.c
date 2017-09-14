/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/heap/heapdef.h"

/*
	   Returns one of following values:
	   0 = Ok.
	   HA_ERR_RECORD_DELETED = Record is deleted.
	   HA_ERR_END_OF_FILE = EOF.
*/

int heap_rrnd(HP_INFO *info, uchar *record, HP_HEAP_POSITION *pos)
{
  HP_SHARE *share=info->s;
  DBUG_ENTER("heap_rrnd");
  DBUG_PRINT("enter",("info: %p  pos: %p", info, pos));

  info->lastinx= -1;
  if (!(info->current_ptr= pos->ptr))
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

  // reposition scan state also
  info->current_record= info->next_block= pos->record_no;

  DBUG_PRINT("exit", ("found record at %p", info->current_ptr));
  info->current_hash_ptr=0;			/* Can't use rnext */
  DBUG_RETURN(0);
} /* heap_rrnd */
