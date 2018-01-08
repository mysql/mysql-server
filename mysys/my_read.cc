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
  @file mysys/my_read.cc
*/

#include "my_config.h"

#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "my_base.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysys_err.h"
#ifdef _WIN32
#include "mysys/mysys_priv.h"
#endif

/*
  Read a chunk of bytes from a file with retry's if needed

  The parameters are:
    File descriptor
    Buffer to hold at least Count bytes
    Bytes to read
    Flags on what to do on error

    Return:
      -1 on error
      0  if flag has bits MY_NABP or MY_FNABP set
      N  number of bytes read.
*/

size_t my_read(File Filedes, uchar *Buffer, size_t Count, myf MyFlags)
{
  size_t readbytes, save_count;
  DBUG_ENTER("my_read");
  DBUG_PRINT("my",("fd: %d  Buffer: %p  Count: %lu  MyFlags: %d",
                   Filedes, Buffer, (ulong) Count, MyFlags));
  save_count= Count;

  for (;;)
  {
    errno= 0;					/* Linux, Windows don't reset this on EOF/success */
#ifdef _WIN32
    readbytes= my_win_read(Filedes, Buffer, Count);
#else
    readbytes= read(Filedes, Buffer, Count);
#endif
    DBUG_EXECUTE_IF ("simulate_file_read_error",
                     {
                       errno= ENOSPC;
                       readbytes= (size_t) -1;
                       DBUG_SET("-d,simulate_file_read_error");
                       DBUG_SET("-d,simulate_my_b_fill_error");
                     });

    if (readbytes != Count)
    {
      set_my_errno(errno);
      if (errno == 0 || (readbytes != (size_t) -1 &&
                         (MyFlags & (MY_NABP | MY_FNABP))))
        set_my_errno(HA_ERR_FILE_TOO_SHORT);
      DBUG_PRINT("warning",("Read only %d bytes off %lu from %d, errno: %d",
                            (int) readbytes, (ulong) Count, Filedes,
                            my_errno()));

      if ((readbytes == 0 || (int) readbytes == -1) && errno == EINTR)
      {  
        DBUG_PRINT("debug", ("my_read() was interrupted and returned %ld",
                             (long) readbytes));
        continue;                              /* Interrupted */
      }

      if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
      {
        char errbuf[MYSYS_STRERROR_SIZE];
        if (readbytes == (size_t) -1)
          my_error(EE_READ, MYF(0), my_filename(Filedes),
                   my_errno(), my_strerror(errbuf, sizeof(errbuf), my_errno()));
        else if (MyFlags & (MY_NABP | MY_FNABP))
          my_error(EE_EOFERR, MYF(0), my_filename(Filedes),
                   my_errno(), my_strerror(errbuf, sizeof(errbuf), my_errno()));
      }
      if (readbytes == (size_t) -1 ||
          ((MyFlags & (MY_FNABP | MY_NABP)) && !(MyFlags & MY_FULL_IO)))
        DBUG_RETURN(MY_FILE_ERROR);	/* Return with error */
      if (readbytes != (size_t) -1 && (MyFlags & MY_FULL_IO))
      {
        Buffer+= readbytes;
        Count-= readbytes;
        continue;
      }
    }

    if (MyFlags & (MY_NABP | MY_FNABP))
      readbytes= 0;			/* Ok on read */
    else if (MyFlags & MY_FULL_IO)
      readbytes= save_count;
    break;
  }
  DBUG_RETURN(readbytes);
} /* my_read */
