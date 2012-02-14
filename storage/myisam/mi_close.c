/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* close a isam-database */
/*
  TODO:
   We need to have a separate mutex on the closed file to allow other threads
   to open other files during the time we flush the cache and close this file
*/

#include "myisamdef.h"

int mi_close(register MI_INFO *info)
{
  int error=0,flag;
  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("mi_close");
  DBUG_PRINT("enter",("base: 0x%lx  reopen: %u  locks: %u",
		      (long) info, (uint) share->reopen,
                      (uint) share->tot_locks));

  mysql_mutex_lock(&THR_LOCK_myisam);
  if (info->lock_type == F_EXTRA_LCK)
    info->lock_type=F_UNLCK;			/* HA_EXTRA_NO_USER_CHANGE */

  if (info->lock_type != F_UNLCK)
  {
    if (mi_lock_database(info,F_UNLCK))
      error=my_errno;
  }
  mysql_mutex_lock(&share->intern_lock);

  if (share->options & HA_OPTION_READ_ONLY_DATA)
  {
    share->r_locks--;
    share->tot_locks--;
  }
  if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
  {
    if (end_io_cache(&info->rec_cache))
      error=my_errno;
    info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  }
  flag= !--share->reopen;
  myisam_open_list=list_delete(myisam_open_list,&info->open_list);
  mysql_mutex_unlock(&share->intern_lock);

  my_free(mi_get_rec_buff_ptr(info, info->rec_buff));
  if (flag)
  {
    DBUG_EXECUTE_IF("crash_before_flush_keys",
                    if (share->kfile >= 0) abort(););
    if (share->kfile >= 0 &&
	flush_key_blocks(share->key_cache, share->kfile,
			 share->temporary ? FLUSH_IGNORE_CHANGED :
			 FLUSH_RELEASE))
      error=my_errno;
    if (share->kfile >= 0)
    {
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
      if (mysql_file_close(share->kfile, MYF(0)))
        error = my_errno;
    }
#ifdef HAVE_MMAP
    if (share->file_map)
    {
      if (share->options & HA_OPTION_COMPRESS_RECORD)
        _mi_unmap_file(info);
      else
        mi_munmap_file(info);
    }
#endif
    if (share->decode_trees)
    {
      my_free(share->decode_trees);
      my_free(share->decode_tables);
    }
    thr_lock_delete(&share->lock);
    mysql_mutex_destroy(&share->intern_lock);
    {
      int i,keys;
      keys = share->state.header.keys;
      mysql_rwlock_destroy(&share->mmap_lock);
      for(i=0; i<keys; i++) {
        mysql_rwlock_destroy(&share->key_root_lock[i]);
      }
    }
    my_free(info->s);
  }
  mysql_mutex_unlock(&THR_LOCK_myisam);
  if (info->ftparser_param)
  {
    my_free(info->ftparser_param);
    info->ftparser_param= 0;
  }
  if (info->dfile >= 0 && mysql_file_close(info->dfile, MYF(0)))
    error = my_errno;

  myisam_log_command(MI_LOG_CLOSE,info,NULL,0,error);
  my_free(info);

  if (error)
  {
    DBUG_RETURN(my_errno=error);
  }
  DBUG_RETURN(0);
} /* mi_close */
