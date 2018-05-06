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
  @file mysys/my_file.cc
*/

#include "my_config.h"

#include <string.h>
#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"
#include "mysys/my_static.h"
#include "mysys/mysys_priv.h"
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h> /* RLIMIT_NOFILE */
#endif

/*
  set how many open files we want to be able to handle

  SYNOPSIS
    set_maximum_open_files()
    max_file_limit		Files to open

  NOTES
    The request may not fulfilled becasue of system limitations

  RETURN
    Files available to open.
    May be more or less than max_file_limit!
*/

#if defined(HAVE_GETRLIMIT)

/*
  This value is certainly wrong on all 64bit platforms,
  and also wrong on many 32bit platforms.
  It is better to get a compile error, than to use a wrong value.
#ifndef RLIM_INFINITY
#define RLIM_INFINITY ((uint) 0xffffffff)
#endif
*/

static uint set_max_open_files(uint max_file_limit) {
  struct rlimit rlimit;
  uint old_cur;
  DBUG_ENTER("set_max_open_files");
  DBUG_PRINT("enter", ("files: %u", max_file_limit));

  if (!getrlimit(RLIMIT_NOFILE, &rlimit)) {
    old_cur = (uint)rlimit.rlim_cur;
    DBUG_PRINT("info", ("rlim_cur: %u  rlim_max: %u", (uint)rlimit.rlim_cur,
                        (uint)rlimit.rlim_max));
    if (rlimit.rlim_cur == (rlim_t)RLIM_INFINITY)
      rlimit.rlim_cur = max_file_limit;
    if (rlimit.rlim_cur >= max_file_limit)
      DBUG_RETURN(rlimit.rlim_cur); /* purecov: inspected */
    rlimit.rlim_cur = rlimit.rlim_max = max_file_limit;
    if (setrlimit(RLIMIT_NOFILE, &rlimit))
      max_file_limit = old_cur; /* Use original value */
    else {
      rlimit.rlim_cur = 0; /* Safety if next call fails */
      (void)getrlimit(RLIMIT_NOFILE, &rlimit);
      DBUG_PRINT("info", ("rlim_cur: %u", (uint)rlimit.rlim_cur));
      if (rlimit.rlim_cur) /* If call didn't fail */
        max_file_limit = (uint)rlimit.rlim_cur;
    }
  }
  DBUG_PRINT("exit", ("max_file_limit: %u", max_file_limit));
  DBUG_RETURN(max_file_limit);
}

#else
static uint set_max_open_files(uint max_file_limit) {
  /* We don't know the limit. Return best guess */
  return MY_MIN(max_file_limit, OS_FILE_LIMIT);
}
#endif

/*
  Change number of open files

  SYNOPSIS:
    my_set_max_open_files()
    files		Number of requested files

  RETURN
    number of files available for open
*/

uint my_set_max_open_files(uint files) {
  struct st_my_file_info *tmp;
  DBUG_ENTER("my_set_max_open_files");
  DBUG_PRINT("enter", ("files: %u  my_file_limit: %u", files, my_file_limit));

  files += MY_FILE_MIN;
  files = set_max_open_files(MY_MIN(files, OS_FILE_LIMIT));
  if (files <= MY_NFILE) DBUG_RETURN(files);

  if (!(tmp = (struct st_my_file_info *)my_malloc(
            key_memory_my_file_info, sizeof(*tmp) * files, MYF(MY_WME))))
    DBUG_RETURN(MY_NFILE);

  /* Copy any initialized files */
  memcpy((char *)tmp, (char *)my_file_info,
         sizeof(*tmp) * MY_MIN(my_file_limit, files));
  memset((tmp + my_file_limit), 0,
         MY_MAX((int)(files - my_file_limit), 0) * sizeof(*tmp));
  my_free_open_file_info(); /* Free if already allocated */
  my_file_info = tmp;
  my_file_limit = files;
  DBUG_PRINT("exit", ("files: %u", files));
  DBUG_RETURN(files);
}

void my_free_open_file_info() {
  DBUG_ENTER("my_free_file_info");
  if (my_file_info != my_file_info_default) {
    /* Copy data back for my_print_open_files */
    memcpy((char *)my_file_info_default, my_file_info,
           sizeof(*my_file_info_default) * MY_NFILE);
    my_free(my_file_info);
    my_file_info = my_file_info_default;
    my_file_limit = MY_NFILE;
  }
  DBUG_VOID_RETURN;
}
