/*
  WL#3234 Maria control file
  First version written by Guilhem Bichot on 2006-04-27.
  Does not compile yet.
*/

#include "maria_def.h"
#include "ma_control_file.h"

/* Here is the implementation of this module */

/*
  a control file contains 3 objects: magic string, LSN of last checkpoint,
  number of last log.
*/

/* total size should be < sector size for atomic write operation */
#define CONTROL_FILE_MAGIC_STRING "MACF"
#define CONTROL_FILE_MAGIC_STRING_OFFSET 0
#define CONTROL_FILE_MAGIC_STRING_SIZE 4
#define CONTROL_FILE_LSN_OFFSET (CONTROL_FILE_MAGIC_STRING_OFFSET + CONTROL_FILE_MAGIC_STRING_SIZE)
#define CONTROL_FILE_LSN_SIZE (4+4)
#define CONTROL_FILE_FILENO_OFFSET (CONTROL_FILE_LSN_OFFSET + CONTROL_FILE_LSN_SIZE)
#define CONTROL_FILE_FILENO_SIZE 4
#define CONTROL_FILE_MAX_SIZE (CONTROL_FILE_FILENO_OFFSET + CONTROL_FILE_FILENO_SIZE)

/*
  This module owns these two vars.
  uint32 is always atomically updated, but LSN is 8 bytes, we will need
  provisions to ensure that it's updated atomically in
  ma_control_file_write_and_force(). Probably the log mutex could be
  used. TODO.
*/
LSN last_checkpoint_lsn;
uint32 last_logno;


/*
  Control file is less then  512 bytes (a disk sector),
  to be as atomic as possible
*/
static int control_file_fd;

static void lsn8store(char *buffer, const LSN *lsn)
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
    ma_control_file_create_or_open()

  Looks for the control file. If absent, it's a fresh start, creates file.
  If present, reads it to find out last checkpoint's LSN and last log, updates
  the last_checkpoint_lsn and last_logno global variables.
  Called at engine's start.

  RETURN
    0 - OK
    1 - Error
*/
int ma_control_file_create_or_open()
{
  char buffer[CONTROL_FILE_MAX_SIZE];
  char name[FN_REFLEN];
  MY_STAT stat_buff;
  DBUG_ENTER("ma_control_file_create_or_open");

  /*
    If you change sizes in the #defines, you at least have to change the
    "*store" and "*korr" calls in this file, and can even create backward
    compatibility problems. Beware!
  */
  DBUG_ASSERT(CONTROL_FILE_LSN_SIZE == (4+4));
  DBUG_ASSERT(CONTROL_FILE_FILENO_SIZE == 4);

  /* name is concatenation of Maria's home dir and "control" */
  if (fn_format(name, "control", maria_data_root, "", MYF(MY_WME)) == NullS)
    DBUG_RETURN(1);

  if ((control_file_fd= my_open(name,
                                O_CREAT | O_BINARY | /*O_DIRECT |*/ O_RDWR,
                                MYF(MY_WME))) < 0)
    DBUG_RETURN(1);

  /*
    TODO: from "man fsync" on Linux:
    "fsync does not necessarily ensure that the entry in  the  direc- tory
    containing  the file has also reached disk.  For that an explicit
    fsync on the file descriptor of the directory is also needed."
    So if we just created the file we should sync the directory.
    Maybe there should be a flag of my_create() to do this.
  */

  if (my_stat(name, &stat_buff, MYF(MY_WME)) == NULL)
    DBUG_RETURN(1);

  if ((uint)stat_buff.st_size < CONTROL_FILE_MAX_SIZE)
  {
    /*
      File shorter than expected (either we just created it, or a previous run
      crashed between creation and first write); do first write.

      To be safer we should make sure that there are no logs or data/index
      files around (indeed it could be that the control file alone was deleted
      or not restored, and we should not go on with life at this point).

      TODO: For now we trust (this is alpha version), but for beta if would
      be great to verify.

      We could have a tool which can rebuild the control file, by reading the
      directory of logs, finding the newest log, reading it to find last
      checkpoint... Slow but can save your db.
    */
    LSN imposs_lsn= CONTROL_FILE_IMPOSSIBLE_LSN;
    uint32 imposs_logno= CONTROL_FILE_IMPOSSIBLE_FILENO;

    /* init the file with these "undefined" values */
    DBUG_RETURN(ma_control_file_write_and_force(&imposs_lsn, imposs_logno,
                                             CONTROL_FILE_WRITE_ALL));
  }
  /* Already existing file, read it */
  if (my_read(control_file_fd, buffer, CONTROL_FILE_MAX_SIZE,
              MYF(MY_FNABP | MY_WME)))
    DBUG_RETURN(1);
  if (memcmp(buffer + CONTROL_FILE_MAGIC_STRING_OFFSET,
             CONTROL_FILE_MAGIC_STRING, CONTROL_FILE_MAGIC_STRING_SIZE))
  {
    /*
      TODO: what is the good way to report the error? Knowing that this
      happens at startup, probably stderr.
    */
    DBUG_PRINT("error", ("bad magic string"));
    DBUG_RETURN(1);
  }
  last_checkpoint_lsn= lsn8korr(buffer + CONTROL_FILE_LSN_OFFSET);
  last_logno= uint4korr(buffer + CONTROL_FILE_FILENO_OFFSET);
  DBUG_RETURN(0);
}


/*
  Write information durably to the control file; stores this information into
  the last_checkpoint_lsn and last_logno global variables.

  SYNOPSIS
    ma_control_file_write_and_force()
    checkpoint_lsn       LSN of last checkpoint
    logno                last log file number
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

int ma_control_file_write_and_force(const LSN *checkpoint_lsn, uint32 logno,
                                 uint objs_to_write)
{
  char buffer[CONTROL_FILE_MAX_SIZE];
  uint start, size;
  DBUG_ENTER("ma_control_file_write_and_force");

  memcpy(buffer + CONTROL_FILE_MAGIC_STRING_OFFSET,
         CONTROL_FILE_MAGIC_STRING, CONTROL_FILE_MAGIC_STRING_SIZE);
  /* write checkpoint LSN */
  if (checkpoint_lsn)
    lsn8store(buffer + CONTROL_FILE_LSN_OFFSET, checkpoint_lsn);
  /* write logno */
  int4store(buffer + CONTROL_FILE_FILENO_OFFSET, logno);
  if (objs_to_write == CONTROL_FILE_WRITE_ALL)
  {
    start= CONTROL_FILE_MAGIC_STRING_OFFSET;
    size= CONTROL_FILE_MAX_SIZE;
    last_checkpoint_lsn= *checkpoint_lsn;
    last_logno= logno;
  }
  else if (objs_to_write == CONTROL_FILE_WRITE_ONLY_LSN)
  {
    start= CONTROL_FILE_LSN_OFFSET;
    size= CONTROL_FILE_LSN_SIZE;
    last_checkpoint_lsn= *checkpoint_lsn;
  }
  else if (objs_to_write == CONTROL_FILE_WRITE_ONLY_LOGNO)
  {
    start= CONTROL_FILE_FILENO_OFFSET;
    size= CONTROL_FILE_FILENO_SIZE;
    last_logno= logno;
  }
  else /* incorrect value of objs_to_write */
    DBUG_ASSERT(0);
  DBUG_RETURN(my_pwrite(control_file_fd, buffer + start, size,
                        start, MYF(MY_FNABP |  MY_WME)) ||
              my_sync(control_file_fd, MYF(MY_WME)));
}


/*
  Free resources taken by control file subsystem

  SYNOPSIS
    ma_control_file_end()
*/

void ma_control_file_end()
{
  DBUG_ENTER("ma_control_file_end");
  my_close(control_file_fd, MYF(MY_WME));
  /*
    As this module owns these variables, closing the module forbids access to
    them (just a safety):
  */
  last_checkpoint_lsn= CONTROL_FILE_IMPOSSIBLE_LSN;
  last_logno= CONTROL_FILE_IMPOSSIBLE_FILENO;
  DBUG_VOID_RETURN;
}
