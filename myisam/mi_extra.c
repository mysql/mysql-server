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

/* Extra functions we want to do with a database */
/* - Set flags for quicker databasehandler */
/* - Set databasehandler to normal */
/* - Reset recordpointers as after open database */

#include "myisamdef.h"
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#ifdef	__WIN__
#include <errno.h>
#endif

	/* set extra flags for database */

int mi_extra(MI_INFO *info, enum ha_extra_function function)
{
  int error=0;
  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("mi_extra");

  switch (function) {
  case HA_EXTRA_RESET:
    info->lastinx= 0;			/* Use first index as def */
    info->last_search_keypage=info->lastpos= HA_OFFSET_ERROR;
    info->page_changed=1;
					/* Next/prev gives first/last */
    if (info->opt_flag & READ_CACHE_USED)
    {
      reinit_io_cache(&info->rec_cache,READ_CACHE,0,
		      (pbool) (info->lock_type != F_UNLCK),
		      (pbool) test(info->update & HA_STATE_ROW_CHANGED)
		      );
    }
    info->update= ((info->update & HA_STATE_CHANGED) | HA_STATE_NEXT_FOUND |
		   HA_STATE_PREV_FOUND);
    break;
  case HA_EXTRA_CACHE:
    if (info->lock_type == F_UNLCK &&
	(share->options & HA_OPTION_PACK_RECORD))
    {
      error=1;			/* Not possibly if not locked */
      my_errno=EACCES;
      break;
    }
#if defined(HAVE_MMAP) && defined(HAVE_MADVICE)
    if ((share->options & HA_OPTION_COMPRESS_RECORD))
    {
      pthread_mutex_lock(&share->intern_lock);
      if (_mi_memmap_file(info))
      {
	/* We don't nead MADV_SEQUENTIAL if small file */
	madvise(share->file_map,share->state.state.data_file_length,
		share->state.state.data_file_length <= RECORD_CACHE_SIZE*16 ?
		MADV_RANDOM : MADV_SEQUENTIAL);
	pthread_mutex_unlock(&share->intern_lock);
	break;
      }
      pthread_mutex_unlock(&share->intern_lock);
    }
#endif
    if (info->opt_flag & WRITE_CACHE_USED)
    {
      info->opt_flag&= ~WRITE_CACHE_USED;
      if ((error=end_io_cache(&info->rec_cache)))
	break;
    }
    if (!(info->opt_flag &
	  (READ_CACHE_USED | WRITE_CACHE_USED | MEMMAP_USED)))
    {
      if (!(init_io_cache(&info->rec_cache,info->dfile,
			 (uint) min(info->state->data_file_length+1,
				    my_default_record_cache_size),
			 READ_CACHE,0L,(pbool) (info->lock_type != F_UNLCK),
			 MYF(share->write_flag & MY_WAIT_IF_FULL))))
      {
	info->opt_flag|=READ_CACHE_USED;
	info->update&= ~HA_STATE_ROW_CHANGED;
      }
      if (share->concurrent_insert)
	info->rec_cache.end_of_file=info->state->data_file_length;
    }
    break;
  case HA_EXTRA_REINIT_CACHE:
    if (info->opt_flag & READ_CACHE_USED)
    {
      reinit_io_cache(&info->rec_cache,READ_CACHE,info->nextpos,
		      (pbool) (info->lock_type != F_UNLCK),
		      (pbool) test(info->update & HA_STATE_ROW_CHANGED));
      info->update&= ~HA_STATE_ROW_CHANGED;
      if (share->concurrent_insert)
	info->rec_cache.end_of_file=info->state->data_file_length;
    }
    break;
  case HA_EXTRA_WRITE_CACHE:
    if (info->lock_type == F_UNLCK)
    {
      error=1;			/* Not possibly if not locked */
      break;
    }
    if (!(info->opt_flag &
	  (READ_CACHE_USED | WRITE_CACHE_USED | OPT_NO_ROWS)) &&
	!share->state.header.uniques)
      if (!(init_io_cache(&info->rec_cache,info->dfile,0,
			 WRITE_CACHE,info->state->data_file_length,
			  (pbool) (info->lock_type != F_UNLCK),
			  MYF(share->write_flag & MY_WAIT_IF_FULL))))
      {
	info->opt_flag|=WRITE_CACHE_USED;
	info->update&= ~(HA_STATE_ROW_CHANGED |
			 HA_STATE_WRITE_AT_END |
			 HA_STATE_EXTEND_BLOCK);
      }
    break;
  case HA_EXTRA_NO_CACHE:
    if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
    {
      info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
      error=end_io_cache(&info->rec_cache);
    }
#if defined(HAVE_MMAP) && defined(HAVE_MADVICE)
    if (info->opt_flag & MEMMAP_USED)
      madvise(share->file_map,share->state.state.data_file_length,MADV_RANDOM);
#endif
    break;
  case HA_EXTRA_FLUSH_CACHE:
    if (info->opt_flag & WRITE_CACHE_USED)
    {
      if ((error=flush_io_cache(&info->rec_cache)))
	mi_mark_crashed(info);			/* Fatal error found */
    }
    break;
  case HA_EXTRA_NO_READCHECK:
    info->opt_flag&= ~READ_CHECK_USED;		/* No readcheck */
    break;
  case HA_EXTRA_READCHECK:
    info->opt_flag|= READ_CHECK_USED;
    break;
  case HA_EXTRA_KEYREAD:			/* Read only keys to record */
  case HA_EXTRA_REMEMBER_POS:
    info->opt_flag |= REMEMBER_OLD_POS;
    bmove((byte*) info->lastkey+share->base.max_key_length*2,
	  (byte*) info->lastkey,info->lastkey_length);
    info->save_update=	info->update;
    info->save_lastinx= info->lastinx;
    info->save_lastpos= info->lastpos;
    info->save_lastkey_length=info->lastkey_length;
    if (function == HA_EXTRA_REMEMBER_POS)
      break;
    /* fall through */
  case HA_EXTRA_KEYREAD_CHANGE_POS:
    info->opt_flag |= KEY_READ_USED;
    info->read_record=_mi_read_key_record;
    break;
  case HA_EXTRA_NO_KEYREAD:
  case HA_EXTRA_RESTORE_POS:
    if (info->opt_flag & REMEMBER_OLD_POS)
    {
      bmove((byte*) info->lastkey,
	    (byte*) info->lastkey+share->base.max_key_length*2,
	    info->save_lastkey_length);
      info->update=	info->save_update | HA_STATE_WRITTEN;
      info->lastinx=	info->save_lastinx;
      info->lastpos=	info->save_lastpos;
      info->lastkey_length=info->save_lastkey_length;
    }
    info->read_record=	share->read_record;
    info->opt_flag&= ~(KEY_READ_USED | REMEMBER_OLD_POS);
    break;
  case HA_EXTRA_NO_USER_CHANGE: /* Database is somehow locked agains changes */
    info->lock_type= F_EXTRA_LCK; /* Simulate as locked */
    break;
  case HA_EXTRA_WAIT_LOCK:
    info->lock_wait=0;
    break;
  case HA_EXTRA_NO_WAIT_LOCK:
    info->lock_wait=MY_DONT_WAIT;
    break;
  case HA_EXTRA_NO_KEYS:
    if (info->lock_type == F_UNLCK)
    {
      error=1;					/* Not possibly if not lock */
      break;
    }
    if (share->state.key_map)
    {
      share->state.key_map=0;
      info->state->key_file_length=share->state.state.key_file_length=
	share->base.keystart;
      if (!share->changed)
      {
	share->state.changed|= STATE_CHANGED | STATE_NOT_ANALYZED;
	share->changed=1;			/* Update on close */
	if (!share->global_changed)
	{
	  share->global_changed=1;
	  share->state.open_count++;
	}
      }
      share->state.state= *info->state;
      error=mi_state_info_write(share->kfile,&share->state,1 | 2);
    }
    break;
  case HA_EXTRA_FORCE_REOPEN:
    pthread_mutex_lock(&THR_LOCK_myisam);
    share->last_version= 0L;			/* Impossible version */
#ifdef __WIN__
    /* Close the isam and data files as Win32 can't drop an open table */
    pthread_mutex_lock(&share->intern_lock);
    if (flush_key_blocks(share->kfile,FLUSH_RELEASE))
    {
      error=my_errno;
      share->changed=1;
      mi_mark_crashed(info);			/* Fatal error found */
    }
    if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
    {
      info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
      error=end_io_cache(&info->rec_cache);
    }
    if (info->lock_type != F_UNLCK && ! info->was_locked)
    {
      info->was_locked=info->lock_type;
      if (mi_lock_database(info,F_UNLCK))
	error=my_errno;
      info->lock_type = F_UNLCK;
    }
    if (share->kfile >= 0)
      _mi_decrement_open_count(info);
    if (share->kfile >= 0 && my_close(share->kfile,MYF(0)))
      error=my_errno;
    {
      LIST *list_element ;
      for (list_element=myisam_open_list ;
	   list_element ;
	   list_element=list_element->next)
      {
	MI_INFO *tmpinfo=(MI_INFO*) list_element->data;
	if (tmpinfo->s == info->s)
	{
	  if (tmpinfo->dfile >= 0 && my_close(tmpinfo->dfile,MYF(0)))
	    error = my_errno;
	  tmpinfo->dfile= -1;
	}
      }
    }
    share->kfile= -1;				/* Files aren't open anymore */
    pthread_mutex_unlock(&share->intern_lock);
#endif
    pthread_mutex_unlock(&THR_LOCK_myisam);
    break;
  case HA_EXTRA_FLUSH:
    if (!share->temporary)
      flush_key_blocks(share->kfile,FLUSH_KEEP);
#ifdef HAVE_PWRITE
    _mi_decrement_open_count(info);
#endif
    if (share->not_flushed)
    {
      share->not_flushed=0;
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
      if ( fsync(share->kfile))
	error=errno;
      if (fsync(share->dfile))
	error=errno;
#endif
      if (error)
      {
	share->changed=1;
	mi_mark_crashed(info);			/* Fatal error found */
      }
    }
    if (share->base.blobs)
    {
      my_free(info->rec_alloc,MYF(MY_ALLOW_ZERO_PTR));
      info->rec_alloc=info->rec_buff=0;
    }
    break;
  case HA_EXTRA_NORMAL:				/* Theese isn't in use */
    info->quick_mode=0;
    break;
  case HA_EXTRA_QUICK:
    info->quick_mode=1;
    break;
  case HA_EXTRA_NO_ROWS:
    if (!share->state.header.uniques)
      info->opt_flag|= OPT_NO_ROWS;
    break;
  case HA_EXTRA_KEY_CACHE:
  case HA_EXTRA_NO_KEY_CACHE:
  default:
    break;
  }
  {
    char tmp[1];
    tmp[0]=function;
    myisam_log_command(MI_LOG_EXTRA,info,(byte*) tmp,1,error);
  }
  DBUG_RETURN(error);
} /* mi_extra */
