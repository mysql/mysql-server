/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/my_create.cc
*/

#include <fcntl.h>

#include "my_sys.h"
#if defined(_WIN32)
#include <share.h>
#endif

#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_thread_local.h"
#include "mysys_err.h"
#if defined(_WIN32)
#include "mysys/mysys_priv.h"
#endif

/*
** Create a new file
** Arguments:
** Path-name of file
** Read | write on file (umask value)
** Read & Write on open file
** Special flags
*/

File my_create(const char *FileName, int CreateFlags, int access_flags,
               myf MyFlags) {
  int fd, rc;
  DBUG_ENTER("my_create");
  DBUG_PRINT("my", ("Name: '%s' CreateFlags: %d  AccessFlags: %d  MyFlags: %d",
                    FileName, CreateFlags, access_flags, MyFlags));
#if defined(_WIN32)
  fd = my_win_open(FileName, access_flags | O_CREAT);
#else
  fd = open((char *)FileName, access_flags | O_CREAT,
            CreateFlags ? CreateFlags : my_umask);
#endif

  if ((MyFlags & MY_SYNC_DIR) && (fd >= 0) &&
      my_sync_dir_by_file(FileName, MyFlags)) {
    my_close(fd, MyFlags);
    fd = -1;
  }

  rc = my_register_filename(fd, FileName, FILE_BY_CREATE, EE_CANTCREATEFILE,
                            MyFlags);
  /*
    my_register_filename() may fail on some platforms even if the call to
    *open() above succeeds. In this case, don't leave the stale file because
    callers assume the file to not exist if my_create() fails, so they don't
    do any cleanups.
  */
  if (unlikely(fd >= 0 && rc < 0)) {
    int tmp = my_errno();
    my_close(fd, MyFlags);
    my_delete(FileName, MyFlags);
    set_my_errno(tmp);
  }

  DBUG_RETURN(rc);
} /* my_create */
