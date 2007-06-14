/* Copyright (C) 2000 MySQL AB

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

#include "mysys_priv.h"

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

my_off_t my_seek(File fd, my_off_t pos, int whence,
		 myf MyFlags __attribute__((unused)))
{
  reg1 os_off_t newpos= -1;
  DBUG_ENTER("my_seek");
  DBUG_PRINT("my",("Fd: %d  Hpos: %lu  Pos: %lu  Whence: %d  MyFlags: %d",
		   fd, (ulong) (((ulonglong) pos) >> 32), (ulong) pos, 
		   whence, MyFlags));
  DBUG_ASSERT(pos != MY_FILEPOS_ERROR);		/* safety check */

  /*
      Make sure we are using a valid file descriptor!
  */
  DBUG_ASSERT(fd != -1);
#if defined(THREAD) && !defined(HAVE_PREAD)
  if (MyFlags & MY_THREADSAFE)
  {
    pthread_mutex_lock(&my_file_info[fd].mutex);
    newpos= lseek(fd, pos, whence);
    pthread_mutex_unlock(&my_file_info[fd].mutex);
  }
  else
#endif
    newpos= lseek(fd, pos, whence);
  if (newpos == (os_off_t) -1)
  {
    my_errno=errno;
    DBUG_PRINT("error",("lseek: %lu  errno: %d", (ulong) newpos,errno));
    DBUG_RETURN(MY_FILEPOS_ERROR);
  }
  if ((my_off_t) newpos != pos)
  {
    DBUG_PRINT("exit",("pos: %lu", (ulong) newpos));
  }
  DBUG_RETURN((my_off_t) newpos);
} /* my_seek */


	/* Tell current position of file */
	/* ARGSUSED */

my_off_t my_tell(File fd, myf MyFlags __attribute__((unused)))
{
  os_off_t pos;
  DBUG_ENTER("my_tell");
  DBUG_PRINT("my",("Fd: %d  MyFlags: %d",fd, MyFlags));
  DBUG_ASSERT(fd >= 0);
#ifdef HAVE_TELL
  pos=tell(fd);
#else
  pos=lseek(fd, 0L, MY_SEEK_CUR);
#endif
  if (pos == (os_off_t) -1)
    my_errno=errno;
  DBUG_PRINT("exit",("pos: %lu", (ulong) pos));
  DBUG_RETURN((my_off_t) pos);
} /* my_tell */
