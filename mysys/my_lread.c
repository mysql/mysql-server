/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA */

#include "mysys_priv.h"
#include "mysys_err.h"

	/* Read a chunk of bytes from a file  */

uint32 my_lread(int Filedes, byte *Buffer, uint32 Count, myf MyFlags)
				/* File descriptor */
				/* Buffer must be at least count bytes */
				/* Max number of bytes returnd */
				/* Flags on what to do on error */
{
  uint32 readbytes;
  DBUG_ENTER("my_lread");
  DBUG_PRINT("my",("Fd: %d  Buffer: %ld  Count: %ld  MyFlags: %d",
		   Filedes, Buffer, Count, MyFlags));

  /* Temp hack to get count to int32 while read wants int */
  if ((readbytes = (uint32) read(Filedes, Buffer, (uint) Count)) != Count)
  {
    my_errno=errno;
    if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
    {
      if (readbytes == MY_FILE_ERROR)
	my_error(EE_READ, MYF(ME_BELL+ME_WAITTANG),
		 my_filename(Filedes),errno);
      else
      if (MyFlags & (MY_NABP | MY_FNABP))
	my_error(EE_EOFERR, MYF(ME_BELL+ME_WAITTANG),
		 my_filename(Filedes),errno);
    }
    if (readbytes == MY_FILE_ERROR || MyFlags & (MY_NABP | MY_FNABP))
      DBUG_RETURN((uint32) -1);		/* Return med felkod */
  }
  if (MyFlags & (MY_NABP | MY_FNABP))
    DBUG_RETURN(0);			/* Ok vid l{sning */
  DBUG_RETURN(readbytes);
} /* my_lread */
