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

#include "isamdef.h"

int nisam_close(register N_INFO *info)
{
  int error=0,flag;
  ISAM_SHARE *share=info->s;
  DBUG_ENTER("nisam_close");
  DBUG_PRINT("enter",("base: %lx  reopen: %u  locks: %u",
		      info,(uint) share->reopen,
		      (uint) (share->w_locks+share->r_locks)));

  pthread_mutex_lock(&THR_LOCK_isam);
  if (info->lock_type == F_EXTRA_LCK)
    info->lock_type=F_UNLCK;			/* HA_EXTRA_NO_USER_CHANGE */

#ifndef NO_LOCKING
  if (info->lock_type != F_UNLCK)
    VOID(nisam_lock_database(info,F_UNLCK));
#else
  info->lock_type=F_UNLCK;
  share->w_locks--;
  if (_nisam_writeinfo(info,test(share->changed)))
    error=my_errno;
#endif
  pthread_mutex_lock(&share->intern_lock);

  if (share->base.options & HA_OPTION_READ_ONLY_DATA)
    share->r_locks--;
  if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
  {
    if (end_io_cache(&info->rec_cache))
      error=my_errno;
    info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
  }
  flag= !--share->reopen;
  nisam_open_list=list_delete(nisam_open_list,&info->open_list);
  pthread_mutex_unlock(&share->intern_lock);

  if (flag)
  {
    if (share->kfile >= 0 && flush_key_blocks(share->kfile,FLUSH_RELEASE))
      error=my_errno;
    if (share->kfile >= 0 && my_close(share->kfile,MYF(0)))
      error = my_errno;
#ifdef HAVE_MMAP
    _nisam_unmap_file(info);
#endif
    if (share->decode_trees)
    {
      my_free((gptr) share->decode_trees,MYF(0));
      my_free((gptr) share->decode_tables,MYF(0));
    }
#ifdef THREAD
    thr_lock_delete(&share->lock);
    VOID(pthread_mutex_destroy(&share->intern_lock));
#endif
    my_free((gptr) info->s,MYF(0));
  }
  pthread_mutex_unlock(&THR_LOCK_isam);
  if (info->dfile >= 0 && my_close(info->dfile,MYF(0)))
    error = my_errno;

  nisam_log_command(LOG_CLOSE,info,NULL,0,error);
  my_free((gptr) info->rec_alloc,MYF(0));
  my_free((gptr) info,MYF(0));

  if (error)
  {
    my_errno=error;
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
} /* nisam_close */
