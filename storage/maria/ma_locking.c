/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

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

#include "ma_ftdefs.h"

	/* lock table by F_UNLCK, F_RDLCK or F_WRLCK */

int maria_lock_database(MARIA_HA *info, int lock_type)
{
  int error;
  uint count;
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("maria_lock_database");
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
    DBUG_RETURN(0);
  }

  error=0;
  pthread_mutex_lock(&share->intern_lock);
  if (share->kfile.file >= 0)		/* May only be false on windows */
  {
    switch (lock_type) {
    case F_UNLCK:
      maria_ftparser_call_deinitializer(info);
      if (info->lock_type == F_RDLCK)
      {
	count= --share->r_locks;
        _ma_restore_status(info);
      }
      else
      {
	count= --share->w_locks;
        _ma_update_status(info);
      }
      --share->tot_locks;
      if (info->lock_type == F_WRLCK && !share->w_locks)
      {
        /* pages of transactional tables get flushed at Checkpoint */
        if (!share->base.born_transactional && !share->temporary &&
            _ma_flush_table_files(info,
                                  share->delay_key_write ? MARIA_FLUSH_DATA :
                                  MARIA_FLUSH_DATA | MARIA_FLUSH_INDEX,
                                  FLUSH_KEEP, FLUSH_KEEP))
          error= my_errno;
      }
      if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
      {
	if (end_io_cache(&info->rec_cache))
	{
	  error=my_errno;
          maria_print_error(info->s, HA_ERR_CRASHED);
	  maria_mark_crashed(info);
	}
      }
      if (!count)
      {
	DBUG_PRINT("info",("changed: %u  w_locks: %u",
			   (uint) share->changed, share->w_locks));
	if (share->changed && !share->w_locks)
	{
#ifdef HAVE_MMAP
          if ((info->s->mmaped_length !=
               info->s->state.state.data_file_length) &&
              (info->s->nonmmaped_inserts > MAX_NONMAPPED_INSERTS))
          {
            if (info->s->concurrent_insert)
              rw_wrlock(&info->s->mmap_lock);
            _ma_remap_file(info, info->s->state.state.data_file_length);
            info->s->nonmmaped_inserts= 0;
            if (info->s->concurrent_insert)
              rw_unlock(&info->s->mmap_lock);
          }
#endif
#ifdef EXTERNAL_LOCKING
	  share->state.process= share->last_process=share->this_process;
	  share->state.unique=   info->last_unique=  info->this_unique;
	  share->state.update_count= info->last_loop= ++info->this_loop;
#endif
          /* transactional tables rather flush their state at Checkpoint */
          if (!share->base.born_transactional)
          {
            if (_ma_state_info_write_sub(share->kfile.file, &share->state, 1))
              error= my_errno;
            else
            {
              /* A value of 0 means below means "state flushed" */
              share->changed= 0;
            }
          }
	  if (maria_flush)
	  {
            if (_ma_sync_table_files(info))
	      error= my_errno;
	  }
	  else
	    share->not_flushed=1;
	  if (error)
          {
            maria_print_error(info->s, HA_ERR_CRASHED);
	    maria_mark_crashed(info);
          }
	}
      }
      info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
      info->lock_type= F_UNLCK;
      break;
    case F_RDLCK:
      if (info->lock_type == F_WRLCK)
      {
        /*
          Change RW to READONLY

          mysqld does not turn write locks to read locks,
          so we're never here in mysqld.
        */
	share->w_locks--;
	share->r_locks++;
	info->lock_type=lock_type;
	break;
      }
#ifdef MARIA_EXTERNAL_LOCKING
      if (!share->r_locks && !share->w_locks)
      {
        /* note that a transactional table should not do this */
	if (_ma_state_info_read_dsk(share->kfile.file, &share->state))
	{
	  error=my_errno;
	  break;
	}
      }
#endif
      VOID(_ma_test_if_changed(info));
      share->r_locks++;
      share->tot_locks++;
      info->lock_type=lock_type;
      break;
    case F_WRLCK:
      if (info->lock_type == F_RDLCK)
      {						/* Change READONLY to RW */
	if (share->r_locks == 1)
	{
	  share->r_locks--;
	  share->w_locks++;
	  info->lock_type=lock_type;
	  break;
	}
      }
#ifdef MARIA_EXTERNAL_LOCKING
      if (!(share->options & HA_OPTION_READ_ONLY_DATA))
      {
	if (!share->w_locks)
	{
	  if (!share->r_locks)
	  {
            /*
              Note that transactional tables should not do this.
              If we enabled this code, we should make sure to skip it if
              born_transactional is true. We should not test
              now_transactional to decide if we can call
              _ma_state_info_read_dsk(), because it can temporarily be 0
              (TRUNCATE on a partitioned table) and thus it would make a state
              modification below without mutex, confusing a concurrent
              checkpoint running.
              Even if this code was enabled only for non-transactional tables:
              in scenario LOCK TABLE t1 WRITE; INSERT INTO t1; DELETE FROM t1;
              state on disk read by DELETE is obsolete as it was not flushed
              at the end of INSERT. MyISAM same. It however causes no issue as
              maria_delete_all_rows() calls _ma_reset_status() thus is not
              influenced by the obsolete read values.
            */
	    if (_ma_state_info_read_dsk(share->kfile.file, &share->state))
	    {
	      error=my_errno;
	      break;
	    }
	  }
	}
      }
#endif /* defined(MARIA_EXTERNAL_LOCKING) */
      VOID(_ma_test_if_changed(info));

      info->lock_type=lock_type;
      info->invalidator=info->s->invalidator;
      share->w_locks++;
      share->tot_locks++;
      break;
    default:
      DBUG_ASSERT(0);
      break;				/* Impossible */
    }
  }
#ifdef __WIN__
  else
  {
    /*
       Check for bad file descriptors if this table is part
       of a merge union. Failing to capture this may cause
       a crash on windows if the table is renamed and
       later on referenced by the merge table.
     */
    if( info->owned_by_merge && (info->s)->kfile.file < 0 )
    {
      error = HA_ERR_NO_SUCH_TABLE;
    }
  }
#endif
  pthread_mutex_unlock(&share->intern_lock);
  DBUG_RETURN(error);
} /* maria_lock_database */


/****************************************************************************
  The following functions are called by thr_lock() in threaded applications
****************************************************************************/

/*
  Create a copy of the current status for the table

  SYNOPSIS
    _ma_get_status()
    param		Pointer to Myisam handler
    concurrent_insert	Set to 1 if we are going to do concurrent inserts
			(THR_WRITE_CONCURRENT_INSERT was used)
*/

void _ma_get_status(void* param, my_bool concurrent_insert)
{
  MARIA_HA *info=(MARIA_HA*) param;
  DBUG_ENTER("_ma_get_status");
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
  DBUG_VOID_RETURN;
}


void _ma_update_status(void* param)
{
  MARIA_HA *info=(MARIA_HA*) param;
  /*
    Because someone may have closed the table we point at, we only
    update the state if its our own state.  This isn't a problem as
    we are always pointing at our own lock or at a read lock.
    (This is enforced by thr_multi_lock.c)
  */
  if (info->state == &info->save_state)
  {
    MARIA_SHARE *share= info->s;
#ifndef DBUG_OFF
    DBUG_PRINT("info",("updating status:  key_file: %ld  data_file: %ld",
		       (long) info->state->key_file_length,
		       (long) info->state->data_file_length));
    if (info->state->key_file_length < share->state.state.key_file_length ||
	info->state->data_file_length < share->state.state.data_file_length)
      DBUG_PRINT("warning",("old info:  key_file: %ld  data_file: %ld",
			    (long) share->state.state.key_file_length,
			    (long) share->state.state.data_file_length));
#endif
    /*
      we are going to modify the state without lock's log, this would break
      recovery if done with a transactional table.
    */
    DBUG_ASSERT(!info->s->base.born_transactional);
    share->state.state= *info->state;
    info->state= &share->state.state;
  }
  info->append_insert_at_end= 0;
}


void _ma_restore_status(void *param)
{
  MARIA_HA *info= (MARIA_HA*) param;
  info->state= &info->s->state.state;
  info->append_insert_at_end= 0;
}


void _ma_copy_status(void* to,void *from)
{
  ((MARIA_HA*) to)->state= &((MARIA_HA*) from)->save_state;
}


/*
  Check if should allow concurrent inserts

  IMPLEMENTATION
    Allow concurrent inserts if we don't have a hole in the table or
    if there is no active write lock and there is active read locks and
    maria_concurrent_insert == 2. In this last case the new
    row('s) are inserted at end of file instead of filling up the hole.

    The last case is to allow one to inserts into a heavily read-used table
    even if there is holes.

  NOTES
    If there is a an rtree indexes in the table, concurrent inserts are
    disabled in maria_open()

  RETURN
    0  ok to use concurrent inserts
    1  not ok
*/

my_bool _ma_check_status(void *param)
{
  MARIA_HA *info=(MARIA_HA*) param;
  /*
    The test for w_locks == 1 is here because this thread has already done an
    external lock (in other words: w_locks == 1 means no other threads has
    a write lock)
  */
  DBUG_PRINT("info",("dellink: %ld  r_locks: %u  w_locks: %u",
                     (long) info->s->state.dellink, (uint) info->s->r_locks,
                     (uint) info->s->w_locks));
  return (my_bool) !(info->s->state.dellink == HA_OFFSET_ERROR ||
                     (maria_concurrent_insert == 2 && info->s->r_locks &&
                      info->s->w_locks == 1));
}


/****************************************************************************
 ** functions to read / write the state
****************************************************************************/

int _ma_readinfo(register MARIA_HA *info __attribute__ ((unused)),
                 int lock_type __attribute__ ((unused)),
                 int check_keybuffer __attribute__ ((unused)))
{
#ifdef MARIA_EXTERNAL_LOCKING
  DBUG_ENTER("_ma_readinfo");

  if (info->lock_type == F_UNLCK)
  {
    MARIA_SHARE *share= info->s;
    if (!share->tot_locks)
    {
      /* should not be done for transactional tables */
      if (_ma_state_info_read_dsk(share->kfile.file, &share->state))
      {
        if (!my_errno)
          my_errno= HA_ERR_FILE_TOO_SHORT;
	DBUG_RETURN(1);
      }
    }
    if (check_keybuffer)
      VOID(_ma_test_if_changed(info));
    info->invalidator=info->s->invalidator;
  }
  else if (lock_type == F_WRLCK && info->lock_type == F_RDLCK)
  {
    my_errno=EACCES;				/* Not allowed to change */
    DBUG_RETURN(-1);				/* when have read_lock() */
  }
  DBUG_RETURN(0);
#else
  return 0;
#endif /* defined(MARIA_EXTERNAL_LOCKING) */
} /* _ma_readinfo */


/*
  Every isam-function that uppdates the isam-database MUST end with this
  request

  NOTES
    my_errno is not changed if this succeeds!
*/

int _ma_writeinfo(register MARIA_HA *info, uint operation)
{
  int error,olderror;
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("_ma_writeinfo");
  DBUG_PRINT("info",("operation: %u  tot_locks: %u", operation,
		     share->tot_locks));

  error=0;
  if (share->tot_locks == 0 && !share->base.born_transactional)
  {
    /* transactional tables flush their state at Checkpoint */
    if (operation)
    {					/* Two threads can't be here */
      olderror= my_errno;               /* Remember last error */

#ifdef EXTERNAL_LOCKING
      /*
        The following only makes sense if we want to be allow two different
        processes access the same table at the same time
      */
      share->state.process= share->last_process=   share->this_process;
      share->state.unique=  info->last_unique=	   info->this_unique;
      share->state.update_count= info->last_loop= ++info->this_loop;
#endif

      if ((error= _ma_state_info_write_sub(share->kfile.file,
                                           &share->state, 1)))
	olderror=my_errno;
#ifdef __WIN__
      if (maria_flush)
      {
	_commit(share->kfile.file);
	_commit(info->dfile.file);
      }
#endif
      my_errno=olderror;
    }
  }
  else if (operation)
    share->changed= 1;			/* Mark keyfile changed */
  DBUG_RETURN(error);
} /* _ma_writeinfo */


/*
  Test if an external process has changed the database
  (Should be called after readinfo)
*/

int _ma_test_if_changed(register MARIA_HA *info)
{
#ifdef EXTERNAL_LOCKING
  MARIA_SHARE *share= info->s;
  if (share->state.process != share->last_process ||
      share->state.unique  != info->last_unique ||
      share->state.update_count != info->last_loop)
  {						/* Keyfile has changed */
    DBUG_PRINT("info",("index file changed"));
    if (share->state.process != share->this_process)
      VOID(flush_pagecache_blocks(share->pagecache, &share->kfile,
                                  FLUSH_RELEASE));
    share->last_process=share->state.process;
    info->last_unique=	share->state.unique;
    info->last_loop=	share->state.update_count;
    info->update|=	HA_STATE_WRITTEN;	/* Must use file on next */
    info->data_changed= 1;			/* For maria_is_changed */
    return 1;
  }
#endif
  return (!(info->update & HA_STATE_AKTIV) ||
	  (info->update & (HA_STATE_WRITTEN | HA_STATE_DELETED |
			   HA_STATE_KEY_CHANGED)));
} /* _ma_test_if_changed */


/*
  Put a mark in the .MAI file that someone is updating the table

  DOCUMENTATION
  state.open_count in the .MAI file is used the following way:
  - For the first change of the .MYI file in this process open_count is
    incremented by _ma_mark_file_changed(). (We have a write lock on the file
    when this happens)
  - In maria_close() it's decremented by _ma_decrement_open_count() if it
    was incremented in the same process.

  This mean that if we are the only process using the file, the open_count
  tells us if the MARIA file wasn't properly closed. (This is true if
  my_disable_locking is set).

  open_count is not maintained on disk for transactional or temporary tables.
*/

int _ma_mark_file_changed(MARIA_HA *info)
{
  uchar buff[3];
  register MARIA_SHARE *share= info->s;
  DBUG_ENTER("_ma_mark_file_changed");

  if (!(share->state.changed & STATE_CHANGED) || ! share->global_changed)
  {
    share->state.changed|=(STATE_CHANGED | STATE_NOT_ANALYZED |
			   STATE_NOT_OPTIMIZED_KEYS);
    if (!share->global_changed)
    {
      share->global_changed=1;
      share->state.open_count++;
    }
    /*
      temp tables don't need an open_count as they are removed on crash;
      transactional tables are fixed by log-based recovery, so don't need an
      open_count either (and we thus avoid the disk write below).
    */
    if (!(share->temporary | share->base.born_transactional))
    {
      mi_int2store(buff,share->state.open_count);
      buff[2]=1;				/* Mark that it's changed */
      if (my_pwrite(share->kfile.file, buff, sizeof(buff),
                    sizeof(share->state.header) +
                    MARIA_FILE_OPEN_COUNT_OFFSET,
                    MYF(MY_NABP)))
        DBUG_RETURN(1);
    }
    /* Set uuid of file if not yet set (zerofilled file) */
    if (share->base.born_transactional &&
        !(share->state.changed & STATE_NOT_MOVABLE))
    {
      /* Lock table to current installation */
      if (_ma_set_uuid(info, 0) ||
          (share->state.create_rename_lsn == LSN_REPAIRED_BY_MARIA_CHK &&
           _ma_update_state_lsns_sub(share, translog_get_horizon(),
                                     TRUE, TRUE)))
        DBUG_RETURN(1);
      share->state.changed|= STATE_NOT_MOVABLE;
    }
  }
  DBUG_RETURN(0);
}

/*
  Check that a region is all zero

  SYNOPSIS
    check_if_zero()
    pos		Start of memory to check
    length	length of memory region

  NOTES
    Used mainly to detect rows with wrong extent information
*/

my_bool _ma_check_if_zero(uchar *pos, size_t length)
{
  uchar *end;
  for (end= pos+ length; pos != end ; pos++)
    if (pos[0] != 0)
      return 1;
  return 0;
}

/*
  This is only called by close or by extra(HA_FLUSH) if the OS has the pwrite()
  call.  In these context the following code should be safe!
 */

int _ma_decrement_open_count(MARIA_HA *info)
{
  uchar buff[2];
  register MARIA_SHARE *share= info->s;
  int lock_error=0,write_error=0;
  if (share->global_changed)
  {
    uint old_lock=info->lock_type;
    share->global_changed=0;
    lock_error=maria_lock_database(info,F_WRLCK);
    /* Its not fatal even if we couldn't get the lock ! */
    if (share->state.open_count > 0)
    {
      share->state.open_count--;
      share->changed= 1;                        /* We have to update state */
      if (!(share->temporary | share->base.born_transactional))
      {
        mi_int2store(buff,share->state.open_count);
        write_error= (int) my_pwrite(share->kfile.file, buff, sizeof(buff),
                                     sizeof(share->state.header) +
                                     MARIA_FILE_OPEN_COUNT_OFFSET,
                                     MYF(MY_NABP));
      }
    }
    if (!lock_error)
      lock_error=maria_lock_database(info,old_lock);
  }
  return test(lock_error || write_error);
}


/** @brief mark file as crashed */

void _ma_mark_file_crashed(MARIA_SHARE *share)
{
  uchar buff[2];
  DBUG_ENTER("_ma_mark_file_crashed");

  share->state.changed|= STATE_CRASHED;
  mi_int2store(buff, share->state.changed);
  /*
    We can ignore the errors, as if the mark failed, there isn't anything
    else we can do;  The user should already have got an error that the
    table was crashed.
  */
  (void) my_pwrite(share->kfile.file, buff, sizeof(buff),
                   sizeof(share->state.header) +
                   MARIA_FILE_CHANGED_OFFSET,
                   MYF(MY_NABP));
  DBUG_VOID_RETURN;
}


/**
   @brief Set uuid of for a Maria file

   @fn _ma_set_uuid()
   @param info		Maria handler
   @param reset_uuid    Instead of setting file to maria_uuid, set it to
			0 to mark it as movable
*/

my_bool _ma_set_uuid(MARIA_HA *info, my_bool reset_uuid)
{
  uchar buff[MY_UUID_SIZE], *uuid;

  uuid= maria_uuid;
  if (reset_uuid)
  {
    bzero(buff, sizeof(buff));
    uuid= buff;
  }
  return (my_bool) my_pwrite(info->s->kfile.file, uuid, MY_UUID_SIZE,
                             mi_uint2korr(info->s->state.header.base_pos),
                             MYF(MY_NABP));
}
