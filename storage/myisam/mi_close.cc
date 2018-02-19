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

/* close a isam-database */
/*
  TODO:
   We need to have a separate mutex on the closed file to allow other threads
   to open other files during the time we flush the cache and close this file
*/

#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "storage/myisam/myisamdef.h"

int mi_close_share(MI_INFO *info, bool *closed_share) {
  int error = 0, flag;
  MYISAM_SHARE *share = info->s;
  DBUG_ENTER("mi_close_share");
  DBUG_PRINT("enter", ("base: %p  reopen: %u  locks: %u", info,
                       (uint)share->reopen, (uint)share->tot_locks));

  if (info->open_list.data) mysql_mutex_lock(&THR_LOCK_myisam);
  if (info->lock_type == F_EXTRA_LCK)
    info->lock_type = F_UNLCK; /* HA_EXTRA_NO_USER_CHANGE */

  if (info->lock_type != F_UNLCK) {
    if (mi_lock_database(info, F_UNLCK)) error = my_errno();
  }
  mysql_mutex_lock(&share->intern_lock);

  if (share->options & HA_OPTION_READ_ONLY_DATA) {
    share->r_locks--;
    share->tot_locks--;
  }
  if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED)) {
    if (end_io_cache(&info->rec_cache)) error = my_errno();
    info->opt_flag &= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  }
  flag = !--share->reopen;
  if (info->open_list.data)
    myisam_open_list = list_delete(myisam_open_list, &info->open_list);
  mysql_mutex_unlock(&share->intern_lock);

  my_free(mi_get_rec_buff_ptr(info, info->rec_buff));
  if (flag) {
    DBUG_EXECUTE_IF("crash_before_flush_keys", if (share->kfile >= 0) abort(););
    if (share->kfile >= 0 &&
        flush_key_blocks(
            share->key_cache, keycache_thread_var(), share->kfile,
            share->temporary ? FLUSH_IGNORE_CHANGED : FLUSH_RELEASE))
      error = my_errno();
    if (share->kfile >= 0) {
      /*
        If we are crashed, we can safely flush the current state as it will
        not change the crashed state.
        We can NOT write the state in other cases as other threads
        may be using the file at this point
      */
      if (share->mode != O_RDONLY && mi_is_crashed(info))
        mi_state_info_write(share->kfile, &share->state, 1);
      /* Decrement open count must be last I/O on this file. */
      _mi_decrement_open_count(info);
      if (mysql_file_close(share->kfile, MYF(0))) error = my_errno();
    }
    if (share->file_map) {
      if (share->options & HA_OPTION_COMPRESS_RECORD)
        _mi_unmap_file(info);
      else
        mi_munmap_file(info);
    }
    if (share->decode_trees) {
      my_free(share->decode_trees);
      my_free(share->decode_tables);
    }
    thr_lock_delete(&share->lock);
    mysql_mutex_destroy(&share->intern_lock);
    {
      int i, keys;
      keys = share->state.header.keys;
      mysql_rwlock_destroy(&share->mmap_lock);
      for (i = 0; i < keys; i++) {
        mysql_rwlock_destroy(&share->key_root_lock[i]);
      }
    }
    my_free(info->s);
    if (closed_share) *closed_share = true;
  }
  if (info->open_list.data) mysql_mutex_unlock(&THR_LOCK_myisam);
  if (info->ftparser_param) {
    my_free(info->ftparser_param);
    info->ftparser_param = 0;
  }
  if (info->dfile >= 0 && mysql_file_close(info->dfile, MYF(0)))
    error = my_errno();

  myisam_log_command(MI_LOG_CLOSE, info, NULL, 0, error);
  my_free(info);

  if (error) {
    set_my_errno(error);
    DBUG_RETURN(error);
  }
  DBUG_RETURN(0);
} /* mi_close_share */
