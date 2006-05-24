/*
  WL#3071 Maria checkpoint
  First version written by Guilhem Bichot on 2006-04-27.
  Does not compile yet.
*/

/* This is the interface of this module. */

typedef enum enum_checkpoint_level {
  NONE=-1,
  INDIRECT, /* just write dirty_pages, transactions table and sync files */
  MEDIUM, /* also flush all dirty pages which were already dirty at prev checkpoint*/
  FULL /* also flush all dirty pages */
} CHECKPOINT_LEVEL;

/*
  Call this when you want to request a checkpoint.
  In real life it will be called by log_write_record() and by client thread
  which explicitely wants to do checkpoint (ALTER ENGINE CHECKPOINT
  checkpoint_level).
*/
int request_checkpoint(CHECKPOINT_LEVEL level, my_bool wait_for_completion);
/* that's all that's needed in the interface */
