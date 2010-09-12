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

#define CONTROL_FILE_BASE_NAME "aria_log_control"
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

extern TrID max_trid_in_control_file;

extern uint8 recovery_failures;

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

C_MODE_START
CONTROL_FILE_ERROR ma_control_file_open(my_bool create_if_missing,
                                        my_bool print_error);
int ma_control_file_write_and_force(LSN last_checkpoint_lsn_arg,
                                    uint32 last_logno_arg, TrID max_trid_arg,
                                    uint8 recovery_failures_arg);
int ma_control_file_end(void);
my_bool ma_control_file_inited(void);
C_MODE_END
#endif
