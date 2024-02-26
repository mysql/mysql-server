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

#include <fcntl.h>

#include "my_dbug.h"
#include "storage/myisam/fulltext.h"
#include "storage/myisam/myisamdef.h"

/* if flag == HA_PANIC_CLOSE then all misam files are closed */
/* if flag == HA_PANIC_WRITE then all misam files are unlocked and
   all changed data in single user misam is written to file */
/* if flag == HA_PANIC_READ then all misam files that was locked when
   mi_panic(HA_PANIC_WRITE) was done is locked. A mi_readinfo() is
   done for all single user files to get changes in database */

int mi_panic(enum ha_panic_function flag) {
  int error = 0;
  LIST *list_element, *next_open;
  MI_INFO *info;
  DBUG_TRACE;

  mysql_mutex_lock(&THR_LOCK_myisam);
  for (list_element = myisam_open_list; list_element;
       list_element = next_open) {
    next_open = list_element->next; /* Save if close */
    info = (MI_INFO *)list_element->data;
    switch (flag) {
      case HA_PANIC_CLOSE:
        mysql_mutex_unlock(&THR_LOCK_myisam); /* Not exactly right... */
        if (mi_close(info)) error = my_errno();
        mysql_mutex_lock(&THR_LOCK_myisam);
        break;
      case HA_PANIC_WRITE: /* Do this to free databases */
        if (flush_key_blocks(info->s->key_cache, keycache_thread_var(),
                             info->s->kfile, FLUSH_RELEASE))
          error = my_errno();
        if (info->opt_flag & WRITE_CACHE_USED)
          if (flush_io_cache(&info->rec_cache)) error = my_errno();
        if (info->opt_flag & READ_CACHE_USED) {
          if (flush_io_cache(&info->rec_cache)) error = my_errno();
          reinit_io_cache(&info->rec_cache, READ_CACHE, 0,
                          (bool)(info->lock_type != F_UNLCK), true);
        }
        if (info->lock_type != F_UNLCK && !info->was_locked) {
          info->was_locked = info->lock_type;
          if (mi_lock_database(info, F_UNLCK)) error = my_errno();
        }
        break;
      case HA_PANIC_READ: /* Restore to before WRITE */
        if (info->was_locked) {
          if (mi_lock_database(info, info->was_locked)) error = my_errno();
          info->was_locked = false;
        }
        break;
    }
  }
  if (flag == HA_PANIC_CLOSE) {
    (void)mi_log(0); /* Close log if needed */
    ft_free_stopwords();
  }
  mysql_mutex_unlock(&THR_LOCK_myisam);
  if (!error) return 0;
  set_my_errno(error);
  return error;
} /* mi_panic */
