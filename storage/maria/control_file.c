/*
  WL#3234 Maria control file
  First version written by Guilhem Bichot on 2006-04-27.
  Does not compile yet.
*/

#include "maria_def.h"


/* Here is the implementation of this module */

/* should be sector size for atomic write operation */
#define STAT_FILE_FILENO_SIZE 4
#define STAT_FILE_FILEOFFSET_SIZE 4
#define STAT_FILE_LSN_SIZE (STAT_FILE_FILENO_SIZE + STAT_FILE_FILEOFFSET_SIZE)
#define STAT_FILE_MAX_SIZE (STAT_FILE_LSN_SIZE + STAT_FILE_FILENO_SIZE)


LSN last_checkpoint_lsn_at_startup;
uint32 last_logno_at_startup;


/*
  Control file is less then  512 bytes (a disk sector),
  to be as atomic as possible
*/
static int control_file_fd;


/*
  Initialize control file subsystem

  SYNOPSIS
    control_file_create_or_open()

  Looks for the control file. If absent, it's a fresh start, create file.
  If present, read it to find out last checkpoint's LSN and last log.
  Called at engine's start.

  RETURN
    0 - OK
    1 - Error
*/
int control_file_create_or_open()
{
  char buffer[STAT_FILE_MAX_SIZE];
  char name[FN_REFLEN];
  MY_STAT stat_buff;

  /* name is concatenation of Maria's home dir and "control" */
  if (fn_format(name, "control", maria_data_root, "", MYF(MY_WME)) == NullS)
    return 1;

  if ((control_file_fd= my_open(name,
                                O_CREAT | O_BINARY | /*O_DIRECT |*/ O_RDWR,
                                MYF(MY_WME))) < 0)
    return 1;

  /*
    TODO: from "man fsync" on Linux:
    "fsync does not necessarily ensure that the entry in  the  direc- tory
    containing  the file has also reached disk.  For that an explicit
    fsync on the file descriptor of the directory is also needed."
    So if we just created the file we should sync the directory.
    Maybe there should be a flag of my_create() to do this.
  */

  if (my_stat(name, &stat_buff, MYF(MY_WME)) == NULL)
    return 1;

  if (stat_buff.st_size < STAT_FILE_MAX_SIZE)
  {
    /*
      File shorter than expected (either we just created it, or a previous run
      crashed between creation and first write); do first write.
    */
    char buffer[STAT_FILE_MAX_SIZE];
    /*
      To be safer we should make sure that there are no logs or data/index
      files around (indeed it could be that the control file alone was deleted
      or not restored, and we should not go on with life at this point).

      TODO: For now we trust (this is alpha version), but for beta if would
      be great to verify.

      We could have a tool which can rebuild the control file, by reading the
      directory of logs, finding the newest log, reading it to find last
      checkpoint... Slow but can save your db.
    */
    last_checkpoint_lsn_at_startup.file_no= CONTROL_FILE_IMPOSSIBLE_LOGNO;
    last_checkpoint_lsn_at_startup.rec_offset= 0;
    last_logno_at_startup= CONTROL_FILE_IMPOSSIBLE_LOGNO;

    /* init the file with these "undefined" values */
    return control_file_write_and_force(last_checkpoint_lsn_at_startup,
                                        last_logno_at_startup);
  }
  /* Already existing file, read it */
  if (my_read(control_file_fd, buffer, STAT_FILE_MAX_SIZE,
              MYF(MY_FNABP | MY_WME)))
    return 1;
  last_checkpoint_lsn_at_startup.file_no= uint4korr(buffer);
  last_checkpoint_lsn_at_startup.rec_offset= uint4korr(buffer +
                                                       STAT_FILE_FILENO_SIZE);
  last_logno_at_startup= uint4korr(buffer + STAT_FILE_LSN_SIZE);
  return 0;
}


/*
  Write information durably to the control file.

  SYNOPSIS
    control_file_write_and_force()
    checkpoint_lsn       LSN of checkpoint
    log_no               last log file number
    args_to_write        bitmap of 1 (write the LSN) and 2 (write the LOGNO)

  Called when we have created a new log (after syncing this log's creation)
  and when we have written a checkpoint (after syncing this log record).

  RETURN
    0 - OK
    1 - Error
*/

int control_file_write_and_force(LSN *checkpoint_lsn, uint32 log_no,
                                 uint args_to_write)
{
  char buffer[STAT_FILE_MAX_SIZE];
  uint start= STAT_FILE_LSN_SIZE, end= STAT_FILE_LSN_SIZE;
  /*
    If LSN was specified...

    rec_offset can't be 0 in real LSN, because all files have header page
  */
  if ((args_to_write & 1) && checkpoint_lsn) /* write checkpoint LSN */
  {
    start= 0;
    int4store(buffer, checkpoint_lsn->file_no);
    int4store(buffer + STAT_FILE_FILENO_SIZE, checkpoint_lsn->rec_offset);
  }
  if (args_to_write & 2) /* write logno */
  {
    end= STAT_FILE_MAX_SIZE;
    int4store(buffer + STAT_FILE_LSN_SIZE, log_no);
  }
  DBUG_ASSERT(start != end);
  return (my_pwrite(control_file_fd, buffer + start, end - start, start,
                    MYF(MY_FNABP |  MY_WME)) ||
          my_sync(control_file_fd, MYF(MY_WME)));
}


/*
  Free resources taken by control file subsystem

  SYNOPSIS
    control_file_end()
*/

void control_file_end()
{
  my_close(control_file_fd, MYF(MY_WME));
}
