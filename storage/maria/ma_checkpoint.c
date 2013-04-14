/* Copyright (C) 2006,2007 MySQL AB

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
  WL#3071 Maria checkpoint
  First version written by Guilhem Bichot on 2006-04-27.
*/

/* Here is the implementation of this module */

/** @todo RECOVERY BUG this is unreviewed code */
/*
  Summary:
  checkpoints are done either by a background thread (checkpoint every Nth
  second) or by a client.
  In ha_maria, it's not made available to clients, and will soon be done by a
  background thread (periodically taking checkpoints and flushing dirty
  pages).
*/

#include "maria_def.h"
#include "ma_pagecache.h"
#include "ma_blockrec.h"
#include "ma_checkpoint.h"
#include "ma_loghandler_lsn.h"
#include "ma_servicethread.h"


/** @brief type of checkpoint currently running */
static CHECKPOINT_LEVEL checkpoint_in_progress= CHECKPOINT_NONE;
/** @brief protects checkpoint_in_progress */
static mysql_mutex_t LOCK_checkpoint;
/** @brief for killing the background checkpoint thread */
static mysql_cond_t  COND_checkpoint;
/** @brief control structure for checkpoint background thread */
static MA_SERVICE_THREAD_CONTROL checkpoint_control=
  {THREAD_DEAD, FALSE, &LOCK_checkpoint, &COND_checkpoint};
/* is ulong like pagecache->blocks_changed */
static ulong pages_to_flush_before_next_checkpoint;
static PAGECACHE_FILE *dfiles, /**< data files to flush in background */
  *dfiles_end; /**< list of data files ends here */
static PAGECACHE_FILE *kfiles, /**< index files to flush in background */
  *kfiles_end; /**< list of index files ends here */
/* those two statistics below could serve in SHOW GLOBAL STATUS */
static uint checkpoints_total= 0, /**< all checkpoint requests made */
  checkpoints_ok_total= 0; /**< all checkpoints which succeeded */

struct st_filter_param
{
  LSN up_to_lsn; /**< only pages with rec_lsn < this LSN */
  uint max_pages; /**< stop after flushing this number pages */
}; /**< information to determine which dirty pages should be flushed */

static enum pagecache_flush_filter_result
filter_flush_file_medium(enum pagecache_page_type type,
                         pgcache_page_no_t page,
                         LSN rec_lsn, void *arg);
static enum pagecache_flush_filter_result
filter_flush_file_full(enum pagecache_page_type type,
                       pgcache_page_no_t page,
                       LSN rec_lsn, void *arg);
static enum pagecache_flush_filter_result
filter_flush_file_evenly(enum pagecache_page_type type,
                         pgcache_page_no_t pageno,
                         LSN rec_lsn, void *arg);
static int really_execute_checkpoint(void);
pthread_handler_t ma_checkpoint_background(void *arg);
static int collect_tables(LEX_STRING *str, LSN checkpoint_start_log_horizon);

/**
   @brief Does a checkpoint

   @param  level               what level of checkpoint to do
   @param  no_wait             if another checkpoint of same or stronger level
                               is already running, consider our job done

   @note In ha_maria, there can never be two threads trying a checkpoint at
   the same time.

   @return Operation status
    @retval 0 ok
    @retval !=0 error
*/

int ma_checkpoint_execute(CHECKPOINT_LEVEL level, my_bool no_wait)
{
  int result= 0;
  DBUG_ENTER("ma_checkpoint_execute");

  if (!checkpoint_control.inited)
  {
    /*
      If ha_maria failed to start, maria_panic_hton is called, we come here.
    */
    DBUG_RETURN(0);
  }
  DBUG_ASSERT(level > CHECKPOINT_NONE);

  /* look for already running checkpoints */
  mysql_mutex_lock(&LOCK_checkpoint);
  while (checkpoint_in_progress != CHECKPOINT_NONE)
  {
    if (no_wait && (checkpoint_in_progress >= level))
    {
      /*
        If we are the checkpoint background thread, we don't wait (it's
        smarter to flush pages instead of waiting here while the other thread
        finishes its checkpoint).
      */
      mysql_mutex_unlock(&LOCK_checkpoint);
      goto end;
    }
    mysql_cond_wait(&COND_checkpoint, &LOCK_checkpoint);
  }

  checkpoint_in_progress= level;
  mysql_mutex_unlock(&LOCK_checkpoint);
  /* from then on, we are sure to be and stay the only checkpointer */

  result= really_execute_checkpoint();
  DBUG_EXECUTE_IF("maria_crash_after_checkpoint",
                  { DBUG_PRINT("maria_crash", ("now")); DBUG_ABORT(); });

  mysql_cond_broadcast(&COND_checkpoint);
end:
  DBUG_RETURN(result);
}


/**
   @brief Does a checkpoint, really; expects no other checkpoints
   running.

   Checkpoint level requested is read from checkpoint_in_progress.

   @return Operation status
    @retval 0   ok
    @retval !=0 error
*/

static int really_execute_checkpoint(void)
{
  uint i, error= 0;
  /** @brief checkpoint_start_log_horizon will be stored there */
  char *ptr;
  LEX_STRING record_pieces[4]; /**< only malloc-ed pieces */
  LSN min_page_rec_lsn, min_trn_rec_lsn, min_first_undo_lsn;
  TRANSLOG_ADDRESS checkpoint_start_log_horizon;
  char checkpoint_start_log_horizon_char[LSN_STORE_SIZE];
  DBUG_ENTER("really_execute_checkpoint");
  DBUG_PRINT("enter", ("level: %d", checkpoint_in_progress));
  bzero(&record_pieces, sizeof(record_pieces));

  /*
    STEP 1: record current end-of-log position using log's lock. It is
    critical for the correctness of Checkpoint (related to memory visibility
    rules, the log's lock is a mutex).
    "Horizon" is a lower bound of the LSN of the next log record.
  */
  checkpoint_start_log_horizon= translog_get_horizon();
  DBUG_PRINT("info",("checkpoint_start_log_horizon (%lu,0x%lx)",
                     LSN_IN_PARTS(checkpoint_start_log_horizon)));
  lsn_store(checkpoint_start_log_horizon_char, checkpoint_start_log_horizon);

  /*
    STEP 2: fetch information about transactions.
    We must fetch transactions before dirty pages. Indeed, a transaction
    first sets its rec_lsn then sets the page's rec_lsn then sets its rec_lsn
    to 0. If we fetched pages first, we may see no dirty page yet, then we
    fetch transactions but the transaction has already reset its rec_lsn to 0
    so we miss rec_lsn again.
    For a similar reason (over-allocated bitmap pages) we have to fetch
    transactions before flushing bitmap pages.

    min_trn_rec_lsn will serve to lower the starting point of the REDO phase
    (down from checkpoint_start_log_horizon).
 */
  if (unlikely(trnman_collect_transactions(&record_pieces[0],
                                           &record_pieces[1],
                                           &min_trn_rec_lsn,
                                           &min_first_undo_lsn)))
    goto err;


  /* STEP 3: fetch information about table files */
  if (unlikely(collect_tables(&record_pieces[2],
                              checkpoint_start_log_horizon)))
    goto err;


  /* STEP 4: fetch information about dirty pages */
  /*
    It's better to do it _after_ having flushed some data pages (which
    collect_tables() may have done), because those are now non-dirty and so we
    have a more up-to-date dirty pages list to put into the checkpoint record,
    and thus we will have less work at Recovery.
  */
  /* Using default pagecache for now */
  if (unlikely(pagecache_collect_changed_blocks_with_lsn(maria_pagecache,
                                                         &record_pieces[3],
                                                         &min_page_rec_lsn)))
    goto err;


  /* LAST STEP: now write the checkpoint log record */
  {
    LSN lsn;
    translog_size_t total_rec_length;
    /*
      the log handler is allowed to modify "str" and "length" (but not "*str")
      of its argument, so we must not pass it record_pieces directly,
      otherwise we would later not know what memory pieces to my_free().
    */
    LEX_CUSTRING log_array[TRANSLOG_INTERNAL_PARTS + 5];
    log_array[TRANSLOG_INTERNAL_PARTS + 0].str=
      (uchar*) checkpoint_start_log_horizon_char;
    log_array[TRANSLOG_INTERNAL_PARTS + 0].length= total_rec_length=
      sizeof(checkpoint_start_log_horizon_char);
    for (i= 0; i < (sizeof(record_pieces)/sizeof(record_pieces[0])); i++)
    {
      log_array[TRANSLOG_INTERNAL_PARTS + 1 + i]=
        *(LEX_CUSTRING *)&record_pieces[i];
      total_rec_length+= (translog_size_t) record_pieces[i].length;
    }
    if (unlikely(translog_write_record(&lsn, LOGREC_CHECKPOINT,
                                       &dummy_transaction_object, NULL,
                                       total_rec_length,
                                       sizeof(log_array)/sizeof(log_array[0]),
                                       log_array, NULL, NULL) ||
                 translog_flush(lsn)))
      goto err;
    translog_lock();
    /*
      This cannot be done as a inwrite_rec_hook of LOGREC_CHECKPOINT, because
      such hook would be called before translog_flush (and we must be sure
      that log was flushed before we write to the control file).
    */
    if (unlikely(ma_control_file_write_and_force(lsn, last_logno,
                                                 max_trid_in_control_file,
                                                 recovery_failures)))
    {
      translog_unlock();
      goto err;
    }
    translog_unlock();
  }

  /*
    Note that we should not alter memory structures until we have successfully
    written the checkpoint record and control file.
  */
  /* checkpoint succeeded */
  ptr= record_pieces[3].str;
  pages_to_flush_before_next_checkpoint= uint4korr(ptr);
  DBUG_PRINT("checkpoint",("%u pages to flush before next checkpoint",
                           (uint)pages_to_flush_before_next_checkpoint));

  /* compute log's low-water mark */
  {
    TRANSLOG_ADDRESS log_low_water_mark= min_page_rec_lsn;
    set_if_smaller(log_low_water_mark, min_trn_rec_lsn);
    set_if_smaller(log_low_water_mark, min_first_undo_lsn);
    set_if_smaller(log_low_water_mark, checkpoint_start_log_horizon);
    /**
       Now purge unneeded logs.
       As some systems have an unreliable fsync (drive lying), we could try to
       be robust against that: remember a few previous checkpoints in the
       control file, and not purge logs immediately... Think about it.
    */
    if (translog_purge(log_low_water_mark))
      ma_message_no_user(0, "log purging failed");
  }

  goto end;

err:
  error= 1;
  ma_message_no_user(0, "checkpoint failed");
  /* we were possibly not able to determine what pages to flush */
  pages_to_flush_before_next_checkpoint= 0;

end:
  for (i= 0; i < (sizeof(record_pieces)/sizeof(record_pieces[0])); i++)
    my_free(record_pieces[i].str);
  mysql_mutex_lock(&LOCK_checkpoint);
  checkpoint_in_progress= CHECKPOINT_NONE;
  checkpoints_total++;
  checkpoints_ok_total+= !error;
  mysql_mutex_unlock(&LOCK_checkpoint);
  DBUG_RETURN(error);
}


/**
   @brief Initializes the checkpoint module

   @param  interval           If one wants the module to create a
                              thread which will periodically do
                              checkpoints, and flush dirty pages, in the
                              background, it should specify a non-zero
                              interval in seconds. The thread will then be
                              created and will take checkpoints separated by
                              approximately 'interval' second.

   @note A checkpoint is taken only if there has been some significant
   activity since the previous checkpoint. Between checkpoint N and N+1 the
   thread flushes all dirty pages which were already dirty at the time of
   checkpoint N.

   @return Operation status
    @retval 0   ok
    @retval !=0 error
*/

int ma_checkpoint_init(ulong interval)
{
  pthread_t th;
  int res= 0;
  DBUG_ENTER("ma_checkpoint_init");
  if (ma_service_thread_control_init(&checkpoint_control))
    res= 1;
  else if (interval > 0)
  {
    compile_time_assert(sizeof(void *) >= sizeof(ulong));
    if (!(res= mysql_thread_create(key_thread_checkpoint,
                                   &th, NULL, ma_checkpoint_background,
                                   (void *)interval)))
    {
      /* thread lives, will have to be killed */
      checkpoint_control.status= THREAD_RUNNING;
    }
  }
  DBUG_RETURN(res);
}


#ifndef DBUG_OFF
/**
   Function used to test recovery: flush some table pieces and then caller
   crashes.

   @param  what_to_flush   0: current bitmap and all data pages
                           1: state
                           2: all bitmap pages
*/
static void flush_all_tables(int what_to_flush)
{
  int res= 0;
  LIST *pos; /**< to iterate over open tables */
  mysql_mutex_lock(&THR_LOCK_maria);
  for (pos= maria_open_list; pos; pos= pos->next)
  {
    MARIA_HA *info= (MARIA_HA*)pos->data;
    if (info->s->now_transactional)
    {
      switch (what_to_flush)
      {
      case 0:
        res= _ma_flush_table_files(info, MARIA_FLUSH_DATA | MARIA_FLUSH_INDEX,
                                   FLUSH_KEEP, FLUSH_KEEP);
        break;
      case 1:
        res= _ma_state_info_write(info->s,
                                  MA_STATE_INFO_WRITE_DONT_MOVE_OFFSET|
                                  MA_STATE_INFO_WRITE_LOCK);
        DBUG_PRINT("maria_flush_states",
                   ("is_of_horizon: LSN (%lu,0x%lx)",
                    LSN_IN_PARTS(info->s->state.is_of_horizon)));
        break;
      case 2:
        res= _ma_bitmap_flush_all(info->s);
        break;
      }
    }
    DBUG_ASSERT(res == 0);
  }
  mysql_mutex_unlock(&THR_LOCK_maria);
}
#endif


/**
   @brief Destroys the checkpoint module
*/

void ma_checkpoint_end(void)
{
  DBUG_ENTER("ma_checkpoint_end");
  /*
    Some intentional crash methods, usually triggered by
    SET MARIA_CHECKPOINT_INTERVAL=X
  */
  DBUG_EXECUTE_IF("maria_flush_bitmap",
                  {
                    DBUG_PRINT("maria_flush_bitmap", ("now"));
                    flush_all_tables(2);
                  });
  DBUG_EXECUTE_IF("maria_flush_whole_page_cache",
                  {
                    DBUG_PRINT("maria_flush_whole_page_cache", ("now"));
                    flush_all_tables(0);
                  });
  DBUG_EXECUTE_IF("maria_flush_whole_log",
                  {
                    DBUG_PRINT("maria_flush_whole_log", ("now"));
                    translog_flush(translog_get_horizon());
                  });
  /*
    Note that for WAL reasons, maria_flush_states requires
    maria_flush_whole_log.
  */
  DBUG_EXECUTE_IF("maria_flush_states",
                  {
                    DBUG_PRINT("maria_flush_states", ("now"));
                    flush_all_tables(1);
                  });
  DBUG_EXECUTE_IF("maria_crash",
                  { DBUG_PRINT("maria_crash", ("now")); DBUG_ABORT(); });

  if (checkpoint_control.inited)
  {
    ma_service_thread_control_end(&checkpoint_control);
    my_free(dfiles);
    my_free(kfiles);
    dfiles= kfiles= NULL;
  }
  DBUG_VOID_RETURN;
}


/**
   @brief dirty-page filtering criteria for MEDIUM checkpoint.

   We flush data/index pages which have been dirty since the previous
   checkpoint (this is the two-checkpoint rule: the REDO phase will not have
   to start from earlier than the next-to-last checkpoint).
   Bitmap pages are handled by _ma_bitmap_flush_all().

   @param  type                Page's type
   @param  pageno              Page's number
   @param  rec_lsn             Page's rec_lsn
   @param  arg                 filter_param
*/

static enum pagecache_flush_filter_result
filter_flush_file_medium(enum pagecache_page_type type,
                         pgcache_page_no_t pageno __attribute__ ((unused)),
                         LSN rec_lsn, void *arg)
{
  struct st_filter_param *param= (struct st_filter_param *)arg;
  return (type == PAGECACHE_LSN_PAGE) &&
    (cmp_translog_addr(rec_lsn, param->up_to_lsn) <= 0);
}


/**
   @brief dirty-page filtering criteria for FULL checkpoint.

   We flush all dirty data/index pages.
   Bitmap pages are handled by _ma_bitmap_flush_all().

   @param  type                Page's type
   @param  pageno              Page's number
   @param  rec_lsn             Page's rec_lsn
   @param  arg                 filter_param
*/

static enum pagecache_flush_filter_result
filter_flush_file_full(enum pagecache_page_type type,
                       pgcache_page_no_t pageno __attribute__ ((unused)),
                       LSN rec_lsn __attribute__ ((unused)),
                       void *arg __attribute__ ((unused)))
{
  return (type == PAGECACHE_LSN_PAGE);
}


/**
   @brief dirty-page filtering criteria for background flushing thread.

   We flush data/index pages which have been dirty since the previous
   checkpoint (this is the two-checkpoint rule: the REDO phase will not have
   to start from earlier than the next-to-last checkpoint), and no
   bitmap pages. But we flush no more than a certain number of pages (to have
   an even flushing, no write burst).
   The reason to not flush bitmap pages is that they may not be in a flushable
   state at this moment and we don't want to wait for them.

   @param  type                Page's type
   @param  pageno              Page's number
   @param  rec_lsn             Page's rec_lsn
   @param  arg                 filter_param
*/

static enum pagecache_flush_filter_result
filter_flush_file_evenly(enum pagecache_page_type type,
                         pgcache_page_no_t pageno __attribute__ ((unused)),
                         LSN rec_lsn, void *arg)
{
  struct st_filter_param *param= (struct st_filter_param *)arg;
  if (unlikely(param->max_pages == 0)) /* all flushed already */
    return FLUSH_FILTER_SKIP_ALL;
  if ((type == PAGECACHE_LSN_PAGE) &&
      (cmp_translog_addr(rec_lsn, param->up_to_lsn) <= 0))
  {
    param->max_pages--;
    return FLUSH_FILTER_OK;
  }
  return FLUSH_FILTER_SKIP_TRY_NEXT;
}


/**
   @brief Background thread which does checkpoints and flushes periodically.

   Takes a checkpoint. After this, all pages dirty at the time of that
   checkpoint are flushed evenly until it is time to take another checkpoint.
   This ensures that the REDO phase starts at earliest (in LSN time) at the
   next-to-last checkpoint record ("two-checkpoint rule").

   @note MikaelR questioned why the same thread does two different jobs, the
   risk could be that while a checkpoint happens no LRD flushing happens.
*/

static ulong maria_checkpoint_min_cache_activity= 10*1024*1024;
/* Set in ha_maria.cc */
ulong maria_checkpoint_min_log_activity= 1*1024*1024;

pthread_handler_t ma_checkpoint_background(void *arg)
{
  /** @brief At least this of log/page bytes written between checkpoints */
  /*
    If the interval could be changed by the user while we are in this thread,
    it could be annoying: for example it could cause "case 2" to be executed
    right after "case 0", thus having 'dfile' unset. So the thread cares only
    about the interval's value when it started.
  */
  const ulong interval= (ulong)arg;
  uint sleeps, sleep_time;
  TRANSLOG_ADDRESS log_horizon_at_last_checkpoint=
    translog_get_horizon();
  ulonglong pagecache_flushes_at_last_checkpoint=
    maria_pagecache->global_cache_write;
  uint UNINIT_VAR(pages_bunch_size);
  struct st_filter_param filter_param;
  PAGECACHE_FILE *UNINIT_VAR(dfile); /**< data file currently being flushed */
  PAGECACHE_FILE *UNINIT_VAR(kfile); /**< index file currently being flushed */

  my_thread_init();
  DBUG_PRINT("info",("Maria background checkpoint thread starts"));
  DBUG_ASSERT(interval > 0);

  /*
    Recovery ended with all tables closed and a checkpoint: no need to take
    one immediately.
  */
  sleeps= 1;
  pages_to_flush_before_next_checkpoint= 0;

  for(;;) /* iterations of checkpoints and dirty page flushing */
  {
#if 0 /* good for testing, to do a lot of checkpoints, finds a lot of bugs */
    sleeps=0;
#endif
    switch (sleeps % interval)
    {
    case 0:
    {
      /* If checkpoints are disabled, wait 1 second and try again */
      if (maria_checkpoint_disabled)
      {
        sleep_time= 1;
        break;
      }
      {
        TRANSLOG_ADDRESS horizon= translog_get_horizon();

        /*
          With background flushing evenly distributed over the time
          between two checkpoints, we should have only little flushing to do
          in the checkpoint.
        */
        /*
          No checkpoint if little work of interest for recovery was done
          since last checkpoint. Such work includes log writing (lengthens
          recovery, checkpoint would shorten it), page flushing (checkpoint
          would decrease the amount of read pages in recovery).
          In case of one short statement per minute (very low load), we don't
          want to checkpoint every minute, hence the positive
          maria_checkpoint_min_activity.
        */
        if ((ulonglong) (horizon - log_horizon_at_last_checkpoint) <=
            maria_checkpoint_min_log_activity &&
            ((ulonglong) (maria_pagecache->global_cache_write -
                          pagecache_flushes_at_last_checkpoint) *
             maria_pagecache->block_size) <=
            maria_checkpoint_min_cache_activity)
        {
          /*
            Not enough has happend since last checkpoint.
            Sleep for a while and try again later
          */
          sleep_time= interval;
          break;
        }
        sleep_time= 1;
        ma_checkpoint_execute(CHECKPOINT_MEDIUM, TRUE);
        /*
          Snapshot this kind of "state" of the engine. Note that the value
          below is possibly greater than last_checkpoint_lsn.
        */
        log_horizon_at_last_checkpoint= translog_get_horizon();
        pagecache_flushes_at_last_checkpoint=
          maria_pagecache->global_cache_write;
        /*
          If the checkpoint above succeeded it has set d|kfiles and
          d|kfiles_end. If is has failed, it has set
          pages_to_flush_before_next_checkpoint to 0 so we will skip flushing
          and sleep until the next checkpoint.
        */
      }
      break;
    }
    case 1:
      /* set up parameters for background page flushing */
      filter_param.up_to_lsn= last_checkpoint_lsn;
      pages_bunch_size= pages_to_flush_before_next_checkpoint / interval;
      dfile= dfiles;
      kfile= kfiles;
      /* fall through */
    default:
      if (pages_bunch_size > 0)
      {
        DBUG_PRINT("checkpoint",
                   ("Maria background checkpoint thread: %u pages",
                    pages_bunch_size));
        /* flush a bunch of dirty pages */
        filter_param.max_pages= pages_bunch_size;
        while (dfile != dfiles_end)
        {
          /*
            We use FLUSH_KEEP_LAZY: if a file is already in flush, it's
            smarter to move to the next file than wait for this one to be
            completely flushed, which may take long.
            StaleFilePointersInFlush: notice how below we use "dfile" which
            is an OS file descriptor plus some function and MARIA_SHARE
            pointers; this data dates from a previous checkpoint; since then,
            the table may have been closed (so MARIA_SHARE* became stale), and
            the file descriptor reassigned to another table which does not
            have the same CRC-read-set callbacks: it is thus important that
            flush_pagecache_blocks_with_filter() does not use the pointers,
            only the OS file descriptor.
          */
          int res=
            flush_pagecache_blocks_with_filter(maria_pagecache,
                                               dfile, FLUSH_KEEP_LAZY,
                                               filter_flush_file_evenly,
                                               &filter_param);
          if (unlikely(res & PCFLUSH_ERROR))
            ma_message_no_user(0, "background data page flush failed");
          if (filter_param.max_pages == 0) /* bunch all flushed, sleep */
            break; /* and we will continue with the same file */
          dfile++; /* otherwise all this file is flushed, move to next file */
          /*
            MikaelR noted that he observed that Linux's file cache may never
            fsync to  disk until this cache is full, at which point it decides
            to empty the cache, making the machine very slow. A solution was
            to fsync after writing 2 MB. So we might want to fsync() here if
            we wrote enough pages.
          */
        }
        while (kfile != kfiles_end)
        {
          int res=
            flush_pagecache_blocks_with_filter(maria_pagecache,
                                               kfile, FLUSH_KEEP_LAZY,
                                               filter_flush_file_evenly,
                                               &filter_param);
          if (unlikely(res & PCFLUSH_ERROR))
            ma_message_no_user(0, "background index page flush failed");
          if (filter_param.max_pages == 0) /* bunch all flushed, sleep */
            break; /* and we will continue with the same file */
          kfile++; /* otherwise all this file is flushed, move to next file */
        }
        sleep_time= 1;
      }
      else
      {
        /* Can directly sleep until the next checkpoint moment */
        sleep_time= interval - (sleeps % interval);
      }
    }
    if (my_service_thread_sleep(&checkpoint_control,
                                sleep_time * 1000000000ULL))
      break;
    sleeps+= sleep_time;
  }
  DBUG_PRINT("info",("Maria background checkpoint thread ends"));
  {
    CHECKPOINT_LEVEL level= CHECKPOINT_FULL;
    /*
      That's the final one, which guarantees that a clean shutdown always ends
      with a checkpoint.
    */
    DBUG_EXECUTE_IF("maria_checkpoint_indirect", level= CHECKPOINT_INDIRECT;);
    ma_checkpoint_execute(level, FALSE);
  }
  my_service_thread_signal_end(&checkpoint_control);
  my_thread_end();
  return 0;
}


/**
   @brief Allocates buffer and stores in it some info about open tables,
   does some flushing on those.

   Does the allocation because the caller cannot know the size itself.
   Memory freeing is to be done by the caller (if the "str" member of the
   LEX_STRING is not NULL).
   The caller is taking a checkpoint.

   @param[out]  str        pointer to where the allocated buffer,
                           and its size, will be put; buffer will be filled
                           with info about open tables
   @param       checkpoint_start_log_horizon  Of the in-progress checkpoint
                                              record.

   @return Operation status
     @retval 0      OK
     @retval 1      Error
*/

static int collect_tables(LEX_STRING *str, LSN checkpoint_start_log_horizon)
{
  MARIA_SHARE **distinct_shares= NULL;
  char *ptr;
  uint error= 1, sync_error= 0, nb, nb_stored, i;
  my_bool unmark_tables= TRUE;
  uint total_names_length;
  LIST *pos; /**< to iterate over open tables */
  struct st_state_copy {
    uint index;
    MARIA_STATE_INFO state;
  };
  struct st_state_copy *state_copies= NULL, /**< fixed-size cache of states */
    *state_copies_end, /**< cache ends here */
    *state_copy; /**< iterator in cache */
  TRANSLOG_ADDRESS UNINIT_VAR(state_copies_horizon); /**< horizon of states' _copies_ */
  struct st_filter_param filter_param;
  PAGECACHE_FLUSH_FILTER filter;
  DBUG_ENTER("collect_tables");

  /* let's make a list of distinct shares */
  mysql_mutex_lock(&THR_LOCK_maria);
  for (nb= 0, pos= maria_open_list; pos; pos= pos->next)
  {
    MARIA_HA *info= (MARIA_HA*)pos->data;
    MARIA_SHARE *share= info->s;
    /* the first three variables below can never change */
    if (share->base.born_transactional && !share->temporary &&
        share->mode != O_RDONLY &&
        !(share->in_checkpoint & MARIA_CHECKPOINT_SEEN_IN_LOOP))
    {
      /*
        Apart from us, only maria_close() reads/sets in_checkpoint but cannot
        run now as we hold THR_LOCK_maria.
      */
      /*
        This table is relevant for checkpoint and not already seen. Mark it,
        so that it is not seen again in the loop.
      */
      nb++;
      DBUG_ASSERT(share->in_checkpoint == 0);
      /* This flag ensures that we count only _distinct_ shares. */
      share->in_checkpoint= MARIA_CHECKPOINT_SEEN_IN_LOOP;
    }
  }
  if (unlikely((distinct_shares=
                (MARIA_SHARE **)my_malloc(nb * sizeof(MARIA_SHARE *),
                                          MYF(MY_WME))) == NULL))
    goto err;
  for (total_names_length= 0, i= 0, pos= maria_open_list; pos; pos= pos->next)
  {
    MARIA_HA *info= (MARIA_HA*)pos->data;
    MARIA_SHARE *share= info->s;
    if (share->in_checkpoint & MARIA_CHECKPOINT_SEEN_IN_LOOP)
    {
      distinct_shares[i++]= share;
      /*
        With this we prevent the share from going away while we later flush
        and force it without holding THR_LOCK_maria. For example if the share
        could be my_free()d by maria_close() we would have a problem when we
        access it to flush the table. We "pin" the share pointer.
        And we also take down MARIA_CHECKPOINT_SEEN_IN_LOOP, so that it is
        not seen again in the loop.
      */
      share->in_checkpoint= MARIA_CHECKPOINT_LOOKS_AT_ME;
      total_names_length+= share->open_file_name.length;
    }
  }

  DBUG_ASSERT(i == nb);
  mysql_mutex_unlock(&THR_LOCK_maria);
  DBUG_PRINT("info",("found %u table shares", nb));

  str->length=
    4 +               /* number of tables */
    (2 +              /* short id */
     LSN_STORE_SIZE + /* first_log_write_at_lsn */
     1                /* end-of-name 0 */
     ) * nb + total_names_length;
  if (unlikely((str->str= my_malloc(str->length, MYF(MY_WME))) == NULL))
    goto err;

  ptr= str->str;
  ptr+= 4; /* real number of stored tables is not yet know */

  /* only possible checkpointer, so can do the read below without mutex */
  filter_param.up_to_lsn= last_checkpoint_lsn;
  switch(checkpoint_in_progress)
  {
  case CHECKPOINT_MEDIUM:
    filter= &filter_flush_file_medium;
    break;
  case CHECKPOINT_FULL:
    filter= &filter_flush_file_full;
    break;
  case CHECKPOINT_INDIRECT:
    filter= NULL;
    break;
  default:
    DBUG_ASSERT(0);
    goto err;
  }

  /*
    The principle of reading/writing the state below is explained in
    ma_recovery.c, look for "Recovery of the state".
  */
#define STATE_COPIES 1024
  state_copies= (struct st_state_copy *)
    my_malloc(STATE_COPIES * sizeof(struct st_state_copy), MYF(MY_WME));
  dfiles= (PAGECACHE_FILE *)my_realloc((uchar *)dfiles,
                                       /* avoid size of 0 for my_realloc */
                                       max(1, nb) * sizeof(PAGECACHE_FILE),
                                       MYF(MY_WME | MY_ALLOW_ZERO_PTR));
  kfiles= (PAGECACHE_FILE *)my_realloc((uchar *)kfiles,
                                       /* avoid size of 0 for my_realloc */
                                       max(1, nb) * sizeof(PAGECACHE_FILE),
                                       MYF(MY_WME | MY_ALLOW_ZERO_PTR));
  if (unlikely((state_copies == NULL) ||
               (dfiles == NULL) || (kfiles == NULL)))
    goto err;
  state_copy= state_copies_end= NULL;
  dfiles_end= dfiles;
  kfiles_end= kfiles;

  for (nb_stored= 0, i= 0; i < nb; i++)
  {
    MARIA_SHARE *share= distinct_shares[i];
    PAGECACHE_FILE kfile, dfile;
    my_bool ignore_share;
    if (!(share->in_checkpoint & MARIA_CHECKPOINT_LOOKS_AT_ME))
    {
      /*
        No need for a mutex to read the above, only us can write *this* bit of
        the in_checkpoint bitmap
      */
      continue;
    }
    /**
       @todo We should not look at tables which didn't change since last
       checkpoint.
    */
    DBUG_PRINT("info",("looking at table '%s'", share->open_file_name.str));
    if (state_copy == state_copies_end) /* we have no more cached states */
    {
      /*
        Collect and cache a bunch of states. We do this for many states at a
        time, to not lock/unlock the log's lock too often.
      */
      uint j, bound= min(nb, i + STATE_COPIES);
      state_copy= state_copies;
      /* part of the state is protected by log's lock */
      translog_lock();
      state_copies_horizon= translog_get_horizon_no_lock();
      for (j= i; j < bound; j++)
      {
        MARIA_SHARE *share2= distinct_shares[j];
        if (!(share2->in_checkpoint & MARIA_CHECKPOINT_LOOKS_AT_ME))
          continue;
        state_copy->index= j;
        state_copy->state= share2->state; /* we copy the state */
        state_copy++;
        /*
          data_file_length is not updated under log's lock by the bitmap
          code, but writing a wrong data_file_length is ok: a next
          maria_close() will correct it; if we crash before, Recovery will
          set it to the true physical size.
        */
      }
      translog_unlock();
      if (state_copy == state_copies)
        break;                                  /* Nothing to do */

      /**
         We are going to flush these states.
         Before, all records describing how to undo such state must be
         in the log (WAL). Usually this means UNDOs. In the special case of
         data|key_file_length, recovery just needs to open the table to fix the
         length, so any LOGREC_FILE_ID/REDO/UNDO allowing recovery to
         understand it must open a table, is enough; so as long as
         data|key_file_length is updated after writing any log record it's ok:
         if we copied new value above, it means the record was before
         state_copies_horizon and we flush such record below.
         Apart from data|key_file_length which are easily recoverable from the
         real file's size, all other state members must be updated only when
         writing the UNDO; otherwise, if updated before, if their new value is
         flushed by a checkpoint and there is a crash before UNDO is written,
         their REDO group will be missing or at least incomplete and skipped
         by recovery, so bad state value will stay. For example, setting
         key_root before writing the UNDO: the table would have old index
         pages (they were pinned at time of crash) and a new, thus wrong,
         key_root.
         @todo RECOVERY BUG check that all code honours that.
      */
      if (translog_flush(state_copies_horizon))
        goto err;
      /* now we have cached states and they are WAL-safe*/
      state_copies_end= state_copy-1;
      state_copy= state_copies;
    }

    /* locate our state among these cached ones */
    for ( ; state_copy->index != i; state_copy++)
      DBUG_ASSERT(state_copy <= state_copies_end);

    /* OS file descriptors are ints which we stored in 4 bytes */
    compile_time_assert(sizeof(int) <= 4);
    /*
      Protect against maria_close() (which does some memory freeing in
      MARIA_FILE_BITMAP) with close_lock. intern_lock is not
      sufficient as we, as well as maria_close(), are going to unlock
      intern_lock in the middle of manipulating the table. Serializing us and
      maria_close() should help avoid problems.
    */
    mysql_mutex_lock(&share->close_lock);
    mysql_mutex_lock(&share->intern_lock);
    /*
      Tables in a normal state have their two file descriptors open.
      In some rare cases like REPAIR, some descriptor may be closed or even
      -1. If that happened, the _ma_state_info_write() may fail. This is
      prevented by enclosing all all places which close/change kfile.file with
      intern_lock.
    */
    kfile= share->kfile;
    dfile= share->bitmap.file;
    /*
      Ignore table which has no logged writes (all its future log records will
      be found naturally by Recovery). Ignore obsolete shares (_before_
      setting themselves to last_version=0 they already did all flush and
      sync; if we flush their state now we may be flushing an obsolete state
      onto a newer one (assuming the table has been reopened with a different
      share but of course same physical index file).
    */
    ignore_share= (share->id == 0) | (share->last_version == 0);
    DBUG_PRINT("info", ("ignore_share: %d", ignore_share));
    if (!ignore_share)
    {
      uint open_file_name_len= share->open_file_name.length + 1;
      /* remember the descriptors for background flush */
      *(dfiles_end++)= dfile;
      *(kfiles_end++)= kfile;
      /* we will store this table in the record */
      nb_stored++;
      int2store(ptr, share->id);
      ptr+= 2;
      lsn_store(ptr, share->lsn_of_file_id);
      ptr+= LSN_STORE_SIZE;
      /*
        first_bitmap_with_space is not updated under log's lock, and is
        important. We would need the bitmap's lock to get it right. Recovery
        of this is not clear, so we just play safe: write it out as
        unknown: if crash, _ma_bitmap_init() at next open (for example in
        Recovery) will convert it to 0 and thus the first insertion will
        search for free space from the file's first bitmap (0) -
        under-optimal but safe.
        If no crash, maria_close() will write the exact value.
      */
      state_copy->state.first_bitmap_with_space= ~(ulonglong)0;
      memcpy(ptr, share->open_file_name.str, open_file_name_len);
      ptr+= open_file_name_len;
      if (cmp_translog_addr(share->state.is_of_horizon,
                            checkpoint_start_log_horizon) >= 0)
      {
        /*
          State was flushed recently, it does not hold down the log's
          low-water mark and will not give avoidable work to Recovery. So we
          needn't flush it. Also, it is possible that while we copied the
          state above (under log's lock, without intern_lock) it was being
          modified in memory or flushed to disk (without log's lock, under
          intern_lock, like in maria_extra()), so our copy may be incorrect
          and we should not flush it.
          It may also be a share which got last_version==0 since we checked
          last_version; in this case, it flushed its state and the LSN test
          above will catch it.
        */
      }
      else
      {
        /*
          We could do the state flush only if share->changed, but it's
          tricky.
          Consider a maria_write() which has written REDO,UNDO, and before it
          calls _ma_writeinfo() (setting share->changed=1), checkpoint
          happens and sees share->changed=0, does not flush state. It is
          possible that Recovery does not start from before the REDO and thus
          the state is not recovered. A solution may be to set
          share->changed=1 under log mutex when writing log records.

          The current solution is to keep a copy the last saved state and
          not write the state if it was same as last time. It's ok if
          is_of_horizon would be different on disk if all other data is
          the same.
        */
        DBUG_ASSERT(share->last_version != 0);
        state_copy->state.is_of_horizon= share->state.is_of_horizon=
          share->checkpoint_state.is_of_horizon= state_copies_horizon;
        if (kfile.file >= 0 && memcmp(&share->checkpoint_state,
                                      &state_copy->state,
                                      sizeof(state_copy->state)))
        {
          sync_error|=
            _ma_state_info_write_sub(kfile.file, &state_copy->state,
                                     MA_STATE_INFO_WRITE_DONT_MOVE_OFFSET);
          memcpy(&share->checkpoint_state,
                 &state_copy->state, sizeof(state_copy->state));
        }
        /*
          We don't set share->changed=0 because it may interfere with a
          concurrent _ma_writeinfo() doing share->changed=1 (cancel its
          effect). The sad consequence is that we will flush the same state at
          each checkpoint if the table was once written and then not anymore.
        */
      }
    }
#ifdef EXTRA_DEBUG_BITMAP
    else
    {
      DBUG_ASSERT(share->bitmap.changed == 0 &&
                  share->bitmap.changed_not_flushed == 0);
    }
#endif

    /*
      _ma_bitmap_flush_all() may wait, so don't keep intern_lock as
      otherwise this would deadlock with allocate_and_write_block_record()
      calling _ma_set_share_data_file_length()
    */
    mysql_mutex_unlock(&share->intern_lock);
    
    if (!ignore_share)
    {
      /*
        share->bitmap is valid because it's destroyed under close_lock which
        we hold.
      */
      if (_ma_bitmap_flush_all(share))
      {
        sync_error= 1;
        /** @todo all write failures should mark table corrupted */
        ma_message_no_user(0, "checkpoint bitmap page flush failed");
      }
      DBUG_ASSERT(share->pagecache == maria_pagecache);
    }
    /*
      Clean up any unused states.
      TODO: Only do this call if there has been # (10?) ended transactions
      since last call.
      We had to release intern_lock to respect lock order with LOCK_trn_list.
    */
    _ma_remove_not_visible_states_with_lock(share, FALSE);

    if (share->in_checkpoint & MARIA_CHECKPOINT_SHOULD_FREE_ME)
    {
      /*
        maria_close() left us free the share. When it run it set share->id
        to 0. As it run before we locked close_lock, we should have seen this
        and so this assertion should be true:
      */
      DBUG_ASSERT(ignore_share);
      mysql_mutex_destroy(&share->intern_lock);
      mysql_mutex_unlock(&share->close_lock);
      mysql_mutex_destroy(&share->close_lock);
      my_free(share);
    }
    else
    {
      /* share goes back to normal state */
      share->in_checkpoint= 0;
      mysql_mutex_unlock(&share->close_lock);
    }

    /*
      We do the big disk writes out of intern_lock to not block other
      users of this table (intern_lock is taken at the start and end of
      every statement). This means that file descriptors may be invalid
      (files may have been closed for example by HA_EXTRA_PREPARE_FOR_*
      under Windows, or REPAIR). This should not be a problem as we use
      MY_IGNORE_BADFD. Descriptors may even point to other files but then
      the old blocks (of before the close) must have been flushed for sure,
      so our flush will flush new blocks (of after the latest open) and that
      should do no harm.
    */
    /*
      If CHECKPOINT_MEDIUM, this big flush below may result in a
      serious write burst. Realize that all pages dirtied between the
      last checkpoint and the one we are doing now, will be flushed at
      next checkpoint, except those evicted by LRU eviction (depending on
      the size of the page cache compared to the size of the working data
      set, eviction may be rare or frequent).
      We avoid that burst by anticipating: those pages are flushed
      in bunches spanned regularly over the time interval between now and
      the next checkpoint, by a background thread. Thus the next checkpoint
      will have only little flushing to do (CHECKPOINT_MEDIUM should thus be
      only a little slower than CHECKPOINT_INDIRECT).
    */

    /*
      PageCacheFlushConcurrencyBugs
      Inside the page cache, calls to flush_pagecache_blocks_int() on the same
      file are serialized. Examples of concurrency bugs which happened when we
      didn't have this serialization:
      - maria_chk_size() (via CHECK TABLE) happens concurrently with
      Checkpoint: Checkpoint is flushing a page: it pins the page and is
      pre-empted, maria_chk_size() wants to flush this page too so gets an
      error because Checkpoint pinned this page. Such error makes
      maria_chk_size() mark the table as corrupted.
      - maria_close() happens concurrently with Checkpoint:
      Checkpoint is flushing a page: it registers a request on the page, is
      pre-empted ; maria_close() flushes this page too with FLUSH_RELEASE:
      FLUSH_RELEASE will cause a free_block() which assumes the page is in the
      LRU, but it is not (as Checkpoint registered a request). Crash.
      - one thread is evicting a page of the file out of the LRU: it marks it
      iPC_BLOCK_IN_SWITCH and is pre-empted. Then two other threads do flushes
      of the same file concurrently (like above). Then one flusher sees the
      page is in switch, removes it from changed_blocks[] and puts it in its
      first_in_switch, so the other flusher will not see the page at all and
      return too early. If it's maria_close() which returns too early, then
      maria_close() may close the file descriptor, and the other flusher, and
      the evicter will fail to write their page: corruption.
    */

    if (!ignore_share)
    {
      if (filter != NULL)
      {
        if ((flush_pagecache_blocks_with_filter(maria_pagecache,
                                                &dfile, FLUSH_KEEP_LAZY,
                                                filter, &filter_param) &
             PCFLUSH_ERROR))
          ma_message_no_user(0, "checkpoint data page flush failed");
        if ((flush_pagecache_blocks_with_filter(maria_pagecache,
                                                &kfile, FLUSH_KEEP_LAZY,
                                                filter, &filter_param) &
             PCFLUSH_ERROR))
          ma_message_no_user(0, "checkpoint index page flush failed");
      }
      /*
        fsyncs the fd, that's the loooong operation (e.g. max 150 fsync
        per second, so if you have touched 1000 files it's 7 seconds).
      */
      sync_error|=
        mysql_file_sync(dfile.file, MYF(MY_WME | MY_IGNORE_BADFD)) |
        mysql_file_sync(kfile.file, MYF(MY_WME | MY_IGNORE_BADFD));
      /*
        in case of error, we continue because writing other tables to disk is
        still useful.
      */
    }
  }

  if (sync_error)
    goto err;
  /* We maybe over-estimated (due to share->id==0 or last_version==0) */
  DBUG_ASSERT(str->length >= (uint)(ptr - str->str));
  str->length= (uint)(ptr - str->str);
  /*
    As we support max 65k tables open at a time (2-byte short id), we
    assume uint is enough for the cumulated length of table names; and
    LEX_STRING::length is uint.
  */
  int4store(str->str, nb_stored);
  error= unmark_tables= 0;

err:
  if (unlikely(unmark_tables))
  {
    /* maria_close() uses THR_LOCK_maria from start to end */
    mysql_mutex_lock(&THR_LOCK_maria);
    for (i= 0; i < nb; i++)
    {
      MARIA_SHARE *share= distinct_shares[i];
      if (share->in_checkpoint & MARIA_CHECKPOINT_SHOULD_FREE_ME)
      {
        /* maria_close() left us to free the share */
        mysql_mutex_destroy(&share->intern_lock);
        my_free(share);
      }
      else
      {
        /* share goes back to normal state */
        share->in_checkpoint= 0;
      }
    }
    mysql_mutex_unlock(&THR_LOCK_maria);
  }
  my_free(distinct_shares);
  my_free(state_copies);
  DBUG_RETURN(error);
}
