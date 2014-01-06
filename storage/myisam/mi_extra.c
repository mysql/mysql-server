/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "myisamdef.h"
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

static void mi_extra_keyflag(MI_INFO *info, enum ha_extra_function function);


/*
  Set options and buffers to optimize table handling

  SYNOPSIS
    mi_extra()
    info	open table
    function	operation
    extra_arg	Pointer to extra argument (normally pointer to ulong)
    		Used when function is one of:
		HA_EXTRA_WRITE_CACHE
		HA_EXTRA_CACHE
  RETURN VALUES
    0  ok
    #  error
*/

int mi_extra(MI_INFO *info, enum ha_extra_function function, void *extra_arg)
{
  int error=0;
  ulong cache_size;
  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("mi_extra");
  DBUG_PRINT("enter",("function: %d",(int) function));

  switch (function) {
  case HA_EXTRA_RESET_STATE:		/* Reset state (don't free buffers) */
    info->lastinx= 0;			/* Use first index as def */
    info->last_search_keypage=info->lastpos= HA_OFFSET_ERROR;
    info->page_changed=1;
					/* Next/prev gives first/last */
    if (info->opt_flag & READ_CACHE_USED)
    {
      reinit_io_cache(&info->rec_cache,READ_CACHE,0,
		      (pbool) (info->lock_type != F_UNLCK),
		      (pbool) MY_TEST(info->update & HA_STATE_ROW_CHANGED)
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
    if (info->s->file_map) /* Don't use cache if mmap */
      break;
#if defined(HAVE_MMAP) && defined(HAVE_MADVISE)
    if ((share->options & HA_OPTION_COMPRESS_RECORD))
    {
      mysql_mutex_lock(&share->intern_lock);
      if (_mi_memmap_file(info))
      {
	/* We don't nead MADV_SEQUENTIAL if small file */
	madvise((char*) share->file_map, share->state.state.data_file_length,
		share->state.state.data_file_length <= RECORD_CACHE_SIZE*16 ?
		MADV_RANDOM : MADV_SEQUENTIAL);
        mysql_mutex_unlock(&share->intern_lock);
	break;
      }
      mysql_mutex_unlock(&share->intern_lock);
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
      cache_size= (extra_arg ? *(ulong*) extra_arg :
		   my_default_record_cache_size);
      if (!(init_io_cache(&info->rec_cache,info->dfile,
			 (uint) MY_MIN(info->state->data_file_length + 1,
                                       cache_size),
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
		      (pbool) MY_TEST(info->update & HA_STATE_ROW_CHANGED));
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

    cache_size= (extra_arg ? *(ulong*) extra_arg :
		 my_default_record_cache_size);
    if (!(info->opt_flag &
	  (READ_CACHE_USED | WRITE_CACHE_USED | OPT_NO_ROWS)) &&
	!share->state.header.uniques)
      if (!(init_io_cache(&info->rec_cache,info->dfile, cache_size,
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
  case HA_EXTRA_PREPARE_FOR_UPDATE:
    if (info->s->data_file_type != DYNAMIC_RECORD)
      break;
    /* Remove read/write cache if dynamic rows */
  case HA_EXTRA_NO_CACHE:
    if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
    {
      info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
      error=end_io_cache(&info->rec_cache);
      /* Sergei will insert full text index caching here */
    }
#if defined(HAVE_MMAP) && defined(HAVE_MADVISE)
    if (info->opt_flag & MEMMAP_USED)
      madvise((char*) share->file_map, share->state.state.data_file_length,
              MADV_RANDOM);
#endif
    break;
  case HA_EXTRA_FLUSH_CACHE:
    if (info->opt_flag & WRITE_CACHE_USED)
    {
      if ((error=flush_io_cache(&info->rec_cache)))
      {
        mi_print_error(info->s, HA_ERR_CRASHED);
	mi_mark_crashed(info);			/* Fatal error found */
      }
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
    bmove((uchar*) info->lastkey+share->base.max_key_length*2,
	  (uchar*) info->lastkey,info->lastkey_length);
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
      bmove((uchar*) info->lastkey,
	    (uchar*) info->lastkey+share->base.max_key_length*2,
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
    if (mi_is_any_key_active(share->state.key_map))
    {
      MI_KEYDEF *key=share->keyinfo;
      uint i;
      for (i=0 ; i < share->base.keys ; i++,key++)
      {
        if (!(key->flag & HA_NOSAME) && info->s->base.auto_key != i+1)
        {
          mi_clear_key_active(share->state.key_map, i);
          info->update|= HA_STATE_CHANGED;
        }
      }

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
    mysql_mutex_lock(&THR_LOCK_myisam);
    share->last_version= 0L;			/* Impossible version */
    mysql_mutex_unlock(&THR_LOCK_myisam);
    break;
  case HA_EXTRA_PREPARE_FOR_DROP:
    mysql_mutex_lock(&THR_LOCK_myisam);
    share->last_version= 0L;			/* Impossible version */
#ifdef __WIN__REMOVE_OBSOLETE_WORKAROUND
    /* Close the isam and data files as Win32 can't drop an open table */
    mysql_mutex_lock(&share->intern_lock);
    if (flush_key_blocks(share->key_cache, share->kfile,
			 (function == HA_EXTRA_FORCE_REOPEN ?
			  FLUSH_RELEASE : FLUSH_IGNORE_CHANGED)))
    {
      error=my_errno;
      share->changed=1;
      mi_print_error(info->s, HA_ERR_CRASHED);
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
    if (share->kfile >= 0 && mysql_file_close(share->kfile, MYF(0)))
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
          if (tmpinfo->dfile >= 0 && mysql_file_close(tmpinfo->dfile, MYF(0)))
	    error = my_errno;
	  tmpinfo->dfile= -1;
	}
      }
    }
    share->kfile= -1;				/* Files aren't open anymore */
    mysql_mutex_unlock(&share->intern_lock);
#endif
    mysql_mutex_unlock(&THR_LOCK_myisam);
    break;
  case HA_EXTRA_FLUSH:
    if (!share->temporary)
      flush_key_blocks(share->key_cache, share->kfile, FLUSH_KEEP);
#ifdef HAVE_PWRITE
    _mi_decrement_open_count(info);
#endif
    if (share->not_flushed)
    {
      share->not_flushed=0;
      if (mysql_file_sync(share->kfile, MYF(0)))
	error= my_errno;
      if (mysql_file_sync(info->dfile, MYF(0)))
	error= my_errno;
      if (error)
      {
	share->changed=1;
        mi_print_error(info->s, HA_ERR_CRASHED);
	mi_mark_crashed(info);			/* Fatal error found */
      }
    }
    if (share->base.blobs)
      mi_alloc_rec_buff(info, -1, &info->rec_buff);
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
  case HA_EXTRA_PRELOAD_BUFFER_SIZE:
    info->preload_buff_size= *((ulong *) extra_arg); 
    break;
  case HA_EXTRA_CHANGE_KEY_TO_UNIQUE:
  case HA_EXTRA_CHANGE_KEY_TO_DUP:
    mi_extra_keyflag(info, function);
    break;
  case HA_EXTRA_MMAP:
#ifdef HAVE_MMAP
    mysql_mutex_lock(&share->intern_lock);
    /*
      Memory map the data file if it is not already mapped. It is safe
      to memory map a file while other threads are using file I/O on it.
      Assigning a new address to a function pointer is an atomic
      operation. intern_lock prevents that two or more mappings are done
      at the same time.
    */
    if (!share->file_map)
    {
      if (mi_dynmap_file(info, share->state.state.data_file_length))
      {
        DBUG_PRINT("warning",("mmap failed: errno: %d",errno));
        error= my_errno= errno;
      }
    }
    mysql_mutex_unlock(&share->intern_lock);
#endif
    break;
  case HA_EXTRA_MARK_AS_LOG_TABLE:
    mysql_mutex_lock(&share->intern_lock);
    share->is_log_table= TRUE;
    mysql_mutex_unlock(&share->intern_lock);
    break;
  case HA_EXTRA_KEY_CACHE:
  case HA_EXTRA_NO_KEY_CACHE:
  default:
    break;
  }
  {
    char tmp[1];
    tmp[0]=function;
    myisam_log_command(MI_LOG_EXTRA,info,(uchar*) tmp,1,error);
  }
  DBUG_RETURN(error);
} /* mi_extra */


void mi_set_index_cond_func(MI_INFO *info, index_cond_func_t func,
                            void *func_arg)
{
  info->index_cond_func= func;
  info->index_cond_func_arg= func_arg;
}

/*
    Start/Stop Inserting Duplicates Into a Table, WL#1648.
 */
static void mi_extra_keyflag(MI_INFO *info, enum ha_extra_function function)
{
  uint  idx;

  for (idx= 0; idx< info->s->base.keys; idx++)
  {
    switch (function) {
    case HA_EXTRA_CHANGE_KEY_TO_UNIQUE:
      info->s->keyinfo[idx].flag|= HA_NOSAME;
      break;
    case HA_EXTRA_CHANGE_KEY_TO_DUP:
      info->s->keyinfo[idx].flag&= ~(HA_NOSAME);
      break;
    default:
      break;
    }
  }
}


int mi_reset(MI_INFO *info)
{
  int error= 0;
  MYISAM_SHARE *share=info->s;
  DBUG_ENTER("mi_reset");
  /*
    Free buffers and reset the following flags:
    EXTRA_CACHE, EXTRA_WRITE_CACHE, EXTRA_KEYREAD, EXTRA_QUICK

    If the row buffer cache is large (for dynamic tables), reduce it
    to save memory.
  */
  if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
  {
    info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
    error= end_io_cache(&info->rec_cache);
  }
  if (share->base.blobs)
    mi_alloc_rec_buff(info, -1, &info->rec_buff);
#if defined(HAVE_MMAP) && defined(HAVE_MADVISE)
  if (info->opt_flag & MEMMAP_USED)
    madvise((char*) share->file_map, share->state.state.data_file_length,
            MADV_RANDOM);
#endif
  info->opt_flag&= ~(KEY_READ_USED | REMEMBER_OLD_POS);
  info->quick_mode=0;
  info->lastinx= 0;			/* Use first index as def */
  info->last_search_keypage= info->lastpos= HA_OFFSET_ERROR;
  info->page_changed= 1;
  info->update= ((info->update & HA_STATE_CHANGED) | HA_STATE_NEXT_FOUND |
                 HA_STATE_PREV_FOUND);
  DBUG_RETURN(error);
}
