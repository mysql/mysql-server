/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

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

/*
  remove all records from database
  Identical as hp_create() and hp_open() but used HP_SHARE* instead of name and
  database remains open.
*/

#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/heap/heapdef.h"

void heap_clear(HP_INFO *info) { hp_clear(info->s); }

void hp_clear(HP_SHARE *info) {
  DBUG_ENTER("hp_clear");

  if (info->block.levels)
    (void)hp_free_level(&info->block, info->block.levels, info->block.root,
                        (uchar *)0);
  info->block.levels = 0;
  hp_clear_keys(info);
  info->records = info->deleted = 0;
  info->data_length = 0;
  info->blength = 1;
  info->changed = 0;
  info->del_link = 0;
  DBUG_VOID_RETURN;
}

/*
  Clear all keys.

  SYNOPSIS
    heap_clear_keys()
    info      A pointer to the heap storage engine HP_INFO struct.

  DESCRIPTION
    Delete all trees of all indexes and leave them empty.

  RETURN
    void
*/

void heap_clear_keys(HP_INFO *info) { hp_clear(info->s); }

/*
  Clear all keys.

  SYNOPSIS
    hp_clear_keys()
    info      A pointer to the heap storage engine HP_SHARE struct.

  DESCRIPTION
    Delete all trees of all indexes and leave them empty.

  RETURN
    void
*/

void hp_clear_keys(HP_SHARE *info) {
  uint key;
  DBUG_ENTER("hp_clear_keys");

  for (key = 0; key < info->keys; key++) {
    HP_KEYDEF *keyinfo = info->keydef + key;
    if (keyinfo->algorithm == HA_KEY_ALG_BTREE) {
      delete_tree(&keyinfo->rb_tree);
    } else {
      HP_BLOCK *block = &keyinfo->block;
      if (block->levels)
        (void)hp_free_level(block, block->levels, block->root, (uchar *)0);
      block->levels = 0;
      block->last_allocated = 0;
      keyinfo->hash_buckets = 0;
    }
  }
  info->index_length = 0;
  DBUG_VOID_RETURN;
}

/*
  Disable all indexes.

  SYNOPSIS
    heap_disable_indexes()
    info      A pointer to the heap storage engine HP_INFO struct.

  DESCRIPTION
    Disable and clear (remove contents of) all indexes.

  RETURN
    0  ok
*/

int heap_disable_indexes(HP_INFO *info) {
  HP_SHARE *share = info->s;

  if (share->keys) {
    hp_clear_keys(share);
    share->currently_disabled_keys = share->keys;
    share->keys = 0;
  }
  return 0;
}

/*
  Enable all indexes

  SYNOPSIS
    heap_enable_indexes()
    info      A pointer to the heap storage engine HP_INFO struct.

  DESCRIPTION
    Enable all indexes. The indexes might have been disabled
    by heap_disable_index() before.
    The function works only if both data and indexes are empty,
    since the heap storage engine cannot repair the indexes.
    To be sure, call handler::delete_all_rows() before.

  RETURN
    0  ok
    HA_ERR_CRASHED data or index is non-empty.
*/

int heap_enable_indexes(HP_INFO *info) {
  int error = 0;
  HP_SHARE *share = info->s;

  if (share->data_length || share->index_length)
    error = HA_ERR_CRASHED;
  else if (share->currently_disabled_keys) {
    share->keys = share->currently_disabled_keys;
    share->currently_disabled_keys = 0;
  }
  return error;
}

/*
  Test if indexes are disabled.

  SYNOPSIS
    heap_indexes_are_disabled()
    info      A pointer to the heap storage engine HP_INFO struct.

  DESCRIPTION
    Test if indexes are disabled.

  RETURN
    0  indexes are not disabled
    1  all indexes are disabled
   [2  non-unique indexes are disabled - NOT YET IMPLEMENTED]
*/

int heap_indexes_are_disabled(HP_INFO *info) {
  HP_SHARE *share = info->s;

  return (!share->keys && share->currently_disabled_keys);
}
