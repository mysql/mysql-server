/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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
  DBUG_PRINT("enter",("base: %lx  reopen: %u  locks: %u",
		      info,(uint) share->reopen, (uint) share->tot_locks));

  pthread_mutex_lock(&THR_LOCK_myisam);
  if (info->lock_type == F_EXTRA_LCK)
    info->lock_type=F_UNLCK;			/* HA_EXTRA_NO_USER_CHANGE */

  if (share->reopen == 1 && share->kfile >= 0)
    _mi_decrement_open_count(info);

  if (info->lock_type != F_UNLCK)
  {
    if (mi_lock_database(info,F_UNLCK))
      error=my_errno;
  }
  pthread_mutex_lock(&share->intern_lock);

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
  pthread_mutex_unlock(&share->intern_lock);

  my_free(mi_get_rec_buff_ptr(info, info->rec_buff), MYF(MY_ALLOW_ZERO_PTR));
  if (flag)
  {
    if (share->kfile >= 0 &&
	flush_key_blocks(share->kfile,
			 share->temporary ? FLUSH_IGNORE_CHANGED :
			 FLUSH_RELEASE))
      error=my_errno;
    if (share->kfile >= 0)
    {
      /* We must always flush the state with the current open_count. */
      if (share->mode != O_RDONLY)
	mi_state_info_write(share->kfile, &share->state, 1);
      if (my_close(share->kfile,MYF(0)))
        error = my_errno;
    }
#ifdef HAVE_MMAP
    if (share->file_map)
      _mi_unmap_file(info);
#endif
    if (share->decode_trees)
    {
      my_free((gptr) share->decode_trees,MYF(0));
      my_free((gptr) share->decode_tables,MYF(0));
    }
#ifdef THREAD
    thr_lock_delete(&share->lock);
    VOID(pthread_mutex_destroy(&share->intern_lock));
    {
      int i,keys;
      keys = share->state.header.keys;
      for(i=0; i<keys; i++) {
	VOID(rwlock_destroy(&share->key_root_lock[i]));
      }
    }
#endif
    my_free((gptr) info->s,MYF(0));
  }
  pthread_mutex_unlock(&THR_LOCK_myisam);
  if (info->dfile >= 0 && my_close(info->dfile,MYF(0)))
    error = my_errno;

  myisam_log_command(MI_LOG_CLOSE,info,NULL,0,error);
  my_free((gptr) info,MYF(0));

  if (error)
  {
    DBUG_RETURN(my_errno=error);
  }
  DBUG_RETURN(0);
} /* mi_close */
