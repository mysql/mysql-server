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

int heap_rrnd(HP_INFO *info, uchar *record, HP_HEAP_POSITION *pos) {
  HP_SHARE *share = info->s;
  DBUG_TRACE;
  DBUG_PRINT("enter", ("info: %p  pos: %p", info, pos));

  info->lastinx = -1;
  if (!(info->current_ptr = pos->ptr)) {
    info->update = 0;
    set_my_errno(HA_ERR_END_OF_FILE);
    return HA_ERR_END_OF_FILE;
  }
  if (!info->current_ptr[share->reclength]) {
    info->update = HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND;
    set_my_errno(HA_ERR_RECORD_DELETED);
    return HA_ERR_RECORD_DELETED;
  }
  info->update = HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND | HA_STATE_AKTIV;
  memcpy(record, info->current_ptr, (size_t)share->reclength);

  // reposition scan state also
  info->current_record = info->next_block = pos->record_no;

  DBUG_PRINT("exit", ("found record at %p", info->current_ptr));
  info->current_hash_ptr = nullptr; /* Can't use rnext */
  return 0;
} /* heap_rrnd */
