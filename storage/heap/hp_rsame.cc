/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* re-read current record */

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/heap/heapdef.h"

/* If inx != -1 the new record is read according to index
   (for next/prev). Record must in this case point to last record
   Returncodes:
   0 = Ok.
   HA_ERR_RECORD_DELETED = Record was removed
   HA_ERR_KEY_NOT_FOUND = Record not found with key
*/

int heap_rsame(HP_INFO *info, uchar *record, int inx) {
  HP_SHARE *share = info->s;
  DBUG_TRACE;

  test_active(info);
  if (info->current_ptr[share->reclength]) {
    if (inx < -1 || inx >= (int)share->keys) {
      set_my_errno(HA_ERR_WRONG_INDEX);
      return HA_ERR_WRONG_INDEX;
    } else if (inx != -1) {
      info->lastinx = inx;
      hp_make_key(share->keydef + inx, info->lastkey, record);
      if (!hp_search(info, share->keydef + inx, info->lastkey, 3)) {
        info->update = 0;
        return my_errno();
      }
    }
    memcpy(record, info->current_ptr, (size_t)share->reclength);
    return 0;
  }
  info->update = 0;

  set_my_errno(HA_ERR_RECORD_DELETED);
  return HA_ERR_RECORD_DELETED;
}
