/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/*
  Logging of MyISAM commands and records on logfile for debugging
  The log can be examined with help of the myisamlog command.
*/

#include <fcntl.h>
#include <sys/types.h>

#include "my_byteorder.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "storage/myisam/myisam_sys.h"
#include "storage/myisam/myisamdef.h"
#ifdef _WIN32
#include <fcntl.h>
#include <process.h>
#endif

#define GETPID() (log_type == 1 ? (long)myisam_pid : (long)my_thread_self())

/* Activate logging if flag is 1 and reset logging if flag is 0 */

static int log_type = 0;
ulong myisam_pid = 0;

int mi_log(int activate_log) {
  int error = 0;
  char buff[FN_REFLEN];
  DBUG_ENTER("mi_log");

  log_type = activate_log;
  if (activate_log) {
    if (!myisam_pid) myisam_pid = (ulong)getpid();
    if (myisam_log_file < 0) {
      if ((myisam_log_file = mysql_file_create(
               mi_key_file_log,
               fn_format(buff, myisam_log_filename, "", ".log", 4), 0,
               (O_RDWR | O_APPEND), MYF(0))) < 0)
        DBUG_RETURN(my_errno());
    }
  } else if (myisam_log_file >= 0) {
    error = mysql_file_close(myisam_log_file, MYF(0)) ? my_errno() : 0;
    myisam_log_file = -1;
  }
  DBUG_RETURN(error);
}

/* Logging of records and commands on logfile */
/* All logs starts with command(1) dfile(2) process(4) result(2) */

void _myisam_log(enum myisam_log_commands command, MI_INFO *info,
                 const uchar *buffert, uint length) {
  uchar buff[11];
  int error, old_errno;
  ulong pid = (ulong)GETPID();
  old_errno = my_errno();
  memset(buff, 0, sizeof(buff));
  buff[0] = (char)command;
  mi_int2store(buff + 1, info->dfile);
  mi_int4store(buff + 3, pid);
  mi_int2store(buff + 9, length);

  mysql_mutex_lock(&THR_LOCK_myisam);
  error = my_lock(myisam_log_file, F_WRLCK, MYF(MY_SEEK_NOT_DONE));
  (void)mysql_file_write(myisam_log_file, buff, sizeof(buff), MYF(0));
  (void)mysql_file_write(myisam_log_file, buffert, length, MYF(0));
  if (!error) error = my_lock(myisam_log_file, F_UNLCK, MYF(MY_SEEK_NOT_DONE));
  mysql_mutex_unlock(&THR_LOCK_myisam);
  set_my_errno(old_errno);
}

void _myisam_log_command(enum myisam_log_commands command, MI_INFO *info,
                         const uchar *buffert, uint length, int result) {
  uchar buff[9];
  int error, old_errno;
  ulong pid = (ulong)GETPID();

  old_errno = my_errno();
  buff[0] = (char)command;
  mi_int2store(buff + 1, info->dfile);
  mi_int4store(buff + 3, pid);
  mi_int2store(buff + 7, result);
  mysql_mutex_lock(&THR_LOCK_myisam);
  error = my_lock(myisam_log_file, F_WRLCK, MYF(MY_SEEK_NOT_DONE));
  (void)mysql_file_write(myisam_log_file, buff, sizeof(buff), MYF(0));
  if (buffert) (void)mysql_file_write(myisam_log_file, buffert, length, MYF(0));
  if (!error) error = my_lock(myisam_log_file, F_UNLCK, MYF(MY_SEEK_NOT_DONE));
  mysql_mutex_unlock(&THR_LOCK_myisam);
  set_my_errno(old_errno);
}

void _myisam_log_record(enum myisam_log_commands command, MI_INFO *info,
                        const uchar *record, my_off_t filepos, int result) {
  uchar buff[21], *pos;
  int error, old_errno;
  uint length;
  ulong pid = (ulong)GETPID();

  old_errno = my_errno();
  if (!info->s->base.blobs)
    length = info->s->base.reclength;
  else
    length = info->s->base.reclength + _my_calc_total_blob_length(info, record);
  buff[0] = (uchar)command;
  mi_int2store(buff + 1, info->dfile);
  mi_int4store(buff + 3, pid);
  mi_int2store(buff + 7, result);
  mi_sizestore(buff + 9, filepos);
  mi_int4store(buff + 17, length);
  mysql_mutex_lock(&THR_LOCK_myisam);
  error = my_lock(myisam_log_file, F_WRLCK, MYF(MY_SEEK_NOT_DONE));
  (void)mysql_file_write(myisam_log_file, buff, sizeof(buff), MYF(0));
  (void)mysql_file_write(myisam_log_file, record, info->s->base.reclength,
                         MYF(0));
  if (info->s->base.blobs) {
    MI_BLOB *blob, *end;

    for (end = info->blobs + info->s->base.blobs, blob = info->blobs;
         blob != end; blob++) {
      memcpy(&pos, record + blob->offset + blob->pack_length, sizeof(char *));
      (void)mysql_file_write(myisam_log_file, pos, blob->length, MYF(0));
    }
  }
  if (!error) error = my_lock(myisam_log_file, F_UNLCK, MYF(MY_SEEK_NOT_DONE));
  mysql_mutex_unlock(&THR_LOCK_myisam);
  set_my_errno(old_errno);
}
