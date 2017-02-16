/* Copyright (c) 2000-2002, 2005-2007 MySQL AB
   Use is subject to license terms

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

/* Scan through all rows */

#include "heapdef.h"

/*
	   Returns one of following values:
	   0 = Ok.
	   HA_ERR_RECORD_DELETED = Record is deleted.
	   HA_ERR_END_OF_FILE = EOF.
*/

int heap_scan_init(register HP_INFO *info)
{
  DBUG_ENTER("heap_scan_init");
  info->lastinx= -1;
  info->current_record= (ulong) ~0L;		/* No current record */
  info->update=0;
  info->next_block=0;
  info->key_version= info->s->key_version;
  info->file_version= info->s->file_version;
  DBUG_RETURN(0);
}

int heap_scan(register HP_INFO *info, uchar *record)
{
  HP_SHARE *share=info->s;
  ulong pos;
  DBUG_ENTER("heap_scan");

  pos= ++info->current_record;
  if (pos < info->next_block)
  {
    info->current_ptr+=share->block.recbuffer;
  }
  else
  {
    info->next_block+=share->block.records_in_block;
    if (info->next_block >= share->records+share->deleted)
    {
      info->next_block= share->records+share->deleted;
      if (pos >= info->next_block)
      {
	info->update= 0;
	DBUG_RETURN(my_errno= HA_ERR_END_OF_FILE);
      }
    }
    hp_find_record(info, pos);
  }
  if (!info->current_ptr[share->reclength])
  {
    DBUG_PRINT("warning",("Found deleted record"));
    info->update= HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND;
    DBUG_RETURN(my_errno=HA_ERR_RECORD_DELETED);
  }
  info->update= HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND | HA_STATE_AKTIV;
  memcpy(record,info->current_ptr,(size_t) share->reclength);
  info->current_hash_ptr=0;			/* Can't use read_next */
  DBUG_RETURN(0);
} /* heap_scan */
