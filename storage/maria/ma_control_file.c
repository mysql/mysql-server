/* Copyright (C) 2007 MySQL AB & Guilhem Bichot & Michael Widenius

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  WL#3234 Maria control file
  First version written by Guilhem Bichot on 2006-04-27.
*/

#ifndef EXTRACT_DEFINITIONS
#include "maria_def.h"
#endif

/*
  A control file contains the following objects:

Start of create time variables (at start of file):
  - Magic string (including version number of Maria control file)
  - Uuid
  - Size of create time part
  - Size of dynamic part
  - Maria block size
.....  Here we can add new variables without changing format
  - Checksum of create time part (last of block)

Start of changeable part:
  - Checksum of changeable part
  - LSN of last checkpoint
  - Number of last log file
.....  Here we can add new variables without changing format

The idea is that one can add new variables to the control file and still
use it with old program versions. If one needs to do an incompatible change
one should increment the control file version number.
*/

/* Total size should be < sector size for atomic write operation */
#define CF_MAX_SIZE 512
#define CF_MIN_SIZE (CF_BLOCKSIZE_OFFSET + CF_BLOCKSIZE_SIZE + \
                     CF_CHECKSUM_SIZE * 2 + CF_LSN_SIZE + CF_FILENO_SIZE)

/* Create time variables */
#define CF_MAGIC_STRING "\xfe\xfe\xc"
#define CF_MAGIC_STRING_OFFSET 0
#define CF_MAGIC_STRING_SIZE   (sizeof(CF_MAGIC_STRING)-1)
#define CF_VERSION_OFFSET      (CF_MAGIC_STRING_OFFSET + CF_MAGIC_STRING_SIZE)
#define CF_VERSION_SIZE        1
#define CF_UUID_OFFSET         (CF_VERSION_OFFSET + CF_VERSION_SIZE)
#define CF_UUID_SIZE           MY_UUID_SIZE
#define CF_CREATE_TIME_SIZE_OFFSET  (CF_UUID_OFFSET + CF_UUID_SIZE)
#define CF_SIZE_SIZE           2
#define CF_CHANGEABLE_SIZE_OFFSET   (CF_CREATE_TIME_SIZE_OFFSET + CF_SIZE_SIZE)
#define CF_BLOCKSIZE_OFFSET    (CF_CHANGEABLE_SIZE_OFFSET + CF_SIZE_SIZE)
#define CF_BLOCKSIZE_SIZE      2

#define CF_CREATE_TIME_TOTAL_SIZE (CF_BLOCKSIZE_OFFSET + CF_BLOCKSIZE_SIZE + \
                                   CF_CHECKSUM_SIZE)

/*
  Start of the part that changes during execution
  This is stored at offset uint2korr(file[CF_CHANGEABLE_SIZE])
*/
#define CF_CHECKSUM_OFFSET 0
#define CF_CHECKSUM_SIZE 4
#define CF_LSN_OFFSET (CF_CHECKSUM_OFFSET + CF_CHECKSUM_SIZE)
#define CF_LSN_SIZE LSN_STORE_SIZE
#define CF_FILENO_OFFSET (CF_LSN_OFFSET + CF_LSN_SIZE)
#define CF_FILENO_SIZE 4

#define CF_CHANGEABLE_TOTAL_SIZE (CF_FILENO_OFFSET + CF_FILENO_SIZE)

/*
  The following values should not be changed, except when changing version
  number of the maria control file. These are the minimum sizes of the
  parts the code can handle.
*/

#define CF_MIN_CREATE_TIME_TOTAL_SIZE \
(CF_BLOCKSIZE_OFFSET + CF_BLOCKSIZE_SIZE + CF_CHECKSUM_SIZE)
#define CF_MIN_CHANGEABLE_TOTAL_SIZE \
(CF_FILENO_OFFSET + CF_FILENO_SIZE)

#ifndef EXTRACT_DEFINITIONS

/* This module owns these two vars. */
/**
   This LSN serves for the two-checkpoint rule, and also to find the
   checkpoint record when doing a recovery.
*/
LSN    last_checkpoint_lsn= LSN_IMPOSSIBLE;
uint32 last_logno=          FILENO_IMPOSSIBLE;

/**
   @brief If log's lock should be asserted when writing to control file.

   Can be re-used by any function which needs to be thread-safe except when
   it is called at startup.
*/
my_bool maria_multi_threaded= FALSE;
/** @brief if currently doing a recovery */
my_bool maria_in_recovery= FALSE;

/*
  Control file is less then  512 bytes (a disk sector),
  to be as atomic as possible
*/
static int control_file_fd= -1;

static uint cf_create_time_size;
static uint cf_changeable_size;

/**
   @brief Create Maria control file
*/

static CONTROL_FILE_ERROR create_control_file(const char *name,
                                              int open_flags)
{
  uint32 sum;
  uchar buffer[CF_CREATE_TIME_TOTAL_SIZE];
  DBUG_ENTER("maria_create_control_file");

  /* in a recovery, we expect to find a control file */
  if (maria_in_recovery)
    DBUG_RETURN(CONTROL_FILE_MISSING);
  if ((control_file_fd= my_create(name, 0,
                                  open_flags,
                                  MYF(MY_SYNC_DIR | MY_WME))) < 0)
    DBUG_RETURN(CONTROL_FILE_UNKNOWN_ERROR);

  /* Reset variables, as we are creating the file */
  cf_create_time_size= CF_CREATE_TIME_TOTAL_SIZE;
  cf_changeable_size=  CF_CHANGEABLE_TOTAL_SIZE;

  /* Create unique uuid for the control file */
  my_uuid_init((ulong) &buffer, (ulong) &maria_uuid);
  my_uuid(maria_uuid);

  /* Prepare and write the file header */
  memcpy(buffer, CF_MAGIC_STRING, CF_MAGIC_STRING_SIZE);
  buffer[CF_VERSION_OFFSET]= CONTROL_FILE_VERSION;
  memcpy(buffer + CF_UUID_OFFSET, maria_uuid, CF_UUID_SIZE);
  int2store(buffer + CF_CREATE_TIME_SIZE_OFFSET, cf_create_time_size);
  int2store(buffer + CF_CHANGEABLE_SIZE_OFFSET, cf_changeable_size);

  /* Write create time variables */
  int2store(buffer + CF_BLOCKSIZE_OFFSET, maria_block_size);

  /* Store checksum for create time parts */
  sum= (uint32) my_checksum(0, buffer, cf_create_time_size -
                            CF_CHECKSUM_SIZE);
  int4store(buffer + cf_create_time_size - CF_CHECKSUM_SIZE, sum);

  if (my_pwrite(control_file_fd, buffer, cf_create_time_size,
                0, MYF(MY_FNABP |  MY_WME)))
    DBUG_RETURN(1);

  /*
    To be safer we should make sure that there are no logs or data/index
    files around (indeed it could be that the control file alone was deleted
    or not restored, and we should not go on with life at this point).

    TODO: For now we trust (this is alpha version), but for beta if would
    be great to verify.

    We could have a tool which can rebuild the control file, by reading the
    directory of logs, finding the newest log, reading it to find last
    checkpoint... Slow but can save your db. For this to be possible, we
    must always write to the control file right after writing the checkpoint
    log record, and do nothing in between (i.e. the checkpoint must be
    usable as soon as it has been written to the log).
  */

  /* init the file with these "undefined" values */
  DBUG_RETURN(ma_control_file_write_and_force(LSN_IMPOSSIBLE,
                                              FILENO_IMPOSSIBLE,
                                              CONTROL_FILE_UPDATE_ALL));
}

/*
  @brief Initialize control file subsystem

  Looks for the control file. If none and creation is requested, creates file.
  If present, reads it to find out last checkpoint's LSN and last log, updates
  the last_checkpoint_lsn and last_logno global variables.
  Called at engine's start.

  @note
    The format of the control file is defined in the comments and defines
    at the start of this file.

  @note If in recovery, file is not created

  @return Operation status
    @retval 0      OK
    @retval 1      Error (in which case the file is left closed)
*/

CONTROL_FILE_ERROR ma_control_file_create_or_open()
{
  uchar buffer[CF_MAX_SIZE];
  char name[FN_REFLEN], errmsg_buff[256];
  const char *errmsg;
  MY_STAT stat_buff;
  uint new_cf_create_time_size, new_cf_changeable_size, new_block_size;
  uint retry;
  int open_flags= O_BINARY | /*O_DIRECT |*/ O_RDWR;
  int error= CONTROL_FILE_UNKNOWN_ERROR;
  DBUG_ENTER("ma_control_file_create_or_open");

  /*
    If you change sizes in the #defines, you at least have to change the
    "*store" and "*korr" calls in this file, and can even create backward
    compatibility problems. Beware!
  */
  DBUG_ASSERT(CF_LSN_SIZE == (3+4));
  DBUG_ASSERT(CF_FILENO_SIZE == 4);

  if (control_file_fd >= 0) /* already open */
    DBUG_RETURN(0);

  if (fn_format(name, CONTROL_FILE_BASE_NAME,
                maria_data_root, "", MYF(MY_WME)) == NullS)
    DBUG_RETURN(CONTROL_FILE_UNKNOWN_ERROR);

  if (my_access(name,F_OK))
    DBUG_RETURN(create_control_file(name, open_flags));

  /* Otherwise, file exists */

  if ((control_file_fd= my_open(name, open_flags, MYF(MY_WME))) < 0)
  {
    errmsg= "Can't open file";
    goto err;
  }

  if (my_stat(name, &stat_buff, MYF(0)) == NULL)
  {
    errmsg= "Can't read status";
    goto err;
  }

  if ((uint) stat_buff.st_size < CF_MIN_SIZE)
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
    error= CONTROL_FILE_TOO_SMALL;
    errmsg= "Size of control file is smaller than expected";
    goto err;
  }

  /* Check if control file is unexpectedly big */
  if ((uint)stat_buff.st_size > CF_MAX_SIZE)
  {
    error= CONTROL_FILE_TOO_BIG;
    errmsg= "File size bigger than expected";
    goto err;
  }

  if (my_read(control_file_fd, buffer, stat_buff.st_size, MYF(MY_FNABP)))
  {
    errmsg= "Can't read file";
    goto err;
  }

  if (memcmp(buffer + CF_MAGIC_STRING_OFFSET,
             CF_MAGIC_STRING, CF_MAGIC_STRING_SIZE))
  {
    error= CONTROL_FILE_BAD_MAGIC_STRING;
    errmsg= "Missing valid id at start of file. File is not a valid maria control file";
    goto err;
  }

  if (buffer[CF_VERSION_OFFSET] > CONTROL_FILE_VERSION)
  {
    error= CONTROL_FILE_BAD_VERSION;
    sprintf(errmsg_buff, "File is from a future maria system: %d. Current version is: %d",
            (int) buffer[CF_VERSION_OFFSET], CONTROL_FILE_VERSION);
    errmsg= errmsg_buff;
    goto err;
  }

  new_cf_create_time_size= uint2korr(buffer + CF_CREATE_TIME_SIZE_OFFSET);
  new_cf_changeable_size=  uint2korr(buffer + CF_CHANGEABLE_SIZE_OFFSET);

  if (new_cf_create_time_size < CF_MIN_CREATE_TIME_TOTAL_SIZE ||
      new_cf_changeable_size <  CF_MIN_CHANGEABLE_TOTAL_SIZE ||
      new_cf_create_time_size + new_cf_changeable_size !=
      stat_buff.st_size)
  {
    error= CONTROL_FILE_INCONSISTENT_INFORMATION;
    errmsg= "Sizes stored in control file are inconsistent";
    goto err;
  }

  new_block_size= uint2korr(buffer + CF_BLOCKSIZE_OFFSET);
  if (new_block_size != maria_block_size)
  {
    error= CONTROL_FILE_WRONG_BLOCKSIZE;
    sprintf(errmsg_buff,
            "Block size in control file (%u) is different than given maria_block_size: %u",
            new_block_size, (uint) maria_block_size);
    errmsg= errmsg_buff;
    goto err;
  }

  if (my_checksum(0, buffer, new_cf_create_time_size - CF_CHECKSUM_SIZE) !=
      uint4korr(buffer + new_cf_create_time_size - CF_CHECKSUM_SIZE))
  {
    error= CONTROL_FILE_BAD_HEAD_CHECKSUM;
    errmsg= "Fixed part checksum mismatch";
    goto err;
  }

  if (my_checksum(0, buffer + new_cf_create_time_size + CF_CHECKSUM_SIZE,
                  new_cf_changeable_size - CF_CHECKSUM_SIZE) !=
      uint4korr(buffer + new_cf_create_time_size))
  {
    error= CONTROL_FILE_BAD_CHECKSUM;
    errmsg= "Changeable part (end of control file) checksum missmatch";
    goto err;
  }

  memcpy(maria_uuid, buffer + CF_UUID_OFFSET, CF_UUID_SIZE);
  cf_create_time_size= new_cf_create_time_size;
  cf_changeable_size=  new_cf_changeable_size;
  last_checkpoint_lsn= lsn_korr(buffer + new_cf_create_time_size +
                                CF_LSN_OFFSET);
  last_logno= uint4korr(buffer + new_cf_create_time_size + CF_FILENO_OFFSET);

  retry= 0;

  /*
    We can't here use the automatic wait in my_lock() as the alarm thread
    may not yet exists.
  */

  while (my_lock(control_file_fd, F_WRLCK, 0L, F_TO_EOF,
                 MYF(MY_SEEK_NOT_DONE | MY_FORCE_LOCK | MY_NO_WAIT)))
  {
    if (retry == 0)
      my_printf_error(HA_ERR_INITIALIZATION,
                      "Can't lock maria control file '%s' for exclusive use, "
                      "error: %d. Will retry for %d seconds", 0,
                      name, my_errno, MARIA_MAX_CONTROL_FILE_LOCK_RETRY);
    if (retry++ > MARIA_MAX_CONTROL_FILE_LOCK_RETRY)
    {
      errmsg= "Could not get an exclusive lock; File is probably in use by another process";
      goto err;
    }
    sleep(1);
  }

  DBUG_RETURN(0);

err:
  my_printf_error(HA_ERR_INITIALIZATION,
                  "Error when trying to use maria control file '%s': %s", 0,
                  name, errmsg);
  ma_control_file_end();
  DBUG_RETURN(error);
}


/*
  Write information durably to the control file; stores this information into
  the last_checkpoint_lsn and last_logno global variables.
  Called when we have created a new log (after syncing this log's creation)
  and when we have written a checkpoint (after syncing this log record).
  Variables last_checkpoint_lsn and last_logno must be protected by caller
  using log's lock, unless this function is called at startup.

  SYNOPSIS
    ma_control_file_write_and_force()
    checkpoint_lsn       LSN of last checkpoint
    logno                last log file number
    objs_to_write        which of the arguments should be used as new values
                         (for example, CF_UPDATE_ONLY_LSN will not
                         write the logno argument to the control file and will
                         not update the last_logno global variable); can be:
                         CF_UPDATE_ALL
                         CF_UPDATE_ONLY_LSN
                         CF_UPDATE_ONLY_LOGNO.

  NOTE
    We always want to do one single my_pwrite() here to be as atomic as
    possible.

  RETURN
    0 - OK
    1 - Error
*/

int ma_control_file_write_and_force(const LSN checkpoint_lsn, uint32 logno,
                                    uint objs_to_write)
{
  char buffer[CF_MAX_SIZE];
  my_bool update_checkpoint_lsn= FALSE, update_logno= FALSE;
  uint32 sum;
  DBUG_ENTER("ma_control_file_write_and_force");

  DBUG_ASSERT(control_file_fd >= 0); /* must be open */
#ifndef DBUG_OFF
  if (maria_multi_threaded)
    translog_lock_handler_assert_owner();
#endif

  if (objs_to_write == CONTROL_FILE_UPDATE_ONLY_LSN)
    update_checkpoint_lsn= TRUE;
  else if (objs_to_write == CONTROL_FILE_UPDATE_ONLY_LOGNO)
    update_logno= TRUE;
  else if (objs_to_write == CONTROL_FILE_UPDATE_ALL)
    update_checkpoint_lsn= update_logno= TRUE;
  else /* incorrect value of objs_to_write */
    DBUG_ASSERT(0);

  if (update_checkpoint_lsn)
    lsn_store(buffer + CF_LSN_OFFSET, checkpoint_lsn);
  else /* store old value == change nothing */
    lsn_store(buffer + CF_LSN_OFFSET, last_checkpoint_lsn);

  if (update_logno)
    int4store(buffer + CF_FILENO_OFFSET, logno);
  else
    int4store(buffer + CF_FILENO_OFFSET, last_logno);

  /*
    Clear unknown part of changeable part.
    Other option would be to remember the original values in the file
    and copy them here, but this should be safer.
   */
  bzero(buffer + CF_CHANGEABLE_TOTAL_SIZE,
        cf_changeable_size - CF_CHANGEABLE_TOTAL_SIZE);

  /* Checksum is stored first */
  compile_time_assert(CF_CHECKSUM_OFFSET == 0);
  sum= my_checksum(0, buffer + CF_CHECKSUM_SIZE,
                   cf_changeable_size - CF_CHECKSUM_SIZE);
  int4store(buffer, sum);

  if (my_pwrite(control_file_fd, buffer, cf_changeable_size,
                cf_create_time_size, MYF(MY_FNABP |  MY_WME)) ||
      my_sync(control_file_fd, MYF(MY_WME)))
    DBUG_RETURN(1);

  if (update_checkpoint_lsn)
    last_checkpoint_lsn= checkpoint_lsn;
  if (update_logno)
    last_logno= logno;

  DBUG_RETURN(0);
}


/*
  Free resources taken by control file subsystem

  SYNOPSIS
    ma_control_file_end()
*/

int ma_control_file_end()
{
  int close_error;
  DBUG_ENTER("ma_control_file_end");

  if (control_file_fd < 0) /* already closed */
    DBUG_RETURN(0);

  (void) my_lock(control_file_fd, F_UNLCK, 0L, F_TO_EOF,
                 MYF(MY_SEEK_NOT_DONE | MY_FORCE_LOCK));

  close_error= my_close(control_file_fd, MYF(MY_WME));
  /*
    As my_close() frees structures even if close() fails, we do the same,
    i.e. we mark the file as closed in all cases.
  */
  control_file_fd= -1;
  /*
    As this module owns these variables, closing the module forbids access to
    them (just a safety):
  */
  last_checkpoint_lsn= LSN_IMPOSSIBLE;
  last_logno= FILENO_IMPOSSIBLE;

  DBUG_RETURN(close_error);
}

#endif /* EXTRACT_DEFINITIONS */
