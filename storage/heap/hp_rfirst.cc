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

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/heap/heapdef.h"

/* Read first record with the current key */

int heap_rfirst(HP_INFO *info, uchar *record, int inx) {
  HP_SHARE *share = info->s;
  HP_KEYDEF *keyinfo = share->keydef + inx;

  DBUG_TRACE;
  info->lastinx = inx;
  if (keyinfo->algorithm == HA_KEY_ALG_BTREE) {
    uchar *pos;

    if ((pos = (uchar *)tree_search_edge(&keyinfo->rb_tree, info->parents,
                                         &info->last_pos,
                                         offsetof(TREE_ELEMENT, left)))) {
      memcpy(&pos, pos + (*keyinfo->get_key_length)(keyinfo, pos),
             sizeof(uchar *));
      info->current_ptr = pos;
      memcpy(record, pos, (size_t)share->reclength);
      /*
        If we're performing index_first on a table that was taken from
        table cache, info->lastkey_len is initialized to previous query.
        Thus we set info->lastkey_len to proper value for subsequent
        heap_rnext() calls.
        This is needed for DELETE queries only, otherwise this variable is
        not used.
        Note that the same workaround may be needed for heap_rlast(), but
        for now heap_rlast() is never used for DELETE queries.
      */
      info->lastkey_len = 0;
      info->update = HA_STATE_AKTIV;
    } else {
      set_my_errno(HA_ERR_END_OF_FILE);
      return my_errno();
    }
    return 0;
  } else {
    if (!(info->s->records)) {
      set_my_errno(HA_ERR_END_OF_FILE);
      return my_errno();
    }
    assert(0); /* TODO fix it */
    info->current_record = 0;
    info->current_hash_ptr = nullptr;
    info->update = HA_STATE_PREV_FOUND;
    return heap_rnext(info, record);
  }
}
