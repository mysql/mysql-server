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

#include "maria_def.h"
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include "ma_blockrec.h"

static void maria_extra_keyflag(MARIA_HA *info,
                                enum ha_extra_function function);

/**
   @brief Set options and buffers to optimize table handling

   @param  name             table's name
   @param  info             open table
   @param  function         operation
   @param  extra_arg        Pointer to extra argument (normally pointer to
                            ulong); used when function is one of:
                            HA_EXTRA_WRITE_CACHE
                            HA_EXTRA_CACHE

   @return Operation status
     @retval 0      ok
     @retval !=0    error
*/

int maria_extra(MARIA_HA *info, enum ha_extra_function function,
                void *extra_arg)
{
  int error= 0;
  ulong cache_size;
  MARIA_SHARE *share= info->s;
  my_bool block_records= share->data_file_type == BLOCK_RECORD;
  DBUG_ENTER("maria_extra");
  DBUG_PRINT("enter",("function: %d",(int) function));

  switch (function) {
  case HA_EXTRA_RESET_STATE:		/* Reset state (don't free buffers) */
    info->lastinx= 0;			/* Use first index as def */
    info->last_search_keypage= info->cur_row.lastpos= HA_OFFSET_ERROR;
    info->page_changed= 1;
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
    if (block_records)
      break;                                    /* Not supported */

    if (info->lock_type == F_UNLCK &&
	(share->options & HA_OPTION_PACK_RECORD))
    {
      error= 1;			/* Not possibly if not locked */
      my_errno= EACCES;
      break;
    }
    if (info->s->file_map) /* Don't use cache if mmap */
      break;
#if defined(HAVE_MMAP) && defined(HAVE_MADVISE)
    if ((share->options & HA_OPTION_COMPRESS_RECORD))
    {
      pthread_mutex_lock(&share->intern_lock);
      if (_ma_memmap_file(info))
      {
	/* We don't nead MADV_SEQUENTIAL if small file */
	madvise((char*) share->file_map, share->state.state.data_file_length,
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
      if ((error= end_io_cache(&info->rec_cache)))
	break;
    }
    if (!(info->opt_flag &
	  (READ_CACHE_USED | WRITE_CACHE_USED | MEMMAP_USED)))
    {
      cache_size= (extra_arg ? *(ulong*) extra_arg :
		   my_default_record_cache_size);
      if (!(init_io_cache(&info->rec_cache, info->dfile.file,
			 (uint) min(share->state.state.data_file_length+1,
				    cache_size),
			  READ_CACHE,0L,(pbool) (info->lock_type != F_UNLCK),
			  MYF(share->write_flag & MY_WAIT_IF_FULL))))
      {
	info->opt_flag|= READ_CACHE_USED;
	info->update&=   ~HA_STATE_ROW_CHANGED;
      }
      if (share->non_transactional_concurrent_insert)
	info->rec_cache.end_of_file= info->state->data_file_length;
    }
    break;
  case HA_EXTRA_REINIT_CACHE:
    if (info->opt_flag & READ_CACHE_USED)
    {
      reinit_io_cache(&info->rec_cache, READ_CACHE, info->cur_row.nextpos,
		      (pbool) (info->lock_type != F_UNLCK),
		      (pbool) test(info->update & HA_STATE_ROW_CHANGED));
      info->update&= ~HA_STATE_ROW_CHANGED;
      if (share->non_transactional_concurrent_insert)
	info->rec_cache.end_of_file= info->state->data_file_length;
    }
    break;
  case HA_EXTRA_WRITE_CACHE:
    if (info->lock_type == F_UNLCK)
    {
      error= 1;                        	/* Not possibly if not locked */
      break;
    }
    if (block_records)
      break;                            /* Not supported */

    cache_size= (extra_arg ? *(ulong*) extra_arg :
		 my_default_record_cache_size);
    if (!(info->opt_flag &
	  (READ_CACHE_USED | WRITE_CACHE_USED | OPT_NO_ROWS)) &&
	!share->state.header.uniques)
      if (!(init_io_cache(&info->rec_cache, info->dfile.file, cache_size,
			 WRITE_CACHE,share->state.state.data_file_length,
			  (pbool) (info->lock_type != F_UNLCK),
			  MYF(share->write_flag & MY_WAIT_IF_FULL))))
      {
	info->opt_flag|= WRITE_CACHE_USED;
	info->update&=   ~(HA_STATE_ROW_CHANGED |
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
      error= end_io_cache(&info->rec_cache);
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
      if ((error= flush_io_cache(&info->rec_cache)))
      {
        maria_print_error(info->s, HA_ERR_CRASHED);
	maria_mark_crashed(info);			/* Fatal error found */
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
    info->opt_flag|= REMEMBER_OLD_POS;
    bmove(info->last_key.data + share->base.max_key_length*2,
	  info->last_key.data,
          info->last_key.data_length + info->last_key.ref_length);
    info->save_update=	info->update;
    info->save_lastinx= info->lastinx;
    info->save_lastpos= info->cur_row.lastpos;
    info->save_lastkey_data_length= info->last_key.data_length;
    info->save_lastkey_ref_length= info->last_key.ref_length;
    if (function == HA_EXTRA_REMEMBER_POS)
      break;
    /* fall through */
  case HA_EXTRA_KEYREAD_CHANGE_POS:
    info->opt_flag|= KEY_READ_USED;
    info->read_record= _ma_read_key_record;
    break;
  case HA_EXTRA_NO_KEYREAD:
  case HA_EXTRA_RESTORE_POS:
    if (info->opt_flag & REMEMBER_OLD_POS)
    {
      bmove(info->last_key.data,
	    info->last_key.data + share->base.max_key_length*2,
	    info->save_lastkey_data_length + info->save_lastkey_ref_length);
      info->update=	info->save_update | HA_STATE_WRITTEN;
      info->lastinx=	info->save_lastinx;
      info->cur_row.lastpos= info->save_lastpos;
      info->last_key.data_length= info->save_lastkey_data_length;
      info->last_key.ref_length= info->save_lastkey_ref_length;
      info->last_key.flag= 0;
    }
    info->read_record=	share->read_record;
    info->opt_flag&= ~(KEY_READ_USED | REMEMBER_OLD_POS);
    break;
  case HA_EXTRA_NO_USER_CHANGE: /* Database is somehow locked agains changes */
    info->lock_type= F_EXTRA_LCK; /* Simulate as locked */
    break;
  case HA_EXTRA_WAIT_LOCK:
    info->lock_wait= 0;
    break;
  case HA_EXTRA_NO_WAIT_LOCK:
    info->lock_wait= MY_SHORT_WAIT;
    break;
  case HA_EXTRA_NO_KEYS:
    /* we're going to modify pieces of the state, stall Checkpoint */
    pthread_mutex_lock(&share->intern_lock);
    if (info->lock_type == F_UNLCK)
    {
      pthread_mutex_unlock(&share->intern_lock);
      error= 1;					/* Not possibly if not lock */
      break;
    }
    if (maria_is_any_key_active(share->state.key_map))
    {
      MARIA_KEYDEF *key= share->keyinfo;
      uint i;
      for (i =0 ; i < share->base.keys ; i++,key++)
      {
        if (!(key->flag & HA_NOSAME) && info->s->base.auto_key != i+1)
        {
          maria_clear_key_active(share->state.key_map, i);
          info->update|= HA_STATE_CHANGED;
        }
      }

      if (!share->changed)
      {
	share->state.changed|= STATE_CHANGED | STATE_NOT_ANALYZED;
	share->changed= 1;			/* Update on close */
	if (!share->global_changed)
	{
	  share->global_changed= 1;
	  share->state.open_count++;
	}
      }
      if (!share->now_transactional)
        share->state.state= *info->state;
      /*
        That state write to disk must be done, even for transactional tables;
        indeed the table's share is going to be lost (there was a
        HA_EXTRA_FORCE_REOPEN before, which set share->last_version to
        0), and so the only way it leaves information (share->state.key_map)
        for the posterity is by writing it to disk.
      */
      DBUG_ASSERT(!maria_in_recovery);
      error= _ma_state_info_write(share,
                                  MA_STATE_INFO_WRITE_DONT_MOVE_OFFSET |
                                  MA_STATE_INFO_WRITE_FULL_INFO);
    }
    pthread_mutex_unlock(&share->intern_lock);
    break;
  case HA_EXTRA_FORCE_REOPEN:
    /*
      MySQL uses this case after it has closed all other instances
      of this table.
      We however do a flush here for additional safety.
    */
    /** @todo consider porting these flush-es to MyISAM */
    DBUG_ASSERT(share->reopen == 1);
    error= _ma_flush_table_files(info, MARIA_FLUSH_DATA | MARIA_FLUSH_INDEX,
                                 FLUSH_FORCE_WRITE, FLUSH_FORCE_WRITE);
    if (!error && share->changed)
    {
      pthread_mutex_lock(&share->intern_lock);
      if (!(error= _ma_state_info_write(share,
                                        MA_STATE_INFO_WRITE_DONT_MOVE_OFFSET|
                                        MA_STATE_INFO_WRITE_FULL_INFO)))
        share->changed= 0;
      pthread_mutex_unlock(&share->intern_lock);
    }
    pthread_mutex_lock(&THR_LOCK_maria);
    pthread_mutex_lock(&share->intern_lock); /* protect against Checkpoint */
    /* this makes the share not be re-used next time the table is opened */
    share->last_version= 0L;			/* Impossible version */
    pthread_mutex_unlock(&share->intern_lock);
    pthread_mutex_unlock(&THR_LOCK_maria);
    break;
  case HA_EXTRA_PREPARE_FOR_DROP:
  case HA_EXTRA_PREPARE_FOR_RENAME:
  {
    my_bool do_flush= test(function != HA_EXTRA_PREPARE_FOR_DROP);
    enum flush_type type;
    pthread_mutex_lock(&THR_LOCK_maria);
    /*
      This share, to have last_version=0, needs to save all its data/index
      blocks to disk if this is not for a DROP TABLE. Otherwise they would be
      invisible to future openers; and they could even go to disk late and
      cancel the work of future openers.
    */
    if (info->lock_type != F_UNLCK && !info->was_locked)
    {
      info->was_locked= info->lock_type;
      if (maria_lock_database(info, F_UNLCK))
        error= my_errno;
      info->lock_type= F_UNLCK;
    }
    /*
      We don't need to call _mi_decrement_open_count() if we are
      dropping the table, as the files will be removed anyway. If we
      are aborted before the files is removed, it's better to not
      call it as in that case the automatic repair on open will add
      the missing index entries
    */
    pthread_mutex_lock(&share->intern_lock);
    if (share->kfile.file >= 0 && function != HA_EXTRA_PREPARE_FOR_DROP)
      _ma_decrement_open_count(info);
    if (info->trn)
    {
      _ma_remove_table_from_trnman(share, info->trn);
      /* Ensure we don't point to the deleted data in trn */
      info->state= info->state_start= &share->state.state;
    }

    type= do_flush ? FLUSH_RELEASE : FLUSH_IGNORE_CHANGED;
    if (_ma_flush_table_files(info, MARIA_FLUSH_DATA | MARIA_FLUSH_INDEX,
                              type, type))
    {
      error=my_errno;
      share->changed= 1;
    }
    if (info->opt_flag & (READ_CACHE_USED | WRITE_CACHE_USED))
    {
      info->opt_flag&= ~(READ_CACHE_USED | WRITE_CACHE_USED);
      if (end_io_cache(&info->rec_cache))
        error= 1;
    }
    if (share->kfile.file >= 0)
    {
      if (do_flush)
      {
        /* Save the state so that others can find it from disk. */
        if ((share->changed &&
             _ma_state_info_write(share,
                                  MA_STATE_INFO_WRITE_DONT_MOVE_OFFSET |
                                  MA_STATE_INFO_WRITE_FULL_INFO)) ||
            my_sync(share->kfile.file, MYF(0)))
          error= my_errno;
        else
          share->changed= 0;
      }
      else
      {
        /* be sure that state is not tried for write as file may be closed */
        share->changed= 0;
      }
    }
    if (share->data_file_type == BLOCK_RECORD &&
        share->bitmap.file.file >= 0)
    {
      if (do_flush && my_sync(share->bitmap.file.file, MYF(0)))
        error= my_errno;
    }
    /* For protection against Checkpoint, we set under intern_lock: */
    share->last_version= 0L;			/* Impossible version */
    pthread_mutex_unlock(&share->intern_lock);
    pthread_mutex_unlock(&THR_LOCK_maria);
    break;
  }
  case HA_EXTRA_PREPARE_FOR_FORCED_CLOSE:
    if (info->trn)
    {
      pthread_mutex_lock(&share->intern_lock);
      _ma_remove_table_from_trnman(share, info->trn);
      /* Ensure we don't point to the deleted data in trn */
      info->state= info->state_start= &share->state.state;
      pthread_mutex_unlock(&share->intern_lock);    
    }
    break;
  case HA_EXTRA_FLUSH:
    if (!share->temporary)
      error= _ma_flush_table_files(info, MARIA_FLUSH_DATA | MARIA_FLUSH_INDEX,
                                   FLUSH_KEEP, FLUSH_KEEP);
#ifdef HAVE_PWRITE
    _ma_decrement_open_count(info);
#endif
    if (share->not_flushed)
    {
      share->not_flushed= 0;
      if (_ma_sync_table_files(info))
	error= my_errno;
      if (error)
      {
	share->changed= 1;
        maria_print_error(info->s, HA_ERR_CRASHED);
	maria_mark_crashed(info);			/* Fatal error found */
      }
    }
    break;
  case HA_EXTRA_NORMAL:				/* Theese isn't in use */
    info->quick_mode= 0;
    break;
  case HA_EXTRA_QUICK:
    info->quick_mode= 1;
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
    maria_extra_keyflag(info, function);
    break;
  case HA_EXTRA_MMAP:
#ifdef HAVE_MMAP
    if (block_records)
      break;                                    /* Not supported */
    pthread_mutex_lock(&share->intern_lock);
    /*
      Memory map the data file if it is not already mapped. It is safe
      to memory map a file while other threads are using file I/O on it.
      Assigning a new address to a function pointer is an atomic
      operation. intern_lock prevents that two or more mappings are done
      at the same time.
    */
    if (!share->file_map)
    {
      if (_ma_dynmap_file(info, share->state.state.data_file_length))
      {
        DBUG_PRINT("warning",("mmap failed: errno: %d",errno));
        error= my_errno= errno;
      }
      else
      {
        share->file_read=  _ma_mmap_pread;
        share->file_write= _ma_mmap_pwrite;
      }
    }
    pthread_mutex_unlock(&share->intern_lock);
#endif
    break;
  case HA_EXTRA_MARK_AS_LOG_TABLE:
    pthread_mutex_lock(&share->intern_lock);
    share->is_log_table= TRUE;
    pthread_mutex_unlock(&share->intern_lock);
    break;
  case HA_EXTRA_KEY_CACHE:
  case HA_EXTRA_NO_KEY_CACHE:
  default:
    break;
  }
  DBUG_RETURN(error);
} /* maria_extra */


/*
  Start/Stop Inserting Duplicates Into a Table, WL#1648.
*/

static void maria_extra_keyflag(MARIA_HA *info,
                                enum ha_extra_function function)
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


int maria_reset(MARIA_HA *info)
{
  int error= 0;
  MARIA_SHARE *share= info->s;
  DBUG_ENTER("maria_reset");
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
  /* Free memory used for keeping blobs */
  if (share->base.blobs)
  {
    if (info->rec_buff_size > share->base.default_rec_buff_size)
    {
      info->rec_buff_size= 1;                 /* Force realloc */
      _ma_alloc_buffer(&info->rec_buff, &info->rec_buff_size,
                       share->base.default_rec_buff_size);
    }
    if (info->blob_buff_size > MARIA_SMALL_BLOB_BUFFER)
    {
      info->blob_buff_size= 1;                 /* Force realloc */
      _ma_alloc_buffer(&info->blob_buff, &info->blob_buff_size,
                       MARIA_SMALL_BLOB_BUFFER);
    }
  }
#if defined(HAVE_MMAP) && defined(HAVE_MADVISE)
  if (info->opt_flag & MEMMAP_USED)
    madvise((char*) share->file_map, share->state.state.data_file_length,
            MADV_RANDOM);
#endif
  info->opt_flag&= ~(KEY_READ_USED | REMEMBER_OLD_POS);
  info->quick_mode= 0;
  info->lastinx= 0;			/* Use first index as def */
  info->last_search_keypage= info->cur_row.lastpos= HA_OFFSET_ERROR;
  info->page_changed= 1;
  info->update= ((info->update & HA_STATE_CHANGED) | HA_STATE_NEXT_FOUND |
                 HA_STATE_PREV_FOUND);
  DBUG_RETURN(error);
}


int _ma_sync_table_files(const MARIA_HA *info)
{
  return (my_sync(info->dfile.file, MYF(MY_WME)) ||
          my_sync(info->s->kfile.file, MYF(MY_WME)));
}


/**
   @brief flushes the data and/or index file of a table

   This is useful when one wants to read a table using OS syscalls (like
   my_copy()) and first wants to be sure that MySQL-level caches go down to
   the OS so that OS syscalls can see all data. It can flush rec_cache,
   bitmap, pagecache of data file, pagecache of index file.

   @param  info                table
   @param  flush_data_or_index one or two of these flags:
                               MARIA_FLUSH_DATA, MARIA_FLUSH_INDEX
   @param  flush_type_for_data
   @param  flush_type_for_index

   @note does not sync files (@see _ma_sync_table_files()).
   @note Progressively this function will be used in all places where we flush
   the index but not the data file (probable bugs).

   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

int _ma_flush_table_files(MARIA_HA *info, uint flush_data_or_index,
                          enum flush_type flush_type_for_data,
                          enum flush_type flush_type_for_index)
{
  int error= 0;
  MARIA_SHARE *share= info->s;
  /* flush data file first because it's more critical */
  if (flush_data_or_index & MARIA_FLUSH_DATA)
  {
    if ((info->opt_flag & WRITE_CACHE_USED) &&
        flush_type_for_data != FLUSH_IGNORE_CHANGED &&
        flush_io_cache(&info->rec_cache))
      error= 1;
    if (share->data_file_type == BLOCK_RECORD)
    {
      if (flush_type_for_data != FLUSH_IGNORE_CHANGED)
      {
        if (_ma_bitmap_flush(share))
          error= 1;
      }
      else
      {
        pthread_mutex_lock(&share->bitmap.bitmap_lock);
        share->bitmap.changed= 0;
        pthread_mutex_unlock(&share->bitmap.bitmap_lock);
      }
      if (flush_pagecache_blocks(share->pagecache, &info->dfile,
                                 flush_type_for_data))
        error= 1;
    }
  }
  if ((flush_data_or_index & MARIA_FLUSH_INDEX) &&
      flush_pagecache_blocks(share->pagecache, &share->kfile,
                             flush_type_for_index))
    error= 1;
  if (!error)
    return 0;

  maria_print_error(info->s, HA_ERR_CRASHED);
  maria_mark_crashed(info);
  return 1;
}
