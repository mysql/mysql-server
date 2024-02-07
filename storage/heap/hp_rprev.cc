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

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/heap/heapdef.h"

/* Read prev record for key */

int heap_rprev(HP_INFO *info, uchar *record) {
  uchar *pos;
  HP_SHARE *share = info->s;
  HP_KEYDEF *keyinfo;
  DBUG_TRACE;

  if (info->lastinx < 0) {
    set_my_errno(HA_ERR_WRONG_INDEX);
    return HA_ERR_WRONG_INDEX;
  }
  keyinfo = share->keydef + info->lastinx;
  if (keyinfo->algorithm == HA_KEY_ALG_BTREE) {
    heap_rb_param custom_arg;

    if (info->last_pos)
      pos = (uchar *)tree_search_next(&keyinfo->rb_tree, &info->last_pos,
                                      offsetof(TREE_ELEMENT, right),
                                      offsetof(TREE_ELEMENT, left));
    else {
      custom_arg.keyseg = keyinfo->seg;
      custom_arg.key_length = keyinfo->length;
      custom_arg.search_flag = SEARCH_SAME;
      pos = (uchar *)tree_search_key(&keyinfo->rb_tree, info->lastkey,
                                     info->parents, &info->last_pos,
                                     info->last_find_flag, &custom_arg);
    }
    if (pos) {
      memcpy(&pos, pos + (*keyinfo->get_key_length)(keyinfo, pos),
             sizeof(uchar *));
      info->current_ptr = pos;
    } else {
      set_my_errno(HA_ERR_KEY_NOT_FOUND);
    }
  } else {
    if (info->current_ptr || (info->update & HA_STATE_NEXT_FOUND)) {
      if ((info->update & HA_STATE_DELETED))
        pos = hp_search(info, share->keydef + info->lastinx, info->lastkey, 3);
      else
        pos = hp_search(info, share->keydef + info->lastinx, info->lastkey, 2);
    } else {
      pos = nullptr; /* Read next after last */
      set_my_errno(HA_ERR_KEY_NOT_FOUND);
    }
  }
  if (!pos) {
    info->update = HA_STATE_PREV_FOUND; /* For heap_rprev */
    if (my_errno() == HA_ERR_KEY_NOT_FOUND) set_my_errno(HA_ERR_END_OF_FILE);
    return my_errno();
  }
  memcpy(record, pos, (size_t)share->reclength);
  info->update = HA_STATE_AKTIV | HA_STATE_PREV_FOUND;
  return 0;
}
