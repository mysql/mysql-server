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

/* Read a record from a random position */

#include "heapdef.h"

/*
	   Returns one of following values:
	   0 = Ok.
	   HA_ERR_RECORD_DELETED = Record is deleted.
	   HA_ERR_END_OF_FILE = EOF.
*/

int heap_rrnd(register HP_INFO *info, byte *record, byte *pos)
{
  HP_SHARE *share=info->s;
  DBUG_ENTER("heap_rrnd");
  DBUG_PRINT("enter",("info: %lx  pos: %lx",info,pos));

  info->lastinx= -1;
  if (!(info->current_ptr= pos))
  {
    info->update= 0;
    DBUG_RETURN(my_errno= HA_ERR_END_OF_FILE);
  }
  if (!info->current_ptr[share->reclength])
  {
    info->update= HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND;
    DBUG_RETURN(my_errno=HA_ERR_RECORD_DELETED);
  }
  info->update=HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND | HA_STATE_AKTIV;
  memcpy(record,info->current_ptr,(size_t) share->reclength);
  DBUG_PRINT("exit",("found record at %lx",info->current_ptr));
  info->current_hash_ptr=0;			/* Can't use rnext */
  DBUG_RETURN(0);
} /* heap_rrnd */


#ifdef WANT_OLD_HEAP_VERSION

/*
	   If pos == -1 then read next record
	   Returns one of following values:
	   0 = Ok.
	   HA_ERR_RECORD_DELETED = Record is deleted.
	   HA_ERR_END_OF_FILE = EOF.
*/

int heap_rrnd_old(register HP_INFO *info, byte *record, ulong pos)
{
  HP_SHARE *share=info->s;
  DBUG_ENTER("heap_rrnd");
  DBUG_PRINT("enter",("info: %lx  pos: %ld",info,pos));

  info->lastinx= -1;
  if (pos == (ulong) -1)
  {
    pos= ++info->current_record;
    if (pos % share->block.records_in_block &&	/* Quick next record */
	pos < share->records+share->deleted &&
	(info->update & HA_STATE_PREV_FOUND))
    {
      info->current_ptr+=share->block.recbuffer;
      goto end;
    }
  }
  else
    info->current_record=pos;

  if (pos >= share->records+share->deleted)
  {
    info->update= 0;
    DBUG_RETURN(my_errno= HA_ERR_END_OF_FILE);
  }

	/* Find record number pos */
  _hp_find_record(info,pos);

end:
  if (!info->current_ptr[share->reclength])
  {
    info->update= HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND;
    DBUG_RETURN(my_errno=HA_ERR_RECORD_DELETED);
  }
  info->update=HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND | HA_STATE_AKTIV;
  memcpy(record,info->current_ptr,(size_t) share->reclength);
  DBUG_PRINT("exit",("found record at %lx",info->current_ptr));
  info->current_hash_ptr=0;			/* Can't use rnext */
  DBUG_RETURN(0);
} /* heap_rrnd */

#endif /* WANT_OLD_HEAP_VERSION */
