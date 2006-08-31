/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "mysys_priv.h"
#include "mysys_err.h"
#include <errno.h>


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

uint my_read(File Filedes, byte *Buffer, uint Count, myf MyFlags)
{
  uint readbytes, save_count;
  DBUG_ENTER("my_read");
  DBUG_PRINT("my",("Fd: %d  Buffer: %lx  Count: %u  MyFlags: %d",
                   Filedes, Buffer, Count, MyFlags));
  save_count= Count;

  for (;;)
  {
    errno= 0;					/* Linux doesn't reset this */
    if ((readbytes= (uint) read(Filedes, Buffer, Count)) != Count)
    {
      my_errno= errno ? errno : -1;
      DBUG_PRINT("warning",("Read only %ld bytes off %ld from %d, errno: %d",
                            readbytes, Count, Filedes, my_errno));
#ifdef THREAD
      if ((int) readbytes <= 0 && errno == EINTR)
      {
        DBUG_PRINT("debug", ("my_read() was interrupted and returned %d", (int) readbytes));
        continue;				/* Interrupted */
      }
#endif
      if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
      {
        if ((int) readbytes == -1)
          my_error(EE_READ, MYF(ME_BELL+ME_WAITTANG),
                   my_filename(Filedes),my_errno);
        else if (MyFlags & (MY_NABP | MY_FNABP))
          my_error(EE_EOFERR, MYF(ME_BELL+ME_WAITTANG),
                   my_filename(Filedes),my_errno);
      }
      if ((int) readbytes == -1 ||
          ((MyFlags & (MY_FNABP | MY_NABP)) && !(MyFlags & MY_FULL_IO)))
        DBUG_RETURN(MY_FILE_ERROR);	/* Return with error */
      if (readbytes > 0 && (MyFlags & MY_FULL_IO))
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
