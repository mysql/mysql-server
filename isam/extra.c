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

#include "isamdef.h"
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
#ifdef	__WIN__
#include <errno.h>
#endif

	/* set extra flags for database */

int nisam_extra(N_INFO *info, enum ha_extra_function function)
{
  int error=0;
  DBUG_ENTER("nisam_extra");

  switch (function) {
  case HA_EXTRA_RESET:
    if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
    {
      info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
      error=end_io_cache(&info->rec_cache);
    }
    info->opt_flag&= ~(KEY_READ_USED | REMEMBER_OLD_POS);

  case HA_EXTRA_RESET_STATE:
    info->lastinx= 0;			/* Use first index as def */
    info->int_pos=info->lastpos= NI_POS_ERROR;
    info->page_changed=1;
					/* Next/prev gives first/last */
    if (info->opt_flag & READ_CACHE_USED)
    {
      VOID(flush_io_cache(&info->rec_cache));
      reinit_io_cache(&info->rec_cache,READ_CACHE,0,
		      (pbool) (info->lock_type != F_UNLCK),
		      (pbool) test(info->update & HA_STATE_ROW_CHANGED));
    }
    info->update=((info->update & HA_STATE_CHANGED) |
		  HA_STATE_NEXT_FOUND | HA_STATE_PREV_FOUND);
    break;
  case HA_EXTRA_CACHE:
#ifndef NO_LOCKING
    if (info->lock_type == F_UNLCK && (info->options & HA_OPTION_PACK_RECORD))
    {
      error=1;			/* Not possibly if not locked */
      my_errno=EACCES;
      break;
    }
#endif
#if defined(HAVE_MMAP) && defined(HAVE_MADVICE)
    if ((info->options & HA_OPTION_COMPRESS_RECORD))
    {
      pthread_mutex_lock(&info->s->intern_lock);
      if (_nisam_memmap_file(info))
      {
	/* We don't nead MADV_SEQUENTIAL if small file */
	madvise(info->s->file_map,info->s->state.data_file_length,
		info->s->state.data_file_length <= RECORD_CACHE_SIZE*16 ?
		MADV_RANDOM : MADV_SEQUENTIAL);
	pthread_mutex_unlock(&info->s->intern_lock);
	break;
      }
      pthread_mutex_unlock(&info->s->intern_lock);
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
			 (uint) min(info->s->state.data_file_length+1,
				    my_default_record_cache_size),
			 READ_CACHE,0L,(pbool) (info->lock_type != F_UNLCK),
			 MYF(MY_WAIT_IF_FULL))))
      {
	info->opt_flag|=READ_CACHE_USED;
	info->update&= ~HA_STATE_ROW_CHANGED;
      }	
      /* info->rec_cache.end_of_file=info->s->state.data_file_length; */
    }
    break;
  case HA_EXTRA_REINIT_CACHE:
    if (info->opt_flag & READ_CACHE_USED)
    {
      reinit_io_cache(&info->rec_cache,READ_CACHE,info->nextpos,
		      (pbool) (info->lock_type != F_UNLCK),
		      (pbool) test(info->update & HA_STATE_ROW_CHANGED));
      info->update&= ~HA_STATE_ROW_CHANGED;
      /* info->rec_cache.end_of_file=info->s->state.data_file_length; */
    }
    break;
  case HA_EXTRA_WRITE_CACHE:
#ifndef NO_LOCKING
    if (info->lock_type == F_UNLCK)
    {
      error=1;			/* Not possibly if not locked */
      break;
    }
#endif
    if (!(info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED)))
      if (!(init_io_cache(&info->rec_cache,info->dfile,0,
			 WRITE_CACHE,info->s->state.data_file_length,
			 (pbool) (info->lock_type != F_UNLCK),
			 MYF(MY_WAIT_IF_FULL))))
      {
	info->opt_flag|=WRITE_CACHE_USED;
	info->update&= ~HA_STATE_ROW_CHANGED;
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
      madvise(info->s->file_map,info->s->state.data_file_length,MADV_RANDOM);
#endif
    break;
  case HA_EXTRA_FLUSH_CACHE:
    if (info->opt_flag & WRITE_CACHE_USED)
      error=flush_io_cache(&info->rec_cache);
    break;
  case HA_EXTRA_NO_READCHECK:
    info->opt_flag&= ~READ_CHECK_USED;	/* No readcheck */
    break;
  case HA_EXTRA_READCHECK:
    info->opt_flag|= READ_CHECK_USED;
    break;
  case HA_EXTRA_KEYREAD:			/* Read only keys to record */
  case HA_EXTRA_REMEMBER_POS:
    info->opt_flag |= REMEMBER_OLD_POS;
    bmove((byte*) info->lastkey+info->s->base.max_key_length*2,
	  (byte*) info->lastkey,info->s->base.max_key_length);
    info->save_update=	info->update;
    info->save_lastinx= info->lastinx;
    info->save_lastpos= info->lastpos;
    if (function == HA_EXTRA_REMEMBER_POS)
      break;
    /* fall through */
  case HA_EXTRA_KEYREAD_CHANGE_POS:
    info->opt_flag |= KEY_READ_USED;
    info->read_record=_nisam_read_key_record;
    break;
  case HA_EXTRA_NO_KEYREAD:
  case HA_EXTRA_RESTORE_POS:
    if (info->opt_flag & REMEMBER_OLD_POS)
    {
      bmove((byte*) info->lastkey,
	    (byte*) info->lastkey+info->s->base.max_key_length*2,
	    info->s->base.max_key_length);
      info->update=	info->save_update | HA_STATE_WRITTEN;
      info->lastinx=	info->save_lastinx;
      info->lastpos=	info->save_lastpos;
    }
    info->read_record=	info->s->read_record;
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
#ifndef NO_LOCKING
    if (info->lock_type == F_UNLCK)
    {
      error=1;					/* Not possibly if not lock */
      break;
    }
#endif
    info->s->state.keys=0;
    info->s->state.key_file_length=info->s->base.keystart;
    info->s->changed=1;				/* Update on close */
    break;
  case HA_EXTRA_FORCE_REOPEN:
    pthread_mutex_lock(&THR_LOCK_isam);
    info->s->last_version= 0L;			/* Impossible version */
#ifdef __WIN__
    /* Close the isam and data files as Win32 can't drop an open table */
    if (flush_key_blocks(info->s->kfile,FLUSH_RELEASE))
      error=my_errno;
    if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
    {
      info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
      error=end_io_cache(&info->rec_cache);
    }
    if (info->lock_type != F_UNLCK && ! info->was_locked)
    {
      info->was_locked=info->lock_type;
      if (nisam_lock_database(info,F_UNLCK))
	error=my_errno;
    }
    if (info->s->kfile >= 0 && my_close(info->s->kfile,MYF(0)))
      error=my_errno;
    {
      LIST *list_element ;
      for (list_element=nisam_open_list ;
	   list_element ;
	   list_element=list_element->next)
      {
	N_INFO *tmpinfo=(N_INFO*) list_element->data;
	if (tmpinfo->s == info->s)
	{
	  if (tmpinfo->dfile >= 0 && my_close(tmpinfo->dfile,MYF(0)))
	    error = my_errno;
	  tmpinfo->dfile=-1;
	}
      }
    }
    info->s->kfile=-1;				/* Files aren't open anymore */
#endif
    pthread_mutex_unlock(&THR_LOCK_isam);
    break;
  case HA_EXTRA_FLUSH:
#ifdef __WIN__
    if (info->s->not_flushed)
    {
      info->s->not_flushed=0;
      if (_commit(info->s->kfile))
	error=errno;
      if (_commit(info->dfile))
	error=errno;
    }
    break;
#endif
  case HA_EXTRA_NORMAL:				/* Theese isn't in use */
  case HA_EXTRA_QUICK:
  case HA_EXTRA_KEY_CACHE:
  case HA_EXTRA_NO_KEY_CACHE:
  default:
    break;
  }
  nisam_log_command(LOG_EXTRA,info,(byte*) &function,sizeof(function),error);
  DBUG_RETURN(error);
} /* nisam_extra */
