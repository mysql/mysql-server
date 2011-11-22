/* Copyright (c) 2000-2007 MySQL AB, 2009 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mysys_priv.h"
#include "mysys_err.h"
#include "m_string.h"

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
      my_errno= errno;
      goto err;
    }
    DBUG_RETURN(0);
#elif defined(HAVE_FTRUNCATE)
    if (ftruncate(fd, (off_t) newlength))
    {
      my_errno= errno;
      goto err;
    }
    DBUG_RETURN(0);
#elif defined(HAVE_CHSIZE)
    if (chsize(fd, (off_t) newlength))
    {
      my_errno=errno;
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
    swap_variables(my_off_t, newlength, oldsize);
#endif
  }

  /* Full file with 'filler' until it's as big as requested */
  bfill(buff, IO_SIZE, filler);
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
    my_error(EE_CANT_CHSIZE, MYF(ME_BELL+ME_WAITTANG), my_errno);
  DBUG_RETURN(1);
} /* my_chsize */
