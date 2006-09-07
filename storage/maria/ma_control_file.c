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
#define CONTROL_FILE_MAGIC_STRING_SIZE (sizeof(CONTROL_FILE_MAGIC_STRING)-1)
#define CONTROL_FILE_CHECKSUM_OFFSET (CONTROL_FILE_MAGIC_STRING_OFFSET + CONTROL_FILE_MAGIC_STRING_SIZE)
#define CONTROL_FILE_CHECKSUM_SIZE 1
#define CONTROL_FILE_LSN_OFFSET (CONTROL_FILE_CHECKSUM_OFFSET + CONTROL_FILE_CHECKSUM_SIZE)
#define CONTROL_FILE_LSN_SIZE (4+4)
#define CONTROL_FILE_FILENO_OFFSET (CONTROL_FILE_LSN_OFFSET + CONTROL_FILE_LSN_SIZE)
#define CONTROL_FILE_FILENO_SIZE 4
#define CONTROL_FILE_SIZE (CONTROL_FILE_FILENO_OFFSET + CONTROL_FILE_FILENO_SIZE)

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

static char simple_checksum(char *buffer, uint size)
{
  /* TODO: improve this sum if we want */
  char s= 0;
  uint i;
  for (i= 0; i<size; i++)
    s+= buffer[i];
  return s;
}

/*
  Initialize control file subsystem

  SYNOPSIS
    ma_control_file_create_or_open()

  Looks for the control file. If absent, it's a fresh start, creates file.
  If present, reads it to find out last checkpoint's LSN and last log, updates
  the last_checkpoint_lsn and last_logno global variables.
  Called at engine's start.

  The format of the control file is:
  4 bytes: magic string
  1 byte: checksum of the following bytes
  4 bytes: number of log where last checkpoint is
  4 bytes: offset in log where last checkpoint is
  4 bytes: number of last log

  RETURN
    0 - OK
    1 - Error
*/
int ma_control_file_create_or_open()
{
  char buffer[CONTROL_FILE_SIZE];
  char name[FN_REFLEN];
  MY_STAT stat_buff;
  my_bool create_file;
  int open_flags= O_BINARY | /*O_DIRECT |*/ O_RDWR;
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

  create_file= test(my_access(name,F_OK));

  if (create_file)
  {
    if ((control_file_fd= my_create(name, 0, open_flags, MYF(0))) < 0)
      DBUG_RETURN(1);
    /*
      TODO: from "man fsync" on Linux:
      "fsync does not necessarily ensure that the entry in  the  directory
      containing  the file has also reached disk.  For that an explicit
      fsync on the file descriptor of the directory is also needed."
      So if we just created the file we should sync the directory.
      Maybe there should be a flag of my_create() to do this.

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
                                                CONTROL_FILE_UPDATE_ALL));
  }

  /* Otherwise, file exists */

  if ((control_file_fd= my_open(name, open_flags, MYF(MY_WME))) < 0)
    DBUG_RETURN(1);
  
  if (my_stat(name, &stat_buff, MYF(MY_WME)) == NULL)
    DBUG_RETURN(1);

  if ((uint)stat_buff.st_size != CONTROL_FILE_SIZE)
  {
    /*
      Given that normally we write only a sector and it's atomic, the only
      possibility for a file to be of too short size is if we crashed at the
      very first startup, between file creation and file write. Quite unlikely
      (and can be made even more unlikely by doing this: create a temp file,
      write it, and then rename it to be the control file).
      What's more likely is if someone forgot to restore the control file,
      just did a "touch control" to try to get Maria to start, or if the
      disk/filesystem has a problem.
      So let's be rigid.
    */
    my_message(0, "wrong file size", MYF(0)); /* TODO: improve errors */
    my_error(HA_ERR_CRASHED, MYF(0), name);
    DBUG_RETURN(1);
  }

  if (my_read(control_file_fd, buffer, CONTROL_FILE_SIZE,
              MYF(MY_FNABP | MY_WME)))
    DBUG_RETURN(1);
  if (memcmp(buffer + CONTROL_FILE_MAGIC_STRING_OFFSET,
             CONTROL_FILE_MAGIC_STRING, CONTROL_FILE_MAGIC_STRING_SIZE))
  {
    my_message(0, "bad magic string", MYF(0));
    DBUG_RETURN(1);
  }
  if (simple_checksum(buffer + CONTROL_FILE_LSN_OFFSET,
                      CONTROL_FILE_SIZE - CONTROL_FILE_LSN_OFFSET) !=
      buffer[CONTROL_FILE_CHECKSUM_OFFSET])
  {
    my_message(0, "checksum mismatch", MYF(0));
    DBUG_RETURN(1);
  }
  last_checkpoint_lsn= lsn8korr(buffer + CONTROL_FILE_LSN_OFFSET);
  last_logno= uint4korr(buffer + CONTROL_FILE_FILENO_OFFSET);

  DBUG_RETURN(0);
}


/*
  Write information durably to the control file; stores this information into
  the last_checkpoint_lsn and last_logno global variables.
  Called when we have created a new log (after syncing this log's creation)
  and when we have written a checkpoint (after syncing this log record).

  SYNOPSIS
    ma_control_file_write_and_force()
    checkpoint_lsn       LSN of last checkpoint
    logno                last log file number
    objs_to_write        which of the arguments should be used as new values
                         (for example, CONTROL_FILE_UPDATE_ONLY_LSN will not
                         write the logno argument to the control file and will
                         not update the last_logno global variable); can be:
                         CONTROL_FILE_UPDATE_ALL
                         CONTROL_FILE_UPDATE_ONLY_LSN
                         CONTROL_FILE_UPDATE_ONLY_LOGNO.

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
  char buffer[CONTROL_FILE_SIZE];
  my_bool update_checkpoint_lsn= FALSE, update_logno= FALSE;
  DBUG_ENTER("ma_control_file_write_and_force");

  memcpy(buffer + CONTROL_FILE_MAGIC_STRING_OFFSET,
         CONTROL_FILE_MAGIC_STRING, CONTROL_FILE_MAGIC_STRING_SIZE);

  /* TODO: you need some protection to be able to read last_* global vars */

  if (objs_to_write == CONTROL_FILE_UPDATE_ONLY_LSN)
    update_checkpoint_lsn= TRUE;
  else if (objs_to_write == CONTROL_FILE_UPDATE_ONLY_LOGNO)
    update_logno= TRUE;
  else if (objs_to_write == CONTROL_FILE_UPDATE_ALL)
    update_checkpoint_lsn= update_logno= TRUE;
  else /* incorrect value of objs_to_write */
    DBUG_ASSERT(0);

  if (update_checkpoint_lsn)
    lsn8store(buffer + CONTROL_FILE_LSN_OFFSET, checkpoint_lsn);
  else /* store old value == change nothing */
    lsn8store(buffer + CONTROL_FILE_LSN_OFFSET, &last_checkpoint_lsn);

  if (update_logno)
    int4store(buffer + CONTROL_FILE_FILENO_OFFSET, logno);
  else
    int4store(buffer + CONTROL_FILE_FILENO_OFFSET, last_logno);

  buffer[CONTROL_FILE_CHECKSUM_OFFSET]=
    simple_checksum(buffer + CONTROL_FILE_LSN_OFFSET,
                    CONTROL_FILE_SIZE - CONTROL_FILE_LSN_OFFSET);

  if (my_pwrite(control_file_fd, buffer, sizeof(buffer),
                0, MYF(MY_FNABP |  MY_WME)) ||
      my_sync(control_file_fd, MYF(MY_WME)))
    DBUG_RETURN(1);
  
  /* TODO: you need some protection to be able to write last_* global vars */
  if (update_checkpoint_lsn)
    last_checkpoint_lsn= *checkpoint_lsn;
  if (update_logno)
    last_logno= logno;

  DBUG_RETURN(0);
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
