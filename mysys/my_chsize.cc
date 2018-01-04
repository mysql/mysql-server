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
  @file mysys/my_chsize.cc
*/

#include "my_config.h"

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

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
  Change size of file.

  SYNOPSIS
    my_chsize()
      fd		File descriptor
      new_length	New file size
      filler		If we don't have truncate, fill up all bytes after
			new_length with this character
      MyFlags		Flags

  DESCRIPTION
    my_chsize() truncates file if shorter else fill with the filler character.
    The function also changes the file pointer. Usually it points to the end
    of the file after execution.

  RETURN VALUE
    0	Ok
    1	Error 
*/
int my_chsize(File fd, my_off_t newlength, int filler, myf MyFlags)
{
  my_off_t oldsize;
  uchar buff[IO_SIZE];
  DBUG_ENTER("my_chsize");
  DBUG_PRINT("my",("fd: %d  length: %lu  MyFlags: %d",fd,(ulong) newlength,
		   MyFlags));

  if ((oldsize= my_seek(fd, 0L, MY_SEEK_END, MYF(MY_WME+MY_FAE))) == newlength)
    DBUG_RETURN(0);

  DBUG_PRINT("info",("old_size: %ld", (ulong) oldsize));

  if (oldsize > newlength)
  {
#ifdef _WIN32
    if (my_win_chsize(fd, newlength))
    {
      set_my_errno(errno);
      goto err;
    }
    DBUG_RETURN(0);
#elif defined(HAVE_FTRUNCATE)
    if (ftruncate(fd, (off_t) newlength))
    {
      set_my_errno(errno);
      goto err;
    }
    DBUG_RETURN(0);
#else
    /*
      Fill space between requested length and true length with 'filler'
      We should never come here on any modern machine
    */
    if (my_seek(fd, newlength, MY_SEEK_SET, MYF(MY_WME+MY_FAE))
        == MY_FILEPOS_ERROR)
    {
      goto err;
    }
    std::swap(newlength, oldsize);
#endif
  }

  /* Full file with 'filler' until it's as big as requested */
  memset(buff, filler, IO_SIZE);
  while (newlength-oldsize > IO_SIZE)
  {
    if (my_write(fd, buff, IO_SIZE, MYF(MY_NABP)))
      goto err;
    oldsize+= IO_SIZE;
  }
  if (my_write(fd,buff,(size_t) (newlength-oldsize), MYF(MY_NABP)))
    goto err;
  DBUG_RETURN(0);

err:
  DBUG_PRINT("error", ("errno: %d", errno));
  if (MyFlags & MY_WME)
  {
    char  errbuf[MYSYS_STRERROR_SIZE];
    my_error(EE_CANT_CHSIZE, MYF(0),
             my_errno(), my_strerror(errbuf, sizeof(errbuf), my_errno()));
  }
  DBUG_RETURN(1);
} /* my_chsize */
