/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Scan through all rows */

#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/heap/heapdef.h"

/*
           Returns one of following values:
           0 = Ok.
           HA_ERR_RECORD_DELETED = Record is deleted.
           HA_ERR_END_OF_FILE = EOF.
*/

int heap_scan_init(HP_INFO *info) {
  DBUG_TRACE;
  info->lastinx = -1;
  info->current_record = (ulong)~0L; /* No current record */
  info->update = 0;
  info->next_block = 0;
  return 0;
}

int heap_scan(HP_INFO *info, uchar *record) {
  HP_SHARE *share = info->s;
  ulong pos;
  DBUG_TRACE;

  pos = ++info->current_record;
  if (pos < info->next_block) {
    info->current_ptr += share->block.recbuffer;
  } else {
    info->next_block += share->block.records_in_block;
    /*
      The table is organized as a linked list of blocks, each block has room
      for a fixed number (share->records_in_block) of records
      of fixed size (share->block.recbuffer).
    */
    info->next_block -= (info->next_block % share->block.records_in_block);
    if (info->next_block >= share->records + share->deleted) {
      info->next_block = share->records + share->deleted;
      if (pos >= info->next_block) {
        info->update = 0;
        set_my_errno(HA_ERR_END_OF_FILE);
        return HA_ERR_END_OF_FILE;
      }
    }
    hp_find_record(info, pos);
  }
  if (!info->current_ptr[share->reclength]) {
    DBUG_PRINT("warning", ("Found deleted record"));
    info->update = HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND;
    set_my_errno(HA_ERR_RECORD_DELETED);
    return HA_ERR_RECORD_DELETED;
  }
  info->update = HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND | HA_STATE_AKTIV;
  memcpy(record, info->current_ptr, (size_t)share->reclength);
  info->current_hash_ptr = nullptr; /* Can't use read_next */
  return 0;
} /* heap_scan */
