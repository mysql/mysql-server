/*
  WL#3071 Maria checkpoint
  First version written by Guilhem Bichot on 2006-04-27.
  Does not compile yet.
*/

/* Here is the implementation of this module */

#include "page_cache.h"
#include "least_recently_dirtied.h"
#include "transaction.h"
#include "share.h"
#include "log.h"

/*
  this transaction is used for any system work (purge, checkpoint writing
  etc), that is, background threads. It will not be declared/initialized here
  in the final version.
*/
st_transaction system_trans= {0 /* long trans id */, 0 /* short trans id */,0,...};

/*
  The maximum rec_lsn in the LRD when last checkpoint was run, serves for the
  MEDIUM checkpoint.
*/
LSN max_rec_lsn_at_last_checkpoint= 0;

/* Picks a checkpoint request and executes it */
my_bool checkpoint()
{
  CHECKPOINT_LEVEL level;
  DBUG_ENTER("checkpoint");

  level= checkpoint_running= checkpoint_request;
  unlock(log_mutex);

  DBUG_ASSERT(level != NONE);

  switch (level)
  {
  case FULL:
    /* flush all pages up to the current end of the LRD */
    flush_all_LRD_to_lsn(MAX_LSN); /* MAX_LSN==ULONGLONG_MAX */
    /* this will go full speed (normal scheduling, no sleep) */
    break;
  case MEDIUM:
    /*
      flush all pages which were already dirty at last checkpoint:
      ensures that recovery will never start from before the next-to-last
      checkpoint (two-checkpoint rule).
      It is max, not min as the WL says (TODO update WL).
    */
    flush_all_LRD_to_lsn(max_rec_lsn_at_last_checkpoint);
    /* this will go full speed (normal scheduling, no sleep) */
    break;
  }
  
  error= checkpoint_indirect();

  lock(log_mutex);
  /*
    this portion cannot be done as a hook in write_log_record() for the
    LOGREC_CHECKPOINT type because:
    - at that moment we still have not written to the control file so cannot
    mark the request as done; this could be solved by writing to the control
    file in the hook but that would be an I/O under the log's mutex, bad.
    - it would not be nice organisation of code (I tried it :).
  */
  mark_checkpoint_done(error);
  unlock(log_mutex);
  DBUG_RETURN(error);
}


my_bool checkpoint_indirect()
{
  DBUG_ENTER("checkpoint_indirect");

  int error= 0;
  /* checkpoint record data: */
  LSN checkpoint_start_lsn;
  LEX_STRING string1={0,0}, string2={0,0}, string3={0,0};
  LEX_STRING *string_array[4];
  char *ptr;
  LSN checkpoint_lsn;
  LSN candidate_max_rec_lsn_at_last_checkpoint= 0;
  list_element *el;   /* to scan lists */


  DBUG_ASSERT(sizeof(byte *) <= 8);
  DBUG_ASSERT(sizeof(LSN) <= 8);

  lock(log_mutex); /* will probably be in log_read_end_lsn() already */
  checkpoint_start_lsn= log_read_end_lsn();
  unlock(log_mutex);

  DBUG_PRINT("info",("checkpoint_start_lsn %lu", checkpoint_start_lsn));

  lock(global_LRD_mutex);
  string1.length= 8+8+(8+8)*LRD->count;
  if (NULL == (string1.str= my_malloc(string1.length)))
    goto err;
  ptr= string1.str;
  int8store(ptr, checkpoint_start_lsn);
  ptr+= 8;
  int8store(ptr, LRD->count);
  ptr+= 8;
  if (LRD->count)
  {
    candidate_max_rec_lsn_at_last_checkpoint= LRD->last->rec_lsn;
    for (el= LRD->first; el; el= el->next)
    {
      int8store(ptr, el->page_id);
      ptr+= 8;
      int8store(ptr, el->rec_lsn);
      ptr+= 8;
    }
  }
  unlock(global_LRD_mutex);

  /*
    If trx are in more than one list (e.g. three:
    running transactions, committed transactions, purge queue), we can either
    take mutexes of all three together or do crabbing.
    But if an element can move from list 1 to list 3 without passing through
    list 2, crabbing is dangerous.
    Hopefully it's ok to take 3 mutexes together...
    Otherwise I'll have to make sure I miss no important trx and I handle dups.
  */
  lock(global_transactions_list_mutex); /* or 3 mutexes if there are 3 */
  string2.length= 8+(8+8)*trx_list->count;
  if (NULL == (string2.str= my_malloc(string2.length)))
    goto err;
  ptr= string2.str;
  int8store(ptr, trx_list->count);
  ptr+= 8;
  for (el= trx_list->first; el; el= el->next)
  {
    /* possibly latch el.rwlock */
    *ptr= el->state;
    ptr++;
    int7store(ptr, el->long_trans_id);
    ptr+= 7;
    int2store(ptr, el->short_trans_id);
    ptr+= 2;
    int8store(ptr, el->undo_lsn);
    ptr+= 8;
    int8store(ptr, el->undo_purge_lsn);
    ptr+= 8;
    /*
      if no latch, use double variable of type ULONGLONG_CONSISTENT in
      st_transaction, or even no need if Intel >=486
    */
    int8store(ptr, el->first_purge_lsn);
    ptr+= 8;
    /* possibly unlatch el.rwlock */
  }
  unlock(global_transactions_list_mutex);

  lock(global_share_list_mutex);
  string3.length= 8+(8+8)*share_list->count;
  if (NULL == (string3.str= my_malloc(string3.length)))
    goto err;
  ptr= string3.str;
  /* possibly latch each MARIA_SHARE */
  make_copy_of_global_share_list_to_array;
  unlock(global_share_list_mutex);

  /* work on copy */
  int8store(ptr, elements_in_array);
  ptr+= 8;
  for (scan_array)
  {
    int8store(ptr, array[...].file_id);
    ptr+= 8;
    memcpy(ptr, array[...].file_name, ...);
    ptr+= ...;
    /*
      these two are long ops (involving disk I/O) that's why we copied the
      list:
    */
    flush_bitmap_pages(el);
    /*
      fsyncs the fd, that's the loooong operation (e.g. max 150 fsync per
      second, so if you have touched 1000 files it's 7 seconds).
    */
    force_file(el);
  }

  /* now write the record */
  string_array[0]= string1;
  string_array[1]= string2;
  string_array[2]= string3;
  string_array[3]= NULL;

  checkpoint_lsn= log_write_record(LOGREC_CHECKPOINT,
                                   &system_trans, string_array);

  if (0 == checkpoint_lsn) /* maybe 0 is impossible LSN to indicate error ? */
    goto err;

  if (0 != control_file_write_and_force(checkpoint_lsn, NULL))
    goto err;

  maximum_rec_lsn_last_checkpoint= candidate_max_rec_lsn_at_last_checkpoint;

  DBUG_RETURN(0);

err:

  print_error_to_error_log(the_error_message);
  my_free(buffer1.str, MYF(MY_ALLOW_ZERO_PTR));
  my_free(buffer2.str, MYF(MY_ALLOW_ZERO_PTR));
  my_free(buffer3.str, MYF(MY_ALLOW_ZERO_PTR));

  DBUG_RETURN(1);
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
    request_checkpoint(INDIRECT, 0 /*wait_for_completion*/);
  }
  ...;
  unlock(log_mutex);
  ...;
}

/*
  Call this when you want to request a checkpoint.
  In real life it will be called by log_write_record() and by client thread
  which explicitely wants to do checkpoint (ALTER ENGINE CHECKPOINT
  checkpoint_level).
*/
int request_checkpoint(CHECKPOINT_LEVEL level, my_bool wait_for_completion)
{
  int error= 0;
  /*
    If caller wants to wait for completion we'll have to release the log mutex
    to wait on condition, if caller had log mutex he may not be happy that we
    release it, so we check that caller didn't have log mutex.
  */
  if (wait_for_completion)
  {
    lock(log_mutex);
  }
  else
    safemutex_assert_owner(log_mutex);

  DBUG_ASSERT(checkpoint_request >= checkpoint_running);
  DBUG_ASSERT(level > NONE);
  if (checkpoint_request < level)
  {
    /* no equal or stronger running or to run, we post request */
    /*
      note that thousands of requests for checkpoints are going to come all
      at the same time (when the log bound is passed), so it may not be a good
      idea for each of them to broadcast a cond. We just don't broacast a
      cond, the checkpoint thread will wake up in max one second.
    */
    checkpoint_request= level; /* post request */
  }
   
  if (wait_for_completion)
  {
    uint checkpoints_done_copy= checkpoints_done;
    uint checkpoint_errors_copy= checkpoint_errors;
    /*
      note that the "==done" works when the uint counter wraps too, so counter
      can even be smaller than uint if we wanted (however it should be big
      enough so that max_the_int_type checkpoints cannot happen between two
      wakeups of our thread below). uint sounds fine.
      Wait for our checkpoint to be done:
    */

    if (checkpoint_running != NONE) /* not ours, let it pass */
    {
      while (1)
      {
        if (checkpoints_done != checkpoints_done_copy)
        {
          if (checkpoints_done == (checkpoints_done_copy+1))
          {
            /* not our checkpoint, forget about it */
            checkpoints_done_copy= checkpoints_done;
          }
          break; /* maybe even ours has been done at this stage! */
        }
        cond_wait(checkpoint_done_cond, log_mutex);
      }
    }

    /* now we come to waiting for our checkpoint */
    while (1)
    {
      if (checkpoints_done != checkpoints_done_copy)
      {
        /* our checkpoint has been done */
        break;
      }
      if (checkpoint_errors != checkpoint_errors_copy)
      {
        /*
          the one which was running a few milliseconds ago (if there was one),
          and/or ours, had an error, just assume it was ours. So there
          is a possibility that we return error though we succeeded, in which
          case user will have to retry; but two simultanate checkpoints have
          high changes to fail together (as the error probably comes from
          malloc or disk write problem), so chance of false alarm is low.
          Reporting the error only to the one which caused the error would
          require having a (not fixed size) list of all requests, not worth it.
        */
        error= 1;
        break;
      }
      cond_wait(checkpoint_done_cond, log_mutex);
    }
    unlock(log_mutex);
  } /* ... if (wait_for_completion) */

  /*
    If wait_for_completion was false, and there was an error, only an error
    message to the error log will say it; normal, for a checkpoint triggered
    by a log write, we probably don't want the client's log write to throw an
    error, as the log write succeeded and a checkpoint failure is not
    critical: the failure in this case is more for the DBA to know than for
    the end user.
  */
  return error;
}

void mark_checkpoint_done(int error)
{
  safemutex_assert_owner(log_mutex);
  if (error)
    checkpoint_errors++;
  /* a checkpoint is said done even if it had an error */
  checkpoints_done++;
  if (checkpoint_request == checkpoint_running)
  {
    /*
      No new request has been posted, so we satisfied all requests, forget
      about them.
    */
    checkpoint_request= NONE;
  }
  checkpoint_running= NONE;
  written_since_last_checkpoint= 0;
  broadcast(checkpoint_done_cond);
}

/*
  Alternative (not to be done, too disturbing):
  do the autocheckpoint in the thread which passed the bound first (and do the
  checkpoint in the client thread which requested it).
  It will give a delay to that client thread which passed the bound (time to
  fsync() for example 1000 files is 16 s on my laptop). Here is code for
  explicit and implicit checkpoints, where client thread does the job:
*/
#if 0
{
  lock(log_mutex); /* explicit takes it here, implicit already has it */
  while (checkpoint_running != NONE)
  {
    if (checkpoint_running >= my_level) /* always true for auto checkpoints */
      goto end; /* we skip checkpoint */
    /* a less strong is running, I'll go next */
    wait_on_checkpoint_done_cond();
  }
  checkpoint_running= my_level;
  checkpoint(my_level); // can gather checkpoint_start_lsn before unlock
  lock(log_mutex);
  checkpoint_running= NONE;
  written_since_last_checkpoint= 0;
end:
  unlock(log_mutex);
}
#endif
