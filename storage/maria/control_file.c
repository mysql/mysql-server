/*
  WL#3234 Maria control file
  First version written by Guilhem Bichot on 2006-04-27.
  Does not compile yet.
*/

/* Here is the implementation of this module */

/* Control file is 512 bytes (a disk sector), to be as atomic as possible */

int control_file_fd;

/*
  Looks for the control file. If absent, it's a fresh start, create file.
  If present, read it to find out last checkpoint's LSN and last log.
  Called at engine's start.
*/
int control_file_create_or_open()
{
  char buffer[4];
  /* name is concatenation of Maria's home dir and "control" */
  if ((control_file_fd= my_open(name, O_RDWR)) < 0)
  {
    /* failure, try to create it */
    if ((control_file_fd= my_create(name, O_RDWR)) < 0)
      return 1;
    /*
      So this is a start from scratch, to be safer we should make sure that
      there are no logs or data/index files around (indeed it could be that
      the control file alone was deleted or not restored, and we should not
      go on with life at this point.
      For now we trust (this is alpha version), but for beta if would be great
      to verify.

      We could have a tool which can rebuild the control file, by reading the
      directory of logs, finding the newest log, reading it to find last
      checkpoint... Slow but can save your db.
    */
    last_checkpoint_lsn_at_startup= 0;
    last_log_name_at_startup= NULL;
    return 0;
  }
  /* Already existing file, read it */
  if (my_read(control_file_fd, buffer, 8, MYF(MY_FNABP)))
    return 1;
  last_checkpoint_lsn_at_startup= uint8korr(buffer);
  if (last_log_name_at_startup= my_malloc(512-8+1))
    return 1;
  if (my_read(control_file_fd, last_log_name_at_startup, 512-8), MYF(MY_FNABP))
    return 1;
  last_log_name[512-8]= 0; /* end zero to be nice */
  return 0;
}

/*
  Write information durably to the control file.
  Called when we have created a new log (after syncing this log's creation)
  and when we have written a checkpoint (after syncing this log record).
*/
int control_file_write_and_force(LSN lsn, char *log_name)
{
  char buffer[512];
  uint start=8,end=8;
  if (lsn != 0) /* LSN was specified */
  {
    start= 0;
    int8store(buffer, lsn);
  }
  if (log_name != NULL) /* log name was specified */
  {
    end= 512;
    memcpy(buffer+8, log_name, 512-8);
  }
  DBUG_ASSERT(start != end);
  return (my_pwrite(control_file_fd, buffer, end-start, start, MYF(MY_FNABP)) ||
          my_sync(control_file_fd))
}
