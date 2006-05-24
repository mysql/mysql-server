/*
  WL#3234 Maria control file
  First version written by Guilhem Bichot on 2006-04-27.
  Does not compile yet.
*/

/* Here is the interface of this module */

extern LSN last_checkpoint_lsn_at_startup;
extern char *last_log_name_at_startup;

/*
  Looks for the control file. If absent, it's a fresh start, create file.
  If present, read it to find out last checkpoint's LSN and last log.
  Called at engine's start.
*/
int control_file_create_or_open();

/*
  Write information durably to the control file.
  Called when we have created a new log (after syncing this log's creation)
  and when we have written a checkpoint (after syncing this log record).
*/
int control_file_write_and_force(LSN lsn, char *log_name);
