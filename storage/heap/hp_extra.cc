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

/* Extra functions we want to do with a database */
/* - Set flags for quicker databasehandler */
/* - Set databasehandler to normal */
/* - Reset recordpointers as after open database */

#include <sys/types.h>

#include "my_dbug.h"
#include "storage/heap/heapdef.h"

static void heap_extra_keyflag(HP_INFO *info, enum ha_extra_function function);

/* set extra flags for database */

int heap_extra(HP_INFO *info, enum ha_extra_function function) {
  DBUG_TRACE;

  switch (function) {
    case HA_EXTRA_RESET_STATE:
      heap_reset(info);
      break;
    case HA_EXTRA_NO_READCHECK:
      info->opt_flag &= ~READ_CHECK_USED; /* No readcheck */
      break;
    case HA_EXTRA_READCHECK:
      info->opt_flag |= READ_CHECK_USED;
      break;
    case HA_EXTRA_CHANGE_KEY_TO_UNIQUE:
    case HA_EXTRA_CHANGE_KEY_TO_DUP:
      heap_extra_keyflag(info, function);
      break;
    default:
      break;
  }
  return 0;
} /* heap_extra */

int heap_reset(HP_INFO *info) {
  info->lastinx = -1;
  info->current_record = (ulong)~0L;
  info->current_hash_ptr = nullptr;
  info->update = 0;
  info->next_block = 0;
  return 0;
}

/*
    Start/Stop Inserting Duplicates Into a Table, WL#1648.
 */
static void heap_extra_keyflag(HP_INFO *info, enum ha_extra_function function) {
  uint idx;

  for (idx = 0; idx < info->s->keys; idx++) {
    switch (function) {
      case HA_EXTRA_CHANGE_KEY_TO_UNIQUE:
        info->s->keydef[idx].flag |= HA_NOSAME;
        break;
      case HA_EXTRA_CHANGE_KEY_TO_DUP:
        info->s->keydef[idx].flag &= ~(HA_NOSAME);
        break;
      default:
        break;
    }
  }
}
