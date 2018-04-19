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
  @file mysys/my_rename.cc
*/

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysys_err.h"

#undef my_rename

/* On unix rename deletes to file if it exists */

int my_rename(const char *from, const char *to, myf MyFlags) {
  int error = 0;
  DBUG_ENTER("my_rename");
  DBUG_PRINT("my", ("from %s to %s MyFlags %d", from, to, MyFlags));

#if defined(_WIN32)
  if (!MoveFileEx(from, to,
                  MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING)) {
    my_osmaperr(GetLastError());
#else
  if (rename(from, to)) {
#endif
    set_my_errno(errno);
    error = -1;
    if (MyFlags & (MY_FAE + MY_WME)) {
      char errbuf[MYSYS_STRERROR_SIZE];
      my_error(EE_LINK, MYF(0), from, to, my_errno(),
               my_strerror(errbuf, sizeof(errbuf), my_errno()));
    }
  } else if (MyFlags & MY_SYNC_DIR) {
    /* do only the needed amount of syncs: */
    if (my_sync_dir_by_file(from, MyFlags) ||
        (strcmp(from, to) && my_sync_dir_by_file(to, MyFlags)))
      error = -1;
  }
  DBUG_RETURN(error);
} /* my_rename */
