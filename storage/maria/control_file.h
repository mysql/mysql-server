/*
  WL#3234 Maria control file
  First version written by Guilhem Bichot on 2006-04-27.
  Does not compile yet.
*/

#ifndef _control_file_h
#define _control_file_h

/* indicate absence of the log file number */
#define CONTROL_FILE_IMPOSSIBLE_LOGNO 0xFFFFFFFF

/* Here is the interface of this module */

/*
  LSN of the last checkoint
  (if last_checkpoint_lsn_at_startup.file_no == CONTROL_FILE_IMPOSSIBLE_LOGNO
  then there was never a checkpoint)
*/
extern LSN last_checkpoint_lsn_at_startup;
/*
  Last log number at startup time (if last_logno_at_startup ==
  CONTROL_FILE_IMPOSSIBLE_LOGNO then there is no log file yet)
*/
extern uint32 last_logno_at_startup;

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
int control_file_write_and_force(LSN *checkpoint_lsn, uint32 log_no,
                                 uint objs_to_write);


/* Free resources taken by control file subsystem */
void control_file_end();

#endif
