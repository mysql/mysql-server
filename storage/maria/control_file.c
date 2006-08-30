/*
  WL#3234 Maria control file
  First version written by Guilhem Bichot on 2006-04-27.
  Does not compile yet.
*/

#include "maria_def.h"


/* Here is the implementation of this module */

/*
  a control file contains 3 objects: magic string, LSN of last checkpoint,
  number of last log.
*/

/* total size should be < sector size for atomic write operation */
#define CONTROL_FILE_MAGIC_STRING "MACF"
#define CONTROL_FILE_MAGIC_STRING_OFFSET 0
#define CONTROL_FILE_MAGIC_STRING_SIZE sizeof(CONTROL_FILE_MAGIC_STRING)
#define CONTROL_FILE_LSN_OFFSET (CONTROL_FILE_MAGIC_STRING_OFFSET + CONTROL_FILE_MAGIC_STRING_SIZE)
#define CONTROL_FILE_LSN_SIZE (4+4)
#define CONTROL_FILE_FILENO_OFFSET (CONTROL_FILE_LSN_OFFSET + CONTROL_FILE_LSN_SIZE)
#define CONTROL_FILE_FILENO_SIZE 4
#define CONTROL_FILE_MAX_SIZE (CONTROL_FILE_FILENO_OFFSET + CONTROL_FILE_FILENO_SIZE)


LSN last_checkpoint_lsn_at_startup;
uint32 last_logno_at_startup;


/*
  Control file is less then  512 bytes (a disk sector),
  to be as atomic as possible
*/
static int control_file_fd;

static void lsn8store(char *buffer, LSN *lsn)
{
  int4store(buffer, lsn->file_no);
  int4store(buffer + CONTROL_FILE_FILENO_SIZE, lsn->rec_offset);
}

static LSN lsn8korr(char *buffer)
{
  LSN tmp;
  tmp.file_no= uint4korr(buffer);
  tmp.rec_offset= uint4korr(buffer + CONTROL_FILE_FILENO_SIZE);
  return tmp;
}

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
  char buffer[CONTROL_FILE_MAX_SIZE];
  char name[FN_REFLEN];
  MY_STAT stat_buff;

  /*
    If you change sizes in the #defines, you at least have to change the
    "*store" and "*korr" calls in this file, and can even create backward
    compatibility problems. Beware!
  */
  DBUG_ASSERT(CONTROL_FILE_LSN_SIZE == (4+4));
  DBUG_ASSERT(CONTROL_FILE_FILENO_SIZE == 4);

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

  if (stat_buff.st_size < CONTROL_FILE_MAX_SIZE)
  {
    /*
      File shorter than expected (either we just created it, or a previous run
      crashed between creation and first write); do first write.
    */
    char buffer[CONTROL_FILE_MAX_SIZE];
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
    last_checkpoint_lsn_at_startup.file_no= CONTROL_FILE_IMPOSSIBLE_FILENO;
    last_checkpoint_lsn_at_startup.rec_offset= 0;
    last_logno_at_startup= CONTROL_FILE_IMPOSSIBLE_FILENO;

    /* init the file with these "undefined" values */
    return control_file_write_and_force(last_checkpoint_lsn_at_startup,
                                        last_logno_at_startup,
                                        CONTROL_FILE_WRITE_ALL);
  }
  /* Already existing file, read it */
  if (my_read(control_file_fd, buffer, CONTROL_FILE_MAX_SIZE,
              MYF(MY_FNABP | MY_WME)))
    return 1;
  if (memcmp(buffer + CONTROL_FILE_MAGIC_STRING_OFFSET,
             CONTROL_FILE_MAGIC_STRING, CONTROL_FILE_MAGIC_STRING_SIZE))
    return 1;
  last_checkpoint_lsn_at_startup= lsn8korr(buffer + CONTROL_FILE_LSN_OFFSET);
  last_logno_at_startup= uint4korr(buffer + CONTROL_FILE_FILENO_OFFSET);
  return 0;
}


#define CONTROL_FILE_WRITE_ALL 0 /* write all 3 objects */
#define CONTROL_FILE_WRITE_ONLY_LSN 1
#define CONTROL_FILE_WRITE_ONLY_LOGNO 2
/*
  Write information durably to the control file.

  SYNOPSIS
    control_file_write_and_force()
    checkpoint_lsn       LSN of last checkpoint
    log_no               last log file number
    objs_to_write         what we should write

  Called when we have created a new log (after syncing this log's creation)
  and when we have written a checkpoint (after syncing this log record).

  NOTE
    We always want to do one single my_pwrite() here to be as atomic as
    possible.

  RETURN
    0 - OK
    1 - Error
*/

int control_file_write_and_force(LSN *checkpoint_lsn, uint32 log_no,
                                 uint objs_to_write)
{
  char buffer[CONTROL_FILE_MAX_SIZE];
  uint start, size;
  memcpy(buffer + CONTROL_FILE_MAGIC_STRING_OFFSET,
         CONTROL_FILE_MAGIC_STRING, CONTROL_FILE_MAGIC_STRING_SIZE);
  /* write checkpoint LSN */
  if (checkpoint_lsn)
    lsn8store(buffer + CONTROL_FILE_LSN_OFFSET, checkpoint_lsn);
  /* write logno */
  int4store(buffer + CONTROL_FILE_FILENO_OFFSET, log_no);
  if (objs_to_write == CONTROL_FILE_WRITE_ALL)
  {
    start= CONTROL_FILE_MAGIC_STRING_OFFSET;
    size= CONTROL_FILE_MAX_SIZE;
  }
  else if (objs_to_write == CONTROL_FILE_WRITE_ONLY_LSN)
  {
    start= CONTROL_FILE_LSN_OFFSET;
    size= CONTROL_FILE_LSN_SIZE;
  }
  else if (objs_to_write == CONTROL_FILE_WRITE_ONLY_LOGNO)
  {
    start= CONTROL_FILE_FILENO_OFFSET;
    size= CONTROL_FILE_FILENO_SIZE;
  }
  else /* incorrect value of objs_to_write */
    DBUG_ASSERT(0);
  return (my_pwrite(control_file_fd, buffer + start, size,
                    start, MYF(MY_FNABP |  MY_WME)) ||
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
