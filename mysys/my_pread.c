/* Copyright (c) 2000, 2011, Oracle and/or its affiliates.
   Copyright (c) 2011, Monty Program Ab

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
#include "my_base.h"
#include <m_string.h>
#include <errno.h>
#ifndef _WIN32
#include <unistd.h>
#endif

/*
  Read a chunk of bytes from a file from a given position

  SYNOPSIOS
    my_pread()
    Filedes	File decsriptor
    Buffer	Buffer to read data into
    Count	Number of bytes to read
    offset	Position to read from
    MyFlags	Flags

  NOTES
    This differs from the normal pread() call in that we don't care
    to set the position in the file back to the original position
    if the system doesn't support pread().

  RETURN
    (size_t) -1   Error
    #             Number of bytes read
*/

size_t my_pread(File Filedes, uchar *Buffer, size_t Count, my_off_t offset,
                myf MyFlags)
{
  size_t readbytes;
  int error= 0;

  DBUG_ENTER("my_pread");

  DBUG_PRINT("my",("fd: %d  Seek: %llu  Buffer: %p  Count: %lu  MyFlags: %d",
             Filedes, (ulonglong)offset, Buffer, (ulong)Count, MyFlags));

  if (!(MyFlags & (MY_WME | MY_FAE | MY_FNABP)))
    MyFlags|= my_global_flags;

  for (;;)
  {
    errno= 0;    /* Linux, Windows don't reset this on EOF/success */
#ifdef _WIN32
    readbytes= my_win_pread(Filedes, Buffer, Count, offset);
#else
    readbytes= pread(Filedes, Buffer, Count, offset);
#endif
    error = (readbytes != Count);

    if (error)
    {
      my_errno= errno ? errno : -1;
      if (errno == 0 || (readbytes != (size_t) -1 &&
                         (MyFlags & (MY_NABP | MY_FNABP))))
        my_errno= HA_ERR_FILE_TOO_SHORT;
      DBUG_PRINT("warning",("Read only %d bytes off %u from %d, errno: %d",
                            (int) readbytes, (uint) Count,Filedes,my_errno));
      if ((readbytes == 0 || readbytes == (size_t) -1) && errno == EINTR)
      {
        DBUG_PRINT("debug", ("my_pread() was interrupted and returned %d",
                             (int) readbytes));
        continue;                              /* Interrupted */
      }
      if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
      {
	if (readbytes == (size_t) -1)
	  my_error(EE_READ,
                   MYF(ME_BELL | ME_WAITTANG | (MyFlags & (ME_JUST_INFO | ME_NOREFRESH))),
		   my_filename(Filedes),my_errno);
	else if (MyFlags & (MY_NABP | MY_FNABP))
	  my_error(EE_EOFERR,
                   MYF(ME_BELL | ME_WAITTANG | (MyFlags & (ME_JUST_INFO | ME_NOREFRESH))),
                   my_filename(Filedes),my_errno);
      }
      if (readbytes == (size_t) -1 || (MyFlags & (MY_FNABP | MY_NABP)))
        DBUG_RETURN(MY_FILE_ERROR);         /* Return with error */
    }
    if (MyFlags & (MY_NABP | MY_FNABP))
      DBUG_RETURN(0);                      /* Read went ok; Return 0 */
    DBUG_RETURN(readbytes);                /* purecov: inspected */
  }
} /* my_pread */


/*
  Write a chunk of bytes to a file at a given position

  SYNOPSIOS
    my_pwrite()
    Filedes	File decsriptor
    Buffer	Buffer to write data from
    Count	Number of bytes to write
    offset	Position to write to
    MyFlags	Flags

  NOTES
    This differs from the normal pwrite() call in that we don't care
    to set the position in the file back to the original position
    if the system doesn't support pwrite()

  RETURN
    (size_t) -1   Error
    #             Number of bytes read
*/

size_t my_pwrite(int Filedes, const uchar *Buffer, size_t Count,
                 my_off_t offset, myf MyFlags)
{
  size_t writtenbytes, written;
  uint errors;
  DBUG_ENTER("my_pwrite");
  DBUG_PRINT("my",("fd: %d  Seek: %llu  Buffer: %p  Count: %lu  MyFlags: %d",
             Filedes, (ulonglong)offset, Buffer, (ulong)Count, MyFlags));
  errors= 0;
  written= 0;
  if (!(MyFlags & (MY_WME | MY_FAE | MY_FNABP)))
    MyFlags|= my_global_flags;

  for (;;)
  {
#ifdef _WIN32
    writtenbytes= my_win_pwrite(Filedes, Buffer, Count,offset);
#else
    writtenbytes= pwrite(Filedes, Buffer, Count, offset);
#endif
    if (writtenbytes == Count)
      break;
    my_errno= errno;
    if (writtenbytes != (size_t) -1)
    {					/* Safegueard */
      written+=writtenbytes;
      Buffer+=writtenbytes;
      Count-=writtenbytes;
      offset+=writtenbytes;
    }
    DBUG_PRINT("error",("Write only %u bytes", (uint) writtenbytes));
#ifndef NO_BACKGROUND
    if (my_thread_var->abort)
      MyFlags&= ~ MY_WAIT_IF_FULL;		/* End if aborted by user */
    if ((my_errno == ENOSPC || my_errno == EDQUOT) &&
        (MyFlags & MY_WAIT_IF_FULL))
    {
      wait_for_free_space(my_filename(Filedes), errors);
      errors++;
      continue;
    }
    if ((writtenbytes && writtenbytes != (size_t) -1) || my_errno == EINTR)
      continue;					/* Retry */
#endif

    /* Don't give a warning if it's ok that we only write part of the data */
    if (MyFlags & (MY_NABP | MY_FNABP))
    {
      if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
        my_error(EE_WRITE, MYF(ME_BELL | ME_WAITTANG | (MyFlags & (ME_JUST_INFO | ME_NOREFRESH))),
                 my_filename(Filedes),my_errno);
      DBUG_RETURN(MY_FILE_ERROR);		/* Error on write */
    }
    break;					/* Return bytes written */
  }
  DBUG_EXECUTE_IF("check", my_seek(Filedes, -1, SEEK_SET, MYF(0)););
  if (MyFlags & (MY_NABP | MY_FNABP))
    DBUG_RETURN(0);                             /* Want only errors */
  DBUG_RETURN(writtenbytes+written);             /* purecov: inspected */
} /* my_pwrite */
