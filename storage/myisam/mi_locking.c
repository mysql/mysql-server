/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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

/*
  locking of isam-tables.
  reads info from a isam-table. Must be first request before doing any furter
  calls to any isamfunktion.  Is used to allow many process use the same
  isamdatabase.
*/

#include "ftdefs.h"

	/* lock table by F_UNLCK, F_RDLCK or F_WRLCK */

int mi_lock_database(MI_INFO *info, int lock_type)
{
  int error;
  uint count;
  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("mi_lock_database");
  DBUG_PRINT("enter",("lock_type: %d  old lock %d  r_locks: %u  w_locks: %u "
                      "global_changed:  %d  open_count: %u  name: '%s'",
                      lock_type, info->lock_type, share->r_locks,
                      share->w_locks,
                      share->global_changed, share->state.open_count,
                      share->index_file_name));
  if (share->options & HA_OPTION_READ_ONLY_DATA ||
      info->lock_type == lock_type)
    DBUG_RETURN(0);
  if (lock_type == F_EXTRA_LCK)                 /* Used by TMP tables */
  {
    ++share->w_locks;
    ++share->tot_locks;
    info->lock_type= lock_type;
    info->s->in_use= list_add(info->s->in_use, &info->in_use);
    DBUG_RETURN(0);
  }

  error= 0;
  mysql_mutex_lock(&share->intern_lock);
  if (share->kfile >= 0)		/* May only be false on windows */
  {
    switch (lock_type) {
    case F_UNLCK:
      ftparser_call_deinitializer(info);
      if (info->lock_type == F_RDLCK)
	count= --share->r_locks;
      else
	count= --share->w_locks;
      --share->tot_locks;
      if (info->lock_type == F_WRLCK && !share->w_locks &&
	  !share->delay_key_write && flush_key_blocks(share->key_cache,
						      share->kfile,FLUSH_KEEP))
      {
	error=my_errno;
        mi_print_error(info->s, HA_ERR_CRASHED);
	mi_mark_crashed(info);		/* Mark that table must be checked */
      }
      if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
      {
	if (end_io_cache(&info->rec_cache))
	{
	  error=my_errno;
          mi_print_error(info->s, HA_ERR_CRASHED);
	  mi_mark_crashed(info);
	}
      }
      if (!count)
      {
	DBUG_PRINT("info",("changed: %u  w_locks: %u",
			   (uint) share->changed, share->w_locks));
	if (share->changed && !share->w_locks)
	{
#ifdef HAVE_MMAP
    if ((info->s->mmaped_length != info->s->state.state.data_file_length) &&
        (info->s->nonmmaped_inserts > MAX_NONMAPPED_INSERTS))
    {
      if (info->s->concurrent_insert)
        mysql_rwlock_wrlock(&info->s->mmap_lock);
      mi_remap_file(info, info->s->state.state.data_file_length);
      info->s->nonmmaped_inserts= 0;
      if (info->s->concurrent_insert)
        mysql_rwlock_unlock(&info->s->mmap_lock);
    }
#endif
	  share->state.process= share->last_process=share->this_process;
	  share->state.unique=   info->last_unique=  info->this_unique;
	  share->state.update_count= info->last_loop= ++info->this_loop;
          if (mi_state_info_write(share->kfile, &share->state, 1))
	    error=my_errno;
	  share->changed=0;
	  if (myisam_flush)
	  {
            if (mysql_file_sync(share->kfile, MYF(0)))
	      error= my_errno;
            if (mysql_file_sync(info->dfile, MYF(0)))
	      error= my_errno;
	  }
	  else
	    share->not_flushed=1;
	  if (error)
          {
            mi_print_error(info->s, HA_ERR_CRASHED);
	    mi_mark_crashed(info);
          }
	}
	if (info->lock_type != F_EXTRA_LCK)
	{
	  if (share->r_locks)
	  {					/* Only read locks left */
	    if (my_lock(share->kfile,F_RDLCK,0L,F_TO_EOF,
			MYF(MY_WME | MY_SEEK_NOT_DONE)) && !error)
	      error=my_errno;
	  }
	  else if (!share->w_locks)
	  {					/* No more locks */
	    if (my_lock(share->kfile,F_UNLCK,0L,F_TO_EOF,
			MYF(MY_WME | MY_SEEK_NOT_DONE)) && !error)
	      error=my_errno;
	  }
	}
      }
      info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
      info->lock_type= F_UNLCK;
      info->s->in_use= list_delete(info->s->in_use, &info->in_use);
      break;
    case F_RDLCK:
      if (info->lock_type == F_WRLCK)
      {
        /*
          Change RW to READONLY

          mysqld does not turn write locks to read locks,
          so we're never here in mysqld.
        */
	if (share->w_locks == 1)
	{
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
	if (my_lock(share->kfile,lock_type,0L,F_TO_EOF,
		    info->lock_wait | MY_SEEK_NOT_DONE))
	{
	  error=my_errno;
	  break;
	}
	if (mi_state_info_read_dsk(share->kfile, &share->state, 1))
	{
	  error=my_errno;
	  (void) my_lock(share->kfile,F_UNLCK,0L,F_TO_EOF,MYF(MY_SEEK_NOT_DONE));
	  my_errno=error;
	  break;
	}
      }
      (void) _mi_test_if_changed(info);
      share->r_locks++;
      share->tot_locks++;
      info->lock_type=lock_type;
      info->s->in_use= list_add(info->s->in_use, &info->in_use);
      break;
    case F_WRLCK:
      if (info->lock_type == F_RDLCK)
      {						/* Change READONLY to RW */
	if (share->r_locks == 1)
	{
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
	  if (my_lock(share->kfile,lock_type,0L,F_TO_EOF,
		      info->lock_wait | MY_SEEK_NOT_DONE))
	  {
	    error=my_errno;
	    break;
	  }
	  if (!share->r_locks)
	  {
	    if (mi_state_info_read_dsk(share->kfile, &share->state, 1))
	    {
	      error=my_errno;
	      (void) my_lock(share->kfile,F_UNLCK,0L,F_TO_EOF,
			   info->lock_wait | MY_SEEK_NOT_DONE);
	      my_errno=error;
	      break;
	    }
	  }
	}
      }
      (void) _mi_test_if_changed(info);
        
      info->lock_type=lock_type;
      info->invalidator=info->s->invalidator;
      share->w_locks++;
      share->tot_locks++;
      info->s->in_use= list_add(info->s->in_use, &info->in_use);
      break;
    default:
      break;				/* Impossible */
    }
  }
#ifdef _WIN32
  else
  {
    /*
       Check for bad file descriptors if this table is part
       of a merge union. Failing to capture this may cause
       a crash on windows if the table is renamed and 
       later on referenced by the merge table.
     */
    if( info->owned_by_merge && (info->s)->kfile < 0 )
    {
      error = HA_ERR_NO_SUCH_TABLE;
    }
  }
#endif
  mysql_mutex_unlock(&share->intern_lock);
  DBUG_RETURN(error);
} /* mi_lock_database */


/****************************************************************************
  The following functions are called by thr_lock() in threaded applications
****************************************************************************/

/*
  Create a copy of the current status for the table

  SYNOPSIS
    mi_get_status()
    param		Pointer to Myisam handler
    concurrent_insert	Set to 1 if we are going to do concurrent inserts
			(THR_WRITE_CONCURRENT_INSERT was used)
*/

void mi_get_status(void* param, int concurrent_insert)
{
  MI_INFO *info=(MI_INFO*) param;
  DBUG_ENTER("mi_get_status");
  DBUG_PRINT("info",("key_file: %ld  data_file: %ld  concurrent_insert: %d",
		     (long) info->s->state.state.key_file_length,
		     (long) info->s->state.state.data_file_length,
                     concurrent_insert));
#ifndef DBUG_OFF
  if (info->state->key_file_length > info->s->state.state.key_file_length ||
      info->state->data_file_length > info->s->state.state.data_file_length)
    DBUG_PRINT("warning",("old info:  key_file: %ld  data_file: %ld",
			  (long) info->state->key_file_length,
			  (long) info->state->data_file_length));
#endif
  info->save_state=info->s->state.state;
  info->state= &info->save_state;
  info->append_insert_at_end= concurrent_insert;
  if (concurrent_insert)
    info->s->state.state.uncacheable= TRUE;
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
  }
  info->state= &info->s->state.state;
  info->append_insert_at_end= 0;

  /*
    We have to flush the write cache here as other threads may start
    reading the table before mi_lock_database() is called
  */
  if (info->opt_flag & WRITE_CACHE_USED)
  {
    if (end_io_cache(&info->rec_cache))
    {
      mi_print_error(info->s, HA_ERR_CRASHED);
      mi_mark_crashed(info);
    }
    info->opt_flag&= ~WRITE_CACHE_USED;
  }
}


void mi_restore_status(void *param)
{
  MI_INFO *info= (MI_INFO*) param;
  info->state= &info->s->state.state;
  info->append_insert_at_end= 0;
}


void mi_copy_status(void* to,void *from)
{
  ((MI_INFO*) to)->state= &((MI_INFO*) from)->save_state;
}


/*
  Check if should allow concurrent inserts

  IMPLEMENTATION
    Allow concurrent inserts if we don't have a hole in the table or
    if there is no active write lock and there is active read locks and 
    myisam_concurrent_insert == 2. In this last case the new
    row('s) are inserted at end of file instead of filling up the hole.

    The last case is to allow one to inserts into a heavily read-used table
    even if there is holes.

  NOTES
    If there is a an rtree indexes in the table, concurrent inserts are
    disabled in mi_open()

  RETURN
    0  ok to use concurrent inserts
    1  not ok
*/

my_bool mi_check_status(void *param)
{
  MI_INFO *info=(MI_INFO*) param;
  /*
    The test for w_locks == 1 is here because this thread has already done an
    external lock (in other words: w_locks == 1 means no other threads has
    a write lock)
  */
  DBUG_PRINT("info",("dellink: %ld  r_locks: %u  w_locks: %u",
                     (long) info->s->state.dellink, (uint) info->s->r_locks,
                     (uint) info->s->w_locks));
  return (my_bool) !(info->s->state.dellink == HA_OFFSET_ERROR ||
                     (myisam_concurrent_insert == 2 && info->s->r_locks &&
                      info->s->w_locks == 1));
}


/****************************************************************************
 ** functions to read / write the state
****************************************************************************/

int _mi_readinfo(register MI_INFO *info, int lock_type, int check_keybuffer)
{
  DBUG_ENTER("_mi_readinfo");

  if (info->lock_type == F_UNLCK)
  {
    MYISAM_SHARE *share=info->s;
    if (!share->tot_locks)
    {
      if (my_lock(share->kfile,lock_type,0L,F_TO_EOF,
		  info->lock_wait | MY_SEEK_NOT_DONE))
	DBUG_RETURN(1);
      if (mi_state_info_read_dsk(share->kfile, &share->state, 1))
      {
	int error=my_errno ? my_errno : -1;
	(void) my_lock(share->kfile,F_UNLCK,0L,F_TO_EOF,
		     MYF(MY_SEEK_NOT_DONE));
	my_errno=error;
	DBUG_RETURN(1);
      }
    }
    if (check_keybuffer)
      (void) _mi_test_if_changed(info);
    info->invalidator=info->s->invalidator;
  }
  else if (lock_type == F_WRLCK && info->lock_type == F_RDLCK)
  {
    my_errno=EACCES;				/* Not allowed to change */
    DBUG_RETURN(-1);				/* when have read_lock() */
  }
  DBUG_RETURN(0);
} /* _mi_readinfo */


/*
  Every isam-function that uppdates the isam-database MUST end with this
  request
*/

int _mi_writeinfo(register MI_INFO *info, uint operation)
{
  int error,olderror;
  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("_mi_writeinfo");
  DBUG_PRINT("info",("operation: %u  tot_locks: %u", operation,
		     share->tot_locks));

  error=0;
  if (share->tot_locks == 0)
  {
    olderror=my_errno;			/* Remember last error */
    if (operation)
    {					/* Two threads can't be here */
      share->state.process= share->last_process=   share->this_process;
      share->state.unique=  info->last_unique=	   info->this_unique;
      share->state.update_count= info->last_loop= ++info->this_loop;
      if ((error=mi_state_info_write(share->kfile, &share->state, 1)))
	olderror=my_errno;
#ifdef _WIN32
      if (myisam_flush)
      {
        mysql_file_sync(share->kfile, 0);
        mysql_file_sync(info->dfile, 0);
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
    share->changed= 1;			/* Mark keyfile changed */
  DBUG_RETURN(error);
} /* _mi_writeinfo */


	/* Test if someone has changed the database */
	/* (Should be called after readinfo) */

int _mi_test_if_changed(register MI_INFO *info)
{
  MYISAM_SHARE *share=info->s;
  if (share->state.process != share->last_process ||
      share->state.unique  != info->last_unique ||
      share->state.update_count != info->last_loop)
  {						/* Keyfile has changed */
    DBUG_PRINT("info",("index file changed"));
    if (share->state.process != share->this_process)
      (void) flush_key_blocks(share->key_cache, share->kfile, FLUSH_RELEASE);
    share->last_process=share->state.process;
    info->last_unique=	share->state.unique;
    info->last_loop=	share->state.update_count;
    info->update|=	HA_STATE_WRITTEN;	/* Must use file on next */
    info->data_changed= 1;			/* For mi_is_changed */
    return 1;
  }
  return (!(info->update & HA_STATE_AKTIV) ||
	  (info->update & (HA_STATE_WRITTEN | HA_STATE_DELETED |
			   HA_STATE_KEY_CHANGED)));
} /* _mi_test_if_changed */


/*
  Put a mark in the .MYI file that someone is updating the table


  DOCUMENTATION

  state.open_count in the .MYI file is used the following way:
  - For the first change of the .MYI file in this process open_count is
    incremented by mi_mark_file_change(). (We have a write lock on the file
    when this happens)
  - In mi_close() it's decremented by _mi_decrement_open_count() if it
    was incremented in the same process.

  This mean that if we are the only process using the file, the open_count
  tells us if the MYISAM file wasn't properly closed. (This is true if
  my_disable_locking is set).
*/


int _mi_mark_file_changed(MI_INFO *info)
{
  uchar buff[3];
  register MYISAM_SHARE *share=info->s;
  DBUG_ENTER("_mi_mark_file_changed");

  if (!(share->state.changed & STATE_CHANGED) || ! share->global_changed)
  {
    share->state.changed|=(STATE_CHANGED | STATE_NOT_ANALYZED |
			   STATE_NOT_OPTIMIZED_KEYS);
    if (!share->global_changed)
    {
      share->global_changed=1;
      share->state.open_count++;
    }
    if (!share->temporary)
    {
      mi_int2store(buff,share->state.open_count);
      buff[2]=1;				/* Mark that it's changed */
      DBUG_RETURN(mysql_file_pwrite(share->kfile, buff, sizeof(buff),
                                    sizeof(share->state.header),
                                    MYF(MY_NABP)));
    }
  }
  DBUG_RETURN(0);
}


/*
  This is only called by close or by extra(HA_FLUSH) if the OS has the pwrite()
  call.  In these context the following code should be safe!
 */

int _mi_decrement_open_count(MI_INFO *info)
{
  uchar buff[2];
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
      write_error= mysql_file_pwrite(share->kfile, buff, sizeof(buff),
                                     sizeof(share->state.header),
                                     MYF(MY_NABP));
    }
    if (!lock_error)
      lock_error=mi_lock_database(info,old_lock);
  }
  return test(lock_error || write_error);
}
