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

void request_asynchronous_checkpoint(CHECKPOINT_LEVEL level);
my_bool execute_synchronous_checkpoint(CHECKPOINT_LEVEL level);
my_bool execute_asynchronous_checkpoint_if_any();
/* that's all that's needed in the interface */
