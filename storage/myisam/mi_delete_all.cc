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

/* Remove all rows from a MyISAM table */
/* This clears the status information and truncates files */

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/myisam/myisamdef.h"

int mi_delete_all_rows(MI_INFO *info) {
  uint i;
  MYISAM_SHARE *share = info->s;
  MI_STATE_INFO *state = &share->state;
  DBUG_TRACE;

  if (share->options & HA_OPTION_READ_ONLY_DATA) {
    set_my_errno(EACCES);
    return EACCES;
  }
  if (_mi_readinfo(info, F_WRLCK, 1)) return my_errno();
  if (_mi_mark_file_changed(info)) goto err;

  info->state->records = info->state->del = state->split = 0;
  state->dellink = HA_OFFSET_ERROR;
  state->sortkey = (ushort)~0;
  info->state->key_file_length = share->base.keystart;
  info->state->data_file_length = 0;
  info->state->empty = info->state->key_empty = 0;
  info->state->checksum = 0;

  for (i = share->base.max_key_block_length / MI_MIN_KEY_BLOCK_LENGTH; i--;)
    state->key_del[i] = HA_OFFSET_ERROR;
  for (i = 0; i < share->base.keys; i++) state->key_root[i] = HA_OFFSET_ERROR;

  myisam_log_command(MI_LOG_DELETE_ALL, info, (uchar *)nullptr, 0, 0);
  /*
    If we are using delayed keys or if the user has done changes to the tables
    since it was locked then there may be key blocks in the key cache
  */
  flush_key_blocks(share->key_cache, keycache_thread_var(), share->kfile,
                   FLUSH_IGNORE_CHANGED);
  if (share->file_map) mi_munmap_file(info);
  if (mysql_file_chsize(info->dfile, 0, 0, MYF(MY_WME)) ||
      mysql_file_chsize(share->kfile, share->base.keystart, 0, MYF(MY_WME)))
    goto err;
  (void)_mi_writeinfo(info, WRITEINFO_UPDATE_KEYFILE);
  return 0;

err : {
  int save_errno = my_errno();
  (void)_mi_writeinfo(info, WRITEINFO_UPDATE_KEYFILE);
  info->update |= HA_STATE_WRITTEN; /* Buffer changed */
  set_my_errno(save_errno);
  return save_errno;
}
} /* mi_delete */
