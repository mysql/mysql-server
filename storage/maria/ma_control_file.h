/* Copyright (C) 2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

#ifndef _ma_control_file_h
#define _ma_control_file_h

#define CONTROL_FILE_BASE_NAME "maria_log_control"
/*
  Major version for control file. Should only be changed when doing
  big changes that made the new control file incompatible with all
  older versions of Maria.
*/
#define CONTROL_FILE_VERSION   1

/* Here is the interface of this module */

/*
  LSN of the last checkoint
  (if last_checkpoint_lsn == LSN_IMPOSSIBLE then there was never a checkpoint)
*/
extern LSN last_checkpoint_lsn;
/*
  Last log number (if last_logno == FILENO_IMPOSSIBLE then there is no log
  file yet)
*/
extern uint32 last_logno;

extern my_bool maria_multi_threaded, maria_in_recovery;

typedef enum enum_control_file_error {
  CONTROL_FILE_OK= 0,
  CONTROL_FILE_TOO_SMALL,
  CONTROL_FILE_TOO_BIG,
  CONTROL_FILE_BAD_MAGIC_STRING,
  CONTROL_FILE_BAD_VERSION,
  CONTROL_FILE_BAD_CHECKSUM,
  CONTROL_FILE_BAD_HEAD_CHECKSUM,
  CONTROL_FILE_MISSING,
  CONTROL_FILE_INCONSISTENT_INFORMATION,
  CONTROL_FILE_WRONG_BLOCKSIZE,
  CONTROL_FILE_UNKNOWN_ERROR /* any other error */
} CONTROL_FILE_ERROR;

#define CONTROL_FILE_UPDATE_ALL 0
#define CONTROL_FILE_UPDATE_ONLY_LSN 1
#define CONTROL_FILE_UPDATE_ONLY_LOGNO 2

#ifdef	__cplusplus
extern "C" {
#endif

/*
  Looks for the control file. If none and creation was requested, creates file.
  If present, reads it to find out last checkpoint's LSN and last log.
  Called at engine's start.
*/
CONTROL_FILE_ERROR ma_control_file_create_or_open();
/*
  Write information durably to the control file.
  Called when we have created a new log (after syncing this log's creation)
  and when we have written a checkpoint (after syncing this log record).
*/
int ma_control_file_write_and_force(const LSN checkpoint_lsn, uint32 logno,
                                    uint objs_to_write);


/* Free resources taken by control file subsystem */
int ma_control_file_end();

#ifdef	__cplusplus
}
#endif
#endif
