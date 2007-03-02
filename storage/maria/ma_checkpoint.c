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
  WL#3071 Maria checkpoint
  First version written by Guilhem Bichot on 2006-04-27.
  Does not compile yet.
*/

/* Here is the implementation of this module */

/*
  Summary:
  - there are asynchronous checkpoints (a writer to the log notices that it's
  been a long time since we last checkpoint-ed, so posts a request for a
  background thread to do a checkpoint; does not care about the success of the
  checkpoint). Then the checkpoint is done by the checkpoint thread, at an
  unspecified moment ("later") (==soon, of course).
  - there are synchronous checkpoints: a thread requests a checkpoint to
  happen now and wants to know when it finishes and if it succeeded; then the
  checkpoint is done by that same thread.
*/

#include "page_cache.h"
#include "least_recently_dirtied.h"
#include "transaction.h"
#include "share.h"
#include "log.h"

#define LSN_IMPOSSIBLE ((LSN)0) /* could also be called LSN_ERROR */
#define LSN_MAX ((LSN)ULONGLONG_MAX)

/*
  this transaction is used for any system work (purge, checkpoint writing
  etc), that is, background threads. It will not be declared/initialized here
  in the final version.
*/
st_transaction system_trans= {0 /* long trans id */, 0 /* short trans id */,0,...};

/* those three are protected by the log's mutex */
/*
  The maximum rec_lsn in the LRD when last checkpoint was run, serves for the
  MEDIUM checkpoint.
*/
LSN max_rec_lsn_at_last_checkpoint= 0;
/* last submitted checkpoint request; cleared when starts */
CHECKPOINT_LEVEL next_asynchronous_checkpoint_to_do= NONE;
CHECKPOINT_LEVEL checkpoint_in_progress= NONE;

static inline ulonglong read_non_atomic(ulonglong volatile *x);

/*
  Used by MySQL client threads requesting a checkpoint (like "ALTER MARIA
  ENGINE DO CHECKPOINT"), and probably by maria_panic(), and at the end of the
  UNDO recovery phase.
*/
my_bool execute_synchronous_checkpoint(CHECKPOINT_LEVEL level)
{
  my_bool result;
  DBUG_ENTER("execute_synchronous_checkpoint");
  DBUG_ASSERT(level > NONE);

  lock(log_mutex);
  while (checkpoint_in_progress != NONE)
    wait_on_checkpoint_done_cond();

  result= execute_checkpoint(level);
  DBUG_RETURN(result);
}

/*
  If no checkpoint is running, and there is a pending asynchronous checkpoint
  request, executes it.
  Is safe if multiple threads call it, though in first version only one will.
  It's intended to be used by a thread which regularly calls this function;
  this is why, if there is a request, it does not wait in a loop for
  synchronous checkpoints to be finished, but just exits (because the thread
  may want to do something useful meanwhile (flushing dirty pages for example)
  instead of waiting).
*/
my_bool execute_asynchronous_checkpoint_if_any()
{
  my_bool result;
  CHECKPOINT_LEVEL level;
  DBUG_ENTER("execute_asynchronous_checkpoint");

  /* first check without mutex, ok to see old data */
  if (likely((next_asynchronous_checkpoint_to_do == NONE) ||
             (checkpoint_in_progress != NONE)))
    DBUG_RETURN(FALSE);

  lock(log_mutex);
  if (likely((next_asynchronous_checkpoint_to_do == NONE) ||
             (checkpoint_in_progress != NONE)))
  {
    unlock(log_mutex);
    DBUG_RETURN(FALSE);
  }

  result= execute_checkpoint(next_asynchronous_checkpoint_to_do);
  DBUG_RETURN(result);
}


/*
  Does the actual checkpointing. Called by
  execute_synchronous_checkpoint() and
  execute_asynchronous_checkpoint_if_any().
*/
my_bool execute_checkpoint(CHECKPOINT_LEVEL level)
{
  my_bool result;
  DBUG_ENTER("execute_checkpoint");

  safemutex_assert_owner(log_mutex);
  if (next_asynchronous_checkpoint_to_do <= level)
    next_asynchronous_checkpoint_to_do= NONE;
  checkpoint_in_progress= level;

  if (unlikely(level > INDIRECT))
  {
    LSN copy_of_max_rec_lsn_at_last_checkpoint=
      max_rec_lsn_at_last_checkpoint;
    /* much I/O work to do, release log mutex */
    unlock(log_mutex);

    switch (level)
    {
    case FULL:
      /* flush all pages up to the current end of the LRD */
      flush_all_LRD_to_lsn(LSN_MAX);
      /* this will go full speed (normal scheduling, no sleep) */
      break;
    case MEDIUM:
      /*
        flush all pages which were already dirty at last checkpoint:
        ensures that recovery will never start from before the next-to-last
        checkpoint (two-checkpoint rule).
      */
      flush_all_LRD_to_lsn(copy_of_max_rec_lsn_at_last_checkpoint);
      /* this will go full speed (normal scheduling, no sleep) */
      break;
    }
    lock(log_mutex);
  }

  result= execute_checkpoint_indirect();
  checkpoint_in_progress= NONE;
  unlock(log_mutex);
  broadcast(checkpoint_done_cond);
  DBUG_RETURN(result);
}


/*
  Does an indirect checpoint (collects data from data structures, writes into
  a checkpoint log record).
  Starts and ends while having log's mutex (released in the middle).
*/
my_bool execute_checkpoint_indirect()
{
  int error= 0, i;
  /* checkpoint record data: */
  LSN checkpoint_start_lsn;
  char checkpoint_start_lsn_char[8];
  LEX_STRING strings[6]=
    {checkpoint_start_lsn_char, 8}, {0,0}, {0,0}, {0,0}, {0,0}, {0,0} };
  char *ptr;
  LSN checkpoint_lsn;
  LSN candidate_max_rec_lsn_at_last_checkpoint;
  DBUG_ENTER("execute_checkpoint_indirect");

  DBUG_ASSERT(sizeof(byte *) <= 8);
  DBUG_ASSERT(sizeof(LSN) <= 8);

  safemutex_assert_owner(log_mutex);

  /* STEP 1: record current end-of-log LSN */
  checkpoint_start_lsn= log_read_end_lsn();
  if (LSN_IMPOSSIBLE == checkpoint_start_lsn) /* error */
    DBUG_RETURN(TRUE);
  unlock(log_mutex);

  DBUG_PRINT("info",("checkpoint_start_lsn %lu", checkpoint_start_lsn));
  int8store(strings[0].str, checkpoint_start_lsn);

  /* STEP 2: fetch information about dirty pages */

  if (pagecache_collect_changed_blocks_with_LSN(pagecache, &strings[1],
                                                &candidate_max_rec_lsn_at_last_checkpoint))
    goto err;

  /* STEP 3: fetch information about transactions */
  if (trnman_collect_transactions(&strings[2], &strings[3]))
    goto err;

  /* STEP 4: fetch information about table files */

  {
    /* This global mutex is in fact THR_LOCK_maria (see ma_open()) */
    lock(global_share_list_mutex);
    strings[4].length= 8+(8+8)*share_list->count;
    if (NULL == (strings[4].str= my_malloc(strings[4].length)))
      goto err;
    ptr= string3.str;
    /*
      Note that maria_open_list is a list of MARIA_HA*, while we would prefer
      a list of MARIA_SHARE* here (we are interested in the short id,
      unique file name, members of MARIA_SHARE*, and in file descriptors,
      which will in the end be in MARIA_SHARE*).
    */
    for (iterate on the maria_open_list)
    {
      /* latch each MARIA_SHARE, one by one, like this: */
      pthread_mutex_lock(&share->intern_lock);
      /*
        TODO:
        we need to prevent the share from going away while we later flush and
        force it without holding THR_LOCK_maria. For example if the share is
        free()d by maria_close() we'll have a problem. Or if the share's file
        descriptor is closed by maria_close() we will not be able to my_sync()
        it.
      */
      pthread_mutex_unlock(&share->intern_lock);
      store the share pointer into a private array;
    }
    unlock(global_share_list_mutex);

    /* work on copy */
    int8store(ptr, elements_in_array);
    ptr+= 8;
    for (el in array)
    {
      int8store(ptr, array[...].short_id);
      ptr+= 8;
      memcpy(ptr, array[...].unique_file_name[_length], ...);
      ptr+= ...;
      /* maybe we need to lock share->intern_lock here */
      /*
        these two are long ops (involving disk I/O) that's why we copied the
        list, to not keep the list locked for long:
      */
      flush_bitmap_pages(el);
      /* TODO: and also autoinc counter, logical file end, free page list */

      /*
        fsyncs the fd, that's the loooong operation (e.g. max 150 fsync per
        second, so if you have touched 1000 files it's 7 seconds).
      */
      force_file(el);
    }
  }

  /* LAST STEP: now write the checkpoint log record */

  checkpoint_lsn= log_write_record(LOGREC_CHECKPOINT,
                                   &system_trans, strings);

  /*
    Do nothing between the log write and the control file write, for the
    "repair control file" tool to be possible one day.
  */

  if (LSN_IMPOSSIBLE == checkpoint_lsn)
    goto err;

  if (0 != control_file_write_and_force(checkpoint_lsn, NULL))
    goto err;

  /*
    Note that we should not alter memory structures until we have successfully
    written the checkpoint record and control file.
    Btw, a log write failure is serious:
    - if we know how many bytes we managed to write, we should try to write
    more, keeping the log's mutex (MY_FULL_IO)
    - if we don't know, this log record is corrupted and we have no way to
    "de-corrupt" it, so it will stay corrupted, and as the log is sequential,
    any log record written after it will not be reachable (for example if we
    would write UNDOs and crash, we would not be able to read the log and so
    not be able to rollback), so we should stop the engine now (holding the
    log's mutex) and do a recovery.
  */
  goto end;

err:
  print_error_to_error_log(the_error_message);
  candidate_max_rec_lsn_at_last_checkpoint= LSN_IMPOSSIBLE;

end:

  for (i= 1; i<6; i++)
    my_free(strings[i].str, MYF(MY_ALLOW_ZERO_PTR));

  /*
    this portion cannot be done as a hook in write_log_record() for the
    LOGREC_CHECKPOINT type because:
    - at that moment we still have not written to the control file so cannot
    mark the request as done; this could be solved by writing to the control
    file in the hook but that would be an I/O under the log's mutex, bad.
    - it would not be nice organisation of code (I tried it :).
  */
  if (candidate_max_rec_lsn_at_last_checkpoint != LSN_IMPOSSIBLE)
  {
    /* checkpoint succeeded */
    /*
      TODO: compute log's low water mark (how to do that with our fuzzy
      ARIES-like reads of data structures? TODO think about it :).
    */
    lock(log_mutex);
    /* That LSN is used for the "two-checkpoint rule" (MEDIUM checkpoints) */
    maximum_rec_lsn_last_checkpoint= candidate_max_rec_lsn_at_last_checkpoint;
    DBUG_RETURN(FALSE);
  }
  lock(log_mutex);
  DBUG_RETURN(TRUE);
  /*
    keep mutex locked upon exit because callers will want to clear
    mutex-protected status variables
  */
}



/*
  Here's what should be put in log_write_record() in the log handler:
*/
log_write_record(...)
{
  ...;
  lock(log_mutex);
  ...;
  write_to_log(length);
  written_since_last_checkpoint+= length;
  if (written_since_last_checkpoint >
      MAX_LOG_BYTES_WRITTEN_BETWEEN_CHECKPOINTS)
  {
    /*
      ask one system thread (the "LRD background flusher and checkpointer
      thread" WL#3261) to do a checkpoint
    */
    request_asynchronous_checkpoint(INDIRECT);
    /* prevent similar redundant requests */
    written_since_last_checkpoint= (my_off_t)0;
  }
  ...;
  unlock(log_mutex);
  ...;
}

/*
  Requests a checkpoint from the background thread, *asynchronously*
  (requestor does not wait for completion, and does not even later check the
  result).
  In real life it will be called by log_write_record().
*/
void request_asynchronous_checkpoint(CHECKPOINT_LEVEL level);
{
  safemutex_assert_owner(log_mutex);

  DBUG_ASSERT(level > NONE);
  if ((next_asynchronous_checkpoint_to_do < level) &&
      (checkpoint_in_progress < level))
  {
    /* no equal or stronger running or to run, we post request */
    /*
      We just don't broacast a cond, the checkpoint thread
      (see ma_least_recently_dirtied.c) will notice our request in max a few
      seconds.
    */
    next_asynchronous_checkpoint_to_do= level; /* post request */
  }

  /*
    If there was an error, only an error
    message to the error log will say it; normal, for a checkpoint triggered
    by a log write, we probably don't want the client's log write to throw an
    error, as the log write succeeded and a checkpoint failure is not
    critical: the failure in this case is more for the DBA to know than for
    the end user.
  */
}


/*
  If a 64-bit variable transitions from both halves being zero to both halves
  being non-zero, and never changes after that (like the transaction's
  first_undo_lsn), this function can be used to do a read of it (without
  mutex, without atomic load) which always produces a correct (though maybe
  slightly old) value (even on 32-bit CPUs).
  The prototype will change with Sanja's new LSN type.
*/
static inline ulonglong read_non_atomic(ulonglong volatile *x)
{
#if ( SIZEOF_CHARP >= 8 )
  /* 64-bit CPU (right?), 64-bit reads are atomic */
  return *x;
#else
  /*
    32-bit CPU, 64-bit reads may give a mixed of old half and new half (old
    low bits and new high bits, or the contrary).
    As the variable we read transitions from both halves being zero to both
    halves being non-zero, and never changes then, we can detect atomicity
    problems:
  */
  ulonglong y;
  for (;;) /* loop until no atomicity problems */
  {
    y= *x;
    if (likely(((0 == y) ||
                ((0 != (y >> 32)) && (0 != (y << 32)))))
      return y;
    /* Worth seeing it! */
    DBUG_PRINT("info",("atomicity problem"));
  }
#endif
}
