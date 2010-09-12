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
#include "ma_checkpoint.h"
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
  - Max trid in control file (since Maria 1.5 May 2008)
  - Number of consecutive recovery failures (since Maria 1.5 May 2008)
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
#define CF_MAX_TRID_OFFSET (CF_FILENO_OFFSET + CF_FILENO_SIZE)
#define CF_MAX_TRID_SIZE TRANSID_SIZE
#define CF_RECOV_FAIL_OFFSET (CF_MAX_TRID_OFFSET + CF_MAX_TRID_SIZE)
#define CF_RECOV_FAIL_SIZE 1
#define CF_CHANGEABLE_TOTAL_SIZE (CF_RECOV_FAIL_OFFSET + CF_RECOV_FAIL_SIZE)

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
   The maximum transaction id given to a transaction. It is only updated at
   clean shutdown (in case of crash, logs have better information).
*/
TrID   max_trid_in_control_file= 0;

/**
  Number of consecutive log or recovery failures. Reset to 0 after recovery's
  success.
*/
uint8 recovery_failures= 0;

/**
   @brief If log's lock should be asserted when writing to control file.

   Can be re-used by any function which needs to be thread-safe except when
   it is called at startup.
*/
my_bool maria_multi_threaded= FALSE;
/** @brief if currently doing a recovery */
my_bool maria_in_recovery= FALSE;

/**
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
    DBUG_RETURN(CONTROL_FILE_UNKNOWN_ERROR);

  /*
    To be safer we should make sure that there are no logs or data/index
    files around (indeed it could be that the control file alone was deleted
    or not restored, and we should not go on with life at this point).

    Things should still be relatively safe as if someone tries to use
    an old table with a new control file the different uuid:s between
    the files will cause ma_open() to generate an HA_ERR_OLD_FILE
    error. When used from mysqld this will cause the table to be open
    in repair mode which will remove all dependencies between the
    table and the old control file.

    We could have a tool which can rebuild the control file, by reading the
    directory of logs, finding the newest log, reading it to find last
    checkpoint... Slow but can save your db. For this to be possible, we
    must always write to the control file right after writing the checkpoint
    log record, and do nothing in between (i.e. the checkpoint must be
    usable as soon as it has been written to the log).
  */

  /* init the file with these "undefined" values */
  DBUG_RETURN(ma_control_file_write_and_force(LSN_IMPOSSIBLE,
                                              FILENO_IMPOSSIBLE, 0, 0));
}


/**
  Locks control file exclusively. This is kept for the duration of the engine
  process, to prevent another Maria instance to write to our logs or control
  file.
*/

static int lock_control_file(const char *name)
{
  uint retry= 0;
  /*
    On Windows, my_lock() uses locking() which is mandatory locking and so
    prevents maria-recovery.test from copying the control file. And in case of
    crash, it may take a while for Windows to unlock file, causing downtime.
  */
  /**
    @todo BUG We should explore my_sopen(_SH_DENYWRD) to open or create the
    file under Windows.
  */
#ifndef __WIN__
  /*
    We can't here use the automatic wait in my_lock() as the alarm thread
    may not yet exists.
  */
  while (my_lock(control_file_fd, F_WRLCK, 0L, F_TO_EOF,
                 MYF(MY_SEEK_NOT_DONE | MY_FORCE_LOCK | MY_NO_WAIT)))
  {
    if (retry == 0)
      my_printf_error(HA_ERR_INITIALIZATION,
                      "Can't lock aria control file '%s' for exclusive use, "
                      "error: %d. Will retry for %d seconds", 0,
                      name, my_errno, MARIA_MAX_CONTROL_FILE_LOCK_RETRY);
    if (retry++ > MARIA_MAX_CONTROL_FILE_LOCK_RETRY)
      return 1;
    sleep(1);
  }
#endif
  return 0;
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

  @param create_if_missing create file if not found

  @return Operation status
    @retval 0      OK
    @retval 1      Error (in which case the file is left closed)
*/

CONTROL_FILE_ERROR ma_control_file_open(my_bool create_if_missing,
                                        my_bool print_error)
{
  uchar buffer[CF_MAX_SIZE];
  char name[FN_REFLEN], errmsg_buff[256];
  const char *errmsg, *lock_failed_errmsg= "Could not get an exclusive lock;"
    " file is probably in use by another process";
  uint new_cf_create_time_size, new_cf_changeable_size, new_block_size;
  my_off_t file_size;
  int open_flags= O_BINARY | /*O_DIRECT |*/ O_RDWR;
  int error= CONTROL_FILE_UNKNOWN_ERROR;
  DBUG_ENTER("ma_control_file_open");

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
  {
    CONTROL_FILE_ERROR create_error;
    if (!create_if_missing)
    {
      error= CONTROL_FILE_MISSING;
      errmsg= "Can't find file";
      goto err;
    }
    if ((create_error= create_control_file(name, open_flags)))
    {
      error= create_error;
      errmsg= "Can't create file";
      goto err;
    }
    if (lock_control_file(name))
    {
      errmsg= lock_failed_errmsg;
      goto err;
    }
    goto ok;
  }

  /* Otherwise, file exists */

  if ((control_file_fd= my_open(name, open_flags, MYF(MY_WME))) < 0)
  {
    errmsg= "Can't open file";
    goto err;
  }

  if (lock_control_file(name)) /* lock it before reading content */
  {
    errmsg= lock_failed_errmsg;
    goto err;
  }

  file_size= my_seek(control_file_fd, 0, SEEK_END, MYF(MY_WME));
  if (file_size == MY_FILEPOS_ERROR)
  {
    errmsg= "Can't read size";
    goto err;
  }
  if (file_size < CF_MIN_SIZE)
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
  if (file_size > CF_MAX_SIZE)
  {
    error= CONTROL_FILE_TOO_BIG;
    errmsg= "File size bigger than expected";
    goto err;
  }

  if (my_pread(control_file_fd, buffer, (size_t)file_size, 0, MYF(MY_FNABP)))
  {
    errmsg= "Can't read file";
    goto err;
  }

  if (memcmp(buffer + CF_MAGIC_STRING_OFFSET,
             CF_MAGIC_STRING, CF_MAGIC_STRING_SIZE))
  {
    error= CONTROL_FILE_BAD_MAGIC_STRING;
    errmsg= "Missing valid id at start of file. File is not a valid aria control file";
    goto err;
  }

  if (buffer[CF_VERSION_OFFSET] > CONTROL_FILE_VERSION)
  {
    error= CONTROL_FILE_BAD_VERSION;
    sprintf(errmsg_buff, "File is from a future aria system: %d. Current version is: %d",
            (int) buffer[CF_VERSION_OFFSET], CONTROL_FILE_VERSION);
    errmsg= errmsg_buff;
    goto err;
  }

  new_cf_create_time_size= uint2korr(buffer + CF_CREATE_TIME_SIZE_OFFSET);
  new_cf_changeable_size=  uint2korr(buffer + CF_CHANGEABLE_SIZE_OFFSET);

  if (new_cf_create_time_size < CF_MIN_CREATE_TIME_TOTAL_SIZE ||
      new_cf_changeable_size <  CF_MIN_CHANGEABLE_TOTAL_SIZE ||
      new_cf_create_time_size + new_cf_changeable_size != file_size)
  {
    error= CONTROL_FILE_INCONSISTENT_INFORMATION;
    errmsg= "Sizes stored in control file are inconsistent";
    goto err;
  }

  new_block_size= uint2korr(buffer + CF_BLOCKSIZE_OFFSET);
  if (new_block_size != maria_block_size && maria_block_size)
  {
    error= CONTROL_FILE_WRONG_BLOCKSIZE;
    sprintf(errmsg_buff,
            "Block size in control file (%u) is different than given aria_block_size: %u",
            new_block_size, (uint) maria_block_size);
    errmsg= errmsg_buff;
    goto err;
  }
  maria_block_size= new_block_size;

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
    errmsg= "Changeable part (end of control file) checksum mismatch";
    goto err;
  }

  memcpy(maria_uuid, buffer + CF_UUID_OFFSET, CF_UUID_SIZE);
  cf_create_time_size= new_cf_create_time_size;
  cf_changeable_size=  new_cf_changeable_size;
  last_checkpoint_lsn= lsn_korr(buffer + new_cf_create_time_size +
                                CF_LSN_OFFSET);
  last_logno= uint4korr(buffer + new_cf_create_time_size + CF_FILENO_OFFSET);
  if (new_cf_changeable_size >= (CF_MAX_TRID_OFFSET + CF_MAX_TRID_SIZE))
    max_trid_in_control_file=
      transid_korr(buffer + new_cf_create_time_size + CF_MAX_TRID_OFFSET);
  if (new_cf_changeable_size >= (CF_RECOV_FAIL_OFFSET + CF_RECOV_FAIL_SIZE))
    recovery_failures=
      (buffer + new_cf_create_time_size + CF_RECOV_FAIL_OFFSET)[0];

ok:
  DBUG_RETURN(0);

err:
  if (print_error)
    my_printf_error(HA_ERR_INITIALIZATION,
                    "Got error '%s' when trying to use aria control file "
                    "'%s'", 0, errmsg, name);
  ma_control_file_end(); /* will unlock file if needed */
  DBUG_RETURN(error);
}


/*
  Write information durably to the control file; stores this information into
  the last_checkpoint_lsn, last_logno, max_trid_in_control_file,
  recovery_failures global variables.
  Called when we have created a new log (after syncing this log's creation),
  when we have written a checkpoint (after syncing this log record), at
  shutdown (for storing trid in case logs are soon removed by user), and
  before and after recovery (to store recovery_failures).
  Variables last_checkpoint_lsn and last_logno must be protected by caller
  using log's lock, unless this function is called at startup.

  SYNOPSIS
    ma_control_file_write_and_force()
    last_checkpoint_lsn_arg LSN of last checkpoint
    last_logno_arg          last log file number
    max_trid_arg            maximum transaction longid
    recovery_failures_arg   consecutive recovery failures

  NOTE
    We always want to do one single my_pwrite() here to be as atomic as
    possible.

  RETURN
    0 - OK
    1 - Error
*/

int ma_control_file_write_and_force(LSN last_checkpoint_lsn_arg,
                                    uint32 last_logno_arg,
                                    TrID max_trid_arg,
                                    uint8 recovery_failures_arg)
{
  uchar buffer[CF_MAX_SIZE];
  uint32 sum;
  my_bool no_need_sync;
  DBUG_ENTER("ma_control_file_write_and_force");

  /*
    We don't need to sync if this is just an increase of
    recovery_failures: it's even good if that counter is not increased on disk
    in case of power or hardware failure (less false positives when removing
    logs).
  */
  no_need_sync= ((last_checkpoint_lsn == last_checkpoint_lsn_arg) &&
                 (last_logno == last_logno_arg) &&
                 (max_trid_in_control_file == max_trid_arg) &&
                 (recovery_failures_arg > 0));

  if (control_file_fd < 0)
    DBUG_RETURN(1);

#ifndef DBUG_OFF
  if (maria_multi_threaded)
    translog_lock_handler_assert_owner();
#endif

  lsn_store(buffer + CF_LSN_OFFSET, last_checkpoint_lsn_arg);
  int4store(buffer + CF_FILENO_OFFSET, last_logno_arg);
  transid_store(buffer + CF_MAX_TRID_OFFSET, max_trid_arg);
  (buffer + CF_RECOV_FAIL_OFFSET)[0]= recovery_failures_arg;

  if (cf_changeable_size > CF_CHANGEABLE_TOTAL_SIZE)
  {
    /*
      More room than needed for us. Must be a newer version. Clear part which
      we cannot maintain, so that any future version notices we didn't
      maintain its extra data.
    */
    uint zeroed= cf_changeable_size - CF_CHANGEABLE_TOTAL_SIZE;
    char msg[150];
    bzero(buffer + CF_CHANGEABLE_TOTAL_SIZE, zeroed);
    my_snprintf(msg, sizeof(msg),
                "Control file must be from a newer version; zero-ing out %u"
                " unknown bytes in control file at offset %u", zeroed,
                cf_changeable_size + cf_create_time_size);
    ma_message_no_user(ME_JUST_WARNING, msg);
  }
  else
  {
    /* not enough room for what we need to store: enlarge */
    cf_changeable_size= CF_CHANGEABLE_TOTAL_SIZE;
  }
  /* Note that the create-time portion is not touched */

  /* Checksum is stored first */
  compile_time_assert(CF_CHECKSUM_OFFSET == 0);
  sum= my_checksum(0, buffer + CF_CHECKSUM_SIZE,
                   cf_changeable_size - CF_CHECKSUM_SIZE);
  int4store(buffer, sum);

  if (my_pwrite(control_file_fd, buffer, cf_changeable_size,
                cf_create_time_size, MYF(MY_FNABP |  MY_WME)) ||
      (!no_need_sync && my_sync(control_file_fd, MYF(MY_WME))))
    DBUG_RETURN(1);

  last_checkpoint_lsn= last_checkpoint_lsn_arg;
  last_logno= last_logno_arg;
  max_trid_in_control_file= max_trid_arg;
  recovery_failures= recovery_failures_arg;

  cf_changeable_size= CF_CHANGEABLE_TOTAL_SIZE; /* no more warning */
  DBUG_RETURN(0);
}


/*
  Free resources taken by control file subsystem

  SYNOPSIS
    ma_control_file_end()
*/

int ma_control_file_end(void)
{
  int close_error;
  DBUG_ENTER("ma_control_file_end");

  if (control_file_fd < 0) /* already closed */
    DBUG_RETURN(0);

#ifndef __WIN__
  (void) my_lock(control_file_fd, F_UNLCK, 0L, F_TO_EOF,
                 MYF(MY_SEEK_NOT_DONE | MY_FORCE_LOCK));
#endif

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
  max_trid_in_control_file= recovery_failures= 0;

  DBUG_RETURN(close_error);
}


/**
  Tells if control file is initialized.
*/

my_bool ma_control_file_inited(void)
{
  return (control_file_fd >= 0);
}

#endif /* EXTRACT_DEFINITIONS */
