/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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

/* Update current record in heap-database */

#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/heap/heapdef.h"

int heap_update(HP_INFO *info, const uchar *old, const uchar *heap_new) {
  HP_KEYDEF *keydef, *end, *p_lastinx;
  uchar *pos;
  bool auto_key_changed = false;
  HP_SHARE *share = info->s;
  DBUG_TRACE;

  test_active(info);
  pos = info->current_ptr;

  if (info->opt_flag & READ_CHECK_USED && hp_rectest(info, old))
    return my_errno(); /* Record changed */
  if (--(share->records) < share->blength >> 1) share->blength >>= 1;
  share->changed = 1;

  p_lastinx = share->keydef + info->lastinx;
  for (keydef = share->keydef, end = keydef + share->keys; keydef < end;
       keydef++) {
    if (hp_rec_key_cmp(keydef, old, heap_new)) {
      if ((*keydef->delete_key)(info, keydef, old, pos, keydef == p_lastinx) ||
          (*keydef->write_key)(info, keydef, heap_new, pos))
        goto err;
      if (share->auto_key == (uint)(keydef - share->keydef + 1))
        auto_key_changed = true;
    }
  }

  memcpy(pos, heap_new, (size_t)share->reclength);
  if (++(share->records) == share->blength) share->blength += share->blength;

#if !defined(NDEBUG) && defined(EXTRA_HEAP_DEBUG)
  DBUG_EXECUTE("check_heap", heap_check_heap(info, 0););
#endif
  if (auto_key_changed) heap_update_auto_increment(info, heap_new);
  return 0;

err:
  if (my_errno() == HA_ERR_FOUND_DUPP_KEY) {
    info->errkey = (int)(keydef - share->keydef);
    if (keydef->algorithm == HA_KEY_ALG_BTREE) {
      /* we don't need to delete non-inserted key from rb-tree */
      if ((*keydef->write_key)(info, keydef, old, pos)) {
        if (++(share->records) == share->blength)
          share->blength += share->blength;
        return my_errno();
      }
      keydef--;
    }
    while (keydef >= share->keydef) {
      if (hp_rec_key_cmp(keydef, old, heap_new)) {
        if ((*keydef->delete_key)(info, keydef, heap_new, pos, 0) ||
            (*keydef->write_key)(info, keydef, old, pos))
          break;
      }
      keydef--;
    }
  }
  if (++(share->records) == share->blength) share->blength += share->blength;
  return my_errno();
} /* heap_update */
