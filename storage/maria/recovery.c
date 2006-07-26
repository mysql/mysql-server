/*
  WL#3072 Maria recovery
  First version written by Guilhem Bichot on 2006-04-27.
  Does not compile yet.
*/

/* Here is the implementation of this module */

#include "page_cache.h"
#include "least_recently_dirtied.h"
#include "transaction.h"
#include "share.h"
#include "log.h"

typedef struct st_record_type_properties {
 /* used for debug error messages or "maria_read_log" command-line tool: */
  char *name,
  my_bool record_ends_group;
  /* a function to execute when we see the record during the REDO phase */
  int (*record_execute_in_redo_phase)(RECORD *); /* param will be record header instead later */
  /* a function to execute when we see the record during the UNDO phase */
  int (*record_execute_in_undo_phase)(RECORD *); /* param will be record header instead later */
} RECORD_TYPE_PROPERTIES;

int no_op(RECORD *) {return 0};

RECORD_TYPE_PROPERTIES all_record_type_properties[]=
{
  /* listed here in the order of the "log records type" enumeration */
  {"REDO_INSERT_HEAD", FALSE, redo_insert_head_execute_in_redo_phase, no_op},
  ...,
  {"UNDO_INSERT"     , TRUE , undo_insert_execute_in_redo_phase, undo_insert_execute_in_undo_phase},
  {"COMMIT",         , TRUE , commit_execute_in_redo_phase, no_op},
  ...
};

int redo_insert_head_execute_in_redo_phase(RECORD *record)
{
  /* write the data to the proper page */
}

int undo_insert_execute_in_redo_phase(RECORD *record)
{
  trans_table[short_trans_id].undo_lsn= record.lsn;
  /* don't restore the old version of the row */
}

int undo_insert_execute_in_undo_phase(RECORD *record)
{
  /* restore the old version of the row */
  trans_table[short_trans_id].undo_lsn= record.prev_undo_lsn;
}

int commit_execute_in_redo_phase(RECORD *record)
{
  trans_table[short_trans_id].state= COMMITTED;
  /*
    and that's all: the delete/update handler should not be woken up! as there
    may be REDO for purge further in the log.
  */
}

#define record_ends_group(R)                                            \
  all_record_type_properties[(R)->type].record_ends_group)

#define execute_log_record_in_redo_phase(R)                                           \
  all_record_type_properties[(R).type].record_execute_in_redo_phase(R)


int recovery()
{
  control_file_create_or_open();
  /*
    init log handler: tell it that we are going to do large reads of the
    log, sequential and backward. Log handler could decide to alloc a big
    read-only IO_CACHE for this, or use its usual page cache.
  */

  /* read checkpoint log record from log handler */
  RECORD *checkpoint_record= log_read_record(last_checkpoint_lsn_at_start);

  /* parse this record, build structs (dirty_pages, transactions table, file_map) */
  /*
    read log records (note: sometimes only the header is needed, for ex during
    REDO phase only the header of UNDO is needed, not the 4G blob in the
    variable-length part, so I could use that; however for PREPARE (which is a
    variable-length record) I'll need to read the full record in the REDO
    phase):
  */

  /**** REDO PHASE *****/

  record= log_read_record(min(rec_lsn, ...)); /* later, read only header */

  /*
    if log handler knows the end LSN of the log, we could print here how many
    MB of log we have to read (to give an idea of the time), and print
    progress notes.
  */

  while (record != NULL)
  {
    /*
      A complete group is a set of log records with an "end mark" record
      (e.g. a set of REDOs for an operation, terminated by an UNDO for this
      operation); if there is no "end mark" record the group is incomplete
      and won't be executed.
    */
    if (record_ends_group(record)
    {
      if (trans_table[record.short_trans_id].group_start_lsn != 0)
      {
        /*
          There is a complete group for this transaction, containing more than
          this event.
          We're going to read recently read log records:
          for this log_read_record() to be efficient (not touch the disk),
          log handler could cache recently read pages
          (can just use an IO_CACHE of 10 MB to read the log, or the normal
          log handler page cache).
          Without it only OS file cache will help.
        */
        record2=
          log_read_record(trans_table[record.short_trans_id].group_start_lsn);

        do
        {
          if (record2.short_trans_id == record.short_trans_id)
            execute_log_record_in_redo_phase(record2); /* it's in our group */
          record2= log_read_next_record();
        }
        while (record2.lsn < record.lsn);
        trans_table[record.short_trans_id].group_start_lsn= 0; /* group finished */
      }
      execute_log_record_in_redo_phase(record);
    }
    else /* record does not end group */
    {
      /* just record the fact, can't know if can execute yet */
      if (trans_table[short_trans_id].group_start_lsn == 0) /* group not yet started */
        trans_table[short_trans_id].group_start_lsn= record.lsn;
    }

    /*
      Later we can optimize: instead of "execute_log_record(record2)", do
      copy_record_into_exec_buffer(record2):
      this will just copy record into a multi-record (10 MB?) memory buffer,
      and when buffer is full, will do sorting of REDOs per 
      page id and execute them.
      This sorting will enable us to do more sequential reads of the
      data/index pages.
      Note that updating bitmap pages (when we have executed a REDO for a page
      we update its bitmap page) may break the sequential read of pages,
      so maybe we should read and cache bitmap pages in the beginning.
      Or ok the sequence will be broken, but quickly all bitmap pages will be
      in memory and so the sequence will not be broken anymore.
      Sorting could even determine, based on physical device of files
      ("st_dev" in stat()), that some files should be should be taken by
      different threads, if we want to do parallism.
    */
    /*
      Here's how to read a complete variable-length record if needed:
      <sanja> read the header, allocate buffer of record length, read whole
      record.
    */
    record= log_read_next_record();
  }

  /*
    Earlier or here, create true transactions in TM.
    If done earlier, note that TM should not wake up the delete/update handler
    when it receives a commit info, as existing REDO for purge may exist in
    the log, and so the delete/update handler may do changes which conflict
    with these REDOs.
    Even if done here, better to not wake it up now as we're going to free the
    page cache.

    MikaelR suggests: support checkpoints during REDO phase too: do checkpoint
    after a certain amount of log records have been executed. This helps
    against repeated crashes. Those checkpoints could not be user-requested
    (as engine is not communicating during the REDO phase), so they would be
    automatic: this changes the original assumption that we don't write to the
    log while in the REDO phase, but why not. How often should we checkpoint?
  */

  /*
    We want to have two steps:
    engine->recover_with_max_memory();
    next_engine->recover_with_max_memory();
    engine->init_with_normal_memory();
    next_engine->init_with_normal_memory();
    So: in recover_with_max_memory() allocate a giant page cache, do REDO
    phase, then all page cache is flushed and emptied and freed (only retain
    small structures like TM): take full checkpoint, which is useful if
    next engine crashes in its recovery the next second.
    Destroy all shares (maria_close()), then at init_with_normal_memory() we
    do this:
  */

  /**** UNDO PHASE *****/

  print_information_to_error_log(nb of trans to roll back, nb of prepared trans);

  /*
    Launch one or more threads to do the background rollback. Don't wait for
    them to complete their rollback (background rollback; for debugging, we
    can have an option which waits). Set a counter (total_of_rollback_threads)
    to the number of threads to lauch.

    Note that InnoDB's rollback-in-background works as long as InnoDB is the
    last engine to recover, otherwise MySQL will refuse new connections until
    the last engine has recovered so it's not "background" from the user's
    point of view. InnoDB is near top of sys_table_types so all others
    (e.g. BDB) recover after it... So it's really "online rollback" only if
    InnoDB is the only engine.
  */

  /* wake up delete/update handler */
  /* tell the TM that it can now accept new transactions */

  /*
    mark that checkpoint requests are now allowed.
  */
  /*
    when all rollback threads have terminated, somebody should print "rollback
    finished" to the error log.
  */
}

pthread_handler_decl rollback_background_thread()
{
  /*
    execute the normal runtime-rollback code for a bunch of transactions.
  */
  while (trans in list_of_trans_to_rollback_by_this_thread)
  {
    while (trans->undo_lsn != 0)
    {
      /* this is the normal runtime-rollback code: */
      record= log_read_record(trans->undo_lsn);
      execute_log_record_in_undo_phase(record);
      trans->undo_lsn= record.prev_undo_lsn;
    }
    /* remove trans from list */
  }
  lock_mutex(rollback_threads); /* or atomic counter */
  if (--total_of_rollback_threads == 0)
  {
    /*
      All rollback threads are done.  Print "rollback finished" to the error
      log.  The UNDO phase has the reputation of being a slow operation
      (slower than the REDO phase), so taking a checkpoint at the end of it is
      intelligent, but as this UNDO phase generates REDOs and CLR_ENDs, if it
      did a lot of work then the "automatic checkpoint when much has been
      written to the log" will do it; and if the UNDO phase didn't do a lot of
      work, no need for a checkpoint. If we change our mind and want to force
      a checkpoint at the end of the UNDO phase, simply call it here.
    */
  }
  unlock_mutex(rollback_threads);
  pthread_exit();
}
