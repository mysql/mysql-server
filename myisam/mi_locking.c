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

/*
  locking of isam-tables.
  reads info from a isam-table. Must be first request before doing any furter
  calls to any isamfunktion.  Is used to allow many process use the same
  isamdatabase.
  */

#include "myisamdef.h"
#ifdef	__WIN__
#include <errno.h>
#endif
#if !defined(HAVE_PREAD) && defined(THREAD)
pthread_mutex_t THR_LOCK_keycache;
#endif

	/* lock table by F_UNLCK, F_RDLCK or F_WRLCK */

int mi_lock_database(MI_INFO *info, int lock_type)
{
  int error;
  uint count;
  MYISAM_SHARE *share=info->s;
  uint flag;
  DBUG_ENTER("mi_lock_database");

  if (share->options & HA_OPTION_READ_ONLY_DATA ||
      info->lock_type == lock_type)
    DBUG_RETURN(0);
  flag=error=0;
  pthread_mutex_lock(&share->intern_lock);
  if (share->kfile >= 0)		/* May only be false on windows */
  {
    switch (lock_type)
    {
    case F_UNLCK:
      if (info->lock_type == F_RDLCK)
	count= --share->r_locks;
      else
	count= --share->w_locks;
      if (info->lock_type == F_WRLCK && !share->w_locks &&
	  !share->delay_key_write && flush_key_blocks(share->kfile,FLUSH_KEEP))
      {
	error=my_errno;
	mi_mark_crashed(info);		/* Mark that table must be checked */
      }
      if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
      {
	if (end_io_cache(&info->rec_cache))
	{
	  error=my_errno;
	  mi_mark_crashed(info);
	}
      }
      if (!count)
      {
	if (share->changed && !share->w_locks)
	{
	  share->state.process= share->last_process=share->this_process;
	  share->state.unique=   info->last_unique=  info->this_unique;
#ifndef HAVE_PREAD
	  pthread_mutex_lock(&THR_LOCK_keycache); /* QQ; Has to be removed! */
#endif
	  if (mi_state_info_write(share->kfile, &share->state, 1))
	    error=my_errno;
#ifndef HAVE_PREAD
	  pthread_mutex_unlock(&THR_LOCK_keycache);/* QQ; Has to be removed! */
#endif
	  share->changed=0;
	  if (myisam_flush)
	  {
#if defined(__WIN__)
	    if (_commit(share->kfile))
	      error=errno;
	    if (_commit(info->dfile))
	      error=errno;
#elif defined(HAVE_FDATASYNC)
	    if (fdatasync(share->kfile))
	      error=errno;
	    if (fdatasync(share->dfile))
	      error=errno;
#elif defined(HAVE_FSYNC)
	    if (fsync(share->kfile))
	      error=errno;
	    if (fsync(share->dfile))
	      error=errno;
#endif
	  }
	  else
	    share->not_flushed=1;
	  if (error)
	    mi_mark_crashed(info);
	}
	if (share->r_locks)
	{					/* Only read locks left */
	  flag=1;
	  if (my_lock(share->kfile,F_RDLCK,0L,F_TO_EOF,
		      MYF(MY_WME | MY_SEEK_NOT_DONE)) && !error)
	    error=my_errno;
	}
	else if (!share->w_locks)
	{					/* No more locks */
	  flag=1;
	  if (my_lock(share->kfile,F_UNLCK,0L,F_TO_EOF,
		      MYF(MY_WME | MY_SEEK_NOT_DONE)) && !error)
	    error=my_errno;
	}
      }
      info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
      info->lock_type= F_UNLCK;
      break;
    case F_RDLCK:
      if (info->lock_type == F_WRLCK)
      {						/* Change RW to READONLY */
	if (share->w_locks == 1)
	{
	  flag=1;
	  if (my_lock(share->kfile,lock_type,0L,F_TO_EOF,
		      MYF(MY_SEEK_NOT_DONE)))
	  {
	    error=my_errno;
	    break;
	  }
	}
	share->w_locks--;
	share->r_locks++;
	info->lock_type=lock_type;
	break;
      }
      if (!share->r_locks && !share->w_locks)
      {
	flag=1;
	if (my_lock(share->kfile,lock_type,0L,F_TO_EOF,
		    info->lock_wait | MY_SEEK_NOT_DONE))
	{
	  error=my_errno;
	  break;
	}
	if (mi_state_info_read_dsk(share->kfile, &share->state, 1))
	{
	  error=my_errno;
	  VOID(my_lock(share->kfile,F_UNLCK,0L,F_TO_EOF,MYF(MY_SEEK_NOT_DONE)));
	  my_errno=error;
	  break;
	}
      }
      VOID(_mi_test_if_changed(info));
      share->r_locks++;
      info->lock_type=lock_type;
      break;
    case F_WRLCK:
      if (info->lock_type == F_RDLCK)
      {						/* Change READONLY to RW */
	if (share->r_locks == 1)
	{
	  flag=1;
	  if (my_lock(share->kfile,lock_type,0L,F_TO_EOF,
		      MYF(info->lock_wait | MY_SEEK_NOT_DONE)))
	  {
	    error=my_errno;
	    break;
	  }
	  share->r_locks--;
	  share->w_locks++;
	  info->lock_type=lock_type;
	  break;
	}
      }
      if (!(share->options & HA_OPTION_READ_ONLY_DATA))
      {
	if (!share->w_locks)
	{
	  flag=1;
	  VOID(my_seek(share->kfile,0L,MY_SEEK_SET,MYF(0)));
	  if (my_lock(share->kfile,lock_type,0L,F_TO_EOF,info->lock_wait))
	  {
	    error=my_errno;
	    break;
	  }
	  if (!share->r_locks)
	  {
	    if (mi_state_info_read_dsk(share->kfile, &share->state, 0))
	    {
	      error=my_errno;
	      VOID(my_lock(share->kfile,F_UNLCK,0L,F_TO_EOF,info->lock_wait));
	      my_errno=error;
	      break;
	    }
	  }
	}
      }
      VOID(_mi_test_if_changed(info));
      info->lock_type=lock_type;
      share->w_locks++;
      break;
    default:
      break;				/* Impossible */
    }
  }
  pthread_mutex_unlock(&share->intern_lock);
#if defined(FULL_LOG) || defined(_lint)
  lock_type|=(int) (flag << 8);		/* Set bit to set if real lock */
  myisam_log_command(MI_LOG_LOCK,info,(byte*) &lock_type,sizeof(lock_type),
		     error);
#endif
  DBUG_RETURN(error);
} /* mi_lock_database */


/****************************************************************************
** The following functions are called by thr_lock() in threaded applications
****************************************************************************/

void mi_get_status(void* param)
{
  MI_INFO *info=(MI_INFO*) param;
  DBUG_ENTER("mi_get_status");
  DBUG_PRINT("info",("key_file: %ld  data_file: %ld",
		     (long) info->s->state.state.key_file_length,
		     (long) info->s->state.state.data_file_length));
#ifndef DBUG_OFF
  if (info->state->key_file_length > info->s->state.state.key_file_length ||
      info->state->data_file_length > info->s->state.state.data_file_length)
    DBUG_PRINT("warning",("old info:  key_file: %ld  data_file: %ld",
			  (long) info->state->key_file_length,
			  (long) info->state->data_file_length));
#endif
  info->save_state=info->s->state.state;
  info->state= &info->save_state;
  DBUG_VOID_RETURN;
}

void mi_update_status(void* param)
{
  MI_INFO *info=(MI_INFO*) param;
  /*
    Because someone may have closed the table we point at, we only
    update the state if its our own state.  This isn't a problem as
    we are always pointing at our own lock or at a read lock.
    (This is enforced by thr_multi_lock.c)
  */
  if (info->state == &info->save_state)
  {
#ifndef DBUG_OFF
    DBUG_PRINT("info",("updating status:  key_file: %ld  data_file: %ld",
		       (long) info->state->key_file_length,
		       (long) info->state->data_file_length));
    if (info->state->key_file_length < info->s->state.state.key_file_length ||
	info->state->data_file_length < info->s->state.state.data_file_length)
      DBUG_PRINT("warning",("old info:  key_file: %ld  data_file: %ld",
			    (long) info->s->state.state.key_file_length,
			    (long) info->s->state.state.data_file_length));
#endif
    info->s->state.state= *info->state;
    info->state= &info->s->state.state;
  }

  /*
    We have to flush the write cache here as other threads may start
    reading the table before mi_lock_database() is called
  */
  if (info->opt_flag & WRITE_CACHE_USED)
  {
    if (end_io_cache(&info->rec_cache))
    {
      mi_mark_crashed(info);
    }
    info->opt_flag&= ~WRITE_CACHE_USED;
  }
}

void mi_copy_status(void* to,void *from)
{
  ((MI_INFO*) to)->state= &((MI_INFO*) from)->save_state;
}

my_bool mi_check_status(void* param)
{
  MI_INFO *info=(MI_INFO*) param;
  return (my_bool) (info->s->state.dellink != HA_OFFSET_ERROR);
}


/****************************************************************************
 ** functions to read / write the state
****************************************************************************/

int _mi_readinfo(register MI_INFO *info, int lock_type, int check_keybuffer)
{
  MYISAM_SHARE *share;
  DBUG_ENTER("_mi_readinfo");

  share=info->s;
  if (info->lock_type == F_UNLCK)
  {
    if (!share->r_locks && !share->w_locks)
    {
      if ((info->tmp_lock_type=lock_type) != F_RDLCK)
	if (my_lock(share->kfile,lock_type,0L,F_TO_EOF,
		    info->lock_wait | MY_SEEK_NOT_DONE))
	  DBUG_RETURN(1);
      if (mi_state_info_read_dsk(share->kfile, &share->state, 1))
      {
	int error=my_errno ? my_errno : -1;
	VOID(my_lock(share->kfile,F_UNLCK,0L,F_TO_EOF,
		     MYF(MY_SEEK_NOT_DONE)));
	my_errno=error;
	DBUG_RETURN(1);
      }
    }
    if (check_keybuffer)
      VOID(_mi_test_if_changed(info));
  }
  else if (lock_type == F_WRLCK && info->lock_type == F_RDLCK)
  {
    my_errno=EACCES;				/* Not allowed to change */
    DBUG_RETURN(-1);				/* when have read_lock() */
  }
  DBUG_RETURN(0);
} /* _mi_readinfo */


	/* Every isam-function that uppdates the isam-database must! end */
	/* with this request */
	/* ARGSUSED */

int _mi_writeinfo(register MI_INFO *info, uint operation)
{
  int error,olderror;
  MYISAM_SHARE *share;
  DBUG_ENTER("_mi_writeinfo");

  error=0;
  share=info->s;
  if (share->r_locks == 0 && share->w_locks == 0)
  {
    olderror=my_errno;			/* Remember last error */
    if (operation)
    {					/* Two threads can't be here */
      share->state.process= share->last_process=   share->this_process;
      share->state.unique=  info->last_unique=	   info->this_unique;
      if ((error=mi_state_info_write(share->kfile, &share->state, 1)))
	olderror=my_errno;
#ifdef __WIN__
      if (myisam_flush)
      {
	_commit(share->kfile);
	_commit(info->dfile);
      }
#endif
    }
    if (!(operation & WRITEINFO_NO_UNLOCK) &&
	my_lock(share->kfile,F_UNLCK,0L,F_TO_EOF,
		MYF(MY_WME | MY_SEEK_NOT_DONE)) && !error)
      DBUG_RETURN(1);
    my_errno=olderror;
  }
  else if (operation)
  {
    share->changed= 1;			/* Mark keyfile changed */
  }
    DBUG_RETURN(error);
} /* _mi_writeinfo */


	/* Test if someone has changed the database */
	/* (Should be called after readinfo) */

int _mi_test_if_changed(register MI_INFO *info)
{
  MYISAM_SHARE *share=info->s;
  if (share->state.process != share->last_process ||
      share->state.unique  != info->last_unique)
  {						/* Keyfile has changed */
    if (share->state.process != share->this_process)
      VOID(flush_key_blocks(share->kfile,FLUSH_RELEASE));
    share->last_process=share->state.process;
    info->last_unique=	share->state.unique;
    info->update|=	HA_STATE_WRITTEN;	/* Must use file on next */
    info->data_changed= 1;			/* For mi_is_changed */
    return 1;
  }
  return (!(info->update & HA_STATE_AKTIV) ||
	  (info->update & (HA_STATE_WRITTEN | HA_STATE_DELETED |
			   HA_STATE_KEY_CHANGED)));
} /* _mi_test_if_changed */


/* Put a mark in the .ISM file that someone is updating the table */

int _mi_mark_file_changed(MI_INFO *info)
{
  char buff[3];
  register MYISAM_SHARE *share=info->s;
  if (!(share->state.changed & STATE_CHANGED) || ! share->global_changed)
  {
    share->state.changed|=(STATE_CHANGED | STATE_NOT_ANALYZED |
			   STATE_NOT_OPTIMIZED_KEYS);
    if (!share->global_changed)
    {
      share->global_changed=1;
      share->state.open_count++;
    }
    mi_int2store(buff,share->state.open_count);
    buff[2]=1;					/* Mark that it's changed */
    return (my_pwrite(share->kfile,buff,sizeof(buff),
		      sizeof(share->state.header),
		      MYF(MY_NABP)));
  }
  return 0;
}


/*
  This is only called by close or by extra(HA_FLUSH) if the OS has the pwrite()
  call.  In these context the following code should be safe!
 */

int _mi_decrement_open_count(MI_INFO *info)
{
  char buff[2];
  register MYISAM_SHARE *share=info->s;
  int lock_error=0,write_error=0;
  if (share->global_changed)
  {
    uint old_lock=info->lock_type;
    share->global_changed=0;
    lock_error=mi_lock_database(info,F_WRLCK);
    /* Its not fatal even if we couldn't get the lock ! */
    if (share->state.open_count > 0)
    {
      share->state.open_count--;
      mi_int2store(buff,share->state.open_count);
      write_error=my_pwrite(share->kfile,buff,sizeof(buff),
			    sizeof(share->state.header),
			    MYF(MY_NABP));
    }
    if (!lock_error)
      lock_error=mi_lock_database(info,old_lock);
  }
  return test(lock_error || write_error);
}
