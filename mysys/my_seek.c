/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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

/* 
  Seek to a position in a file.

  ARGUMENTS
  File fd          The file descriptor
  my_off_t pos     The expected position (absolute or relative)
  int whence       A direction parameter and one of 
                   {SEEK_SET, SEEK_CUR, SEEK_END}
  myf MyFlags      MY_THREADSAFE must be set in case my_seek may be mixed
                   with my_pread/my_pwrite calls and fd is shared among
                   threads.

  DESCRIPTION
    The my_seek  function  is a wrapper around the system call lseek and
    repositions  the  offset of the file descriptor fd to the argument 
    offset according to the directive whence as follows:
      SEEK_SET    The offset is set to offset bytes.
      SEEK_CUR    The offset is set to its current location plus offset bytes
      SEEK_END    The offset is set to the size of the file plus offset bytes

  RETURN VALUE
    my_off_t newpos    The new position in the file.
    MY_FILEPOS_ERROR   An error was encountered while performing 
                       the seek. my_errno is set to indicate the
                       actual error.
*/

my_off_t my_seek(File fd, my_off_t pos, int whence, myf MyFlags)
{
  os_off_t newpos= -1;
  DBUG_ENTER("my_seek");
  DBUG_PRINT("my",("fd: %d Pos: %llu  Whence: %d  MyFlags: %d",
		   fd, (ulonglong) pos, whence, MyFlags));
  DBUG_ASSERT(pos != MY_FILEPOS_ERROR);		/* safety check */

  /*
      Make sure we are using a valid file descriptor!
  */
  DBUG_ASSERT(fd != -1);
#if defined (_WIN32)
  newpos= my_win_lseek(fd, pos, whence);
#else
  newpos= lseek(fd, pos, whence);
#endif
  if (newpos == (os_off_t) -1)
  {
    my_errno= errno;
    if (MyFlags & MY_WME)
      my_error(EE_CANT_SEEK, MYF(0), my_filename(fd), my_errno);
    DBUG_PRINT("error", ("lseek: %llu  errno: %d", (ulonglong) newpos, errno));
    DBUG_RETURN(MY_FILEPOS_ERROR);
  }
  if ((my_off_t) newpos != pos)
  {
    DBUG_PRINT("exit",("pos: %llu", (ulonglong) newpos));
  }
  DBUG_RETURN((my_off_t) newpos);
} /* my_seek */


	/* Tell current position of file */
	/* ARGSUSED */

my_off_t my_tell(File fd, myf MyFlags)
{
  os_off_t pos;
  DBUG_ENTER("my_tell");
  DBUG_PRINT("my",("fd: %d  MyFlags: %d",fd, MyFlags));
  DBUG_ASSERT(fd >= 0);
#if defined (HAVE_TELL) && !defined (_WIN32)
  pos= tell(fd);
#else
  pos= my_seek(fd, 0L, MY_SEEK_CUR,0);
#endif
  if (pos == (os_off_t) -1)
  {
    my_errno= errno;
    if (MyFlags & MY_WME)
      my_error(EE_CANT_SEEK, MYF(0), my_filename(fd), my_errno);
    DBUG_PRINT("error", ("tell: %llu  errno: %d", (ulonglong) pos, my_errno));
  }
  DBUG_PRINT("exit",("pos: %llu", (ulonglong) pos));
  DBUG_RETURN((my_off_t) pos);
} /* my_tell */
