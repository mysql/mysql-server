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

/* USE_MY_STREAM isn't set because we can't thrust my_fclose! */

#include "mysys_priv.h"
#include "mysys_err.h"
#include <errno.h>
#include <stdio.h>

#ifdef HAVE_FSEEKO
#undef ftell
#undef fseek
#define ftell(A) ftello(A)
#define fseek(A,B,C) fseeko((A),(B),(C))
#endif

	/* Read a chunk of bytes from a file  */
	/* Returns (uint) -1 if error as my_read() */

uint my_fread(FILE *stream, byte *Buffer, uint Count, myf MyFlags)
			/* File descriptor */
			/* Buffer must be at least count bytes */
			/* Max number of bytes returnd */
			/* Flags on what to do on error */
{
  uint readbytes;
  DBUG_ENTER("my_fread");
  DBUG_PRINT("my",("stream: 0x%lx  Buffer: 0x%lx  Count: %u  MyFlags: %d",
		   stream, Buffer, Count, MyFlags));

  if ((readbytes = (uint) fread(Buffer,sizeof(char),(size_t) Count,stream))
      != Count)
  {
    DBUG_PRINT("error",("Read only %d bytes",readbytes));
    if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
    {
      if (ferror(stream))
	my_error(EE_READ, MYF(ME_BELL+ME_WAITTANG),
		 my_filename(fileno(stream)),errno);
      else
      if (MyFlags & (MY_NABP | MY_FNABP))
	my_error(EE_EOFERR, MYF(ME_BELL+ME_WAITTANG),
		 my_filename(fileno(stream)),errno);
    }
    my_errno=errno ? errno : -1;
    if (ferror(stream) || MyFlags & (MY_NABP | MY_FNABP))
      DBUG_RETURN((uint) -1);			/* Return with error */
  }
  if (MyFlags & (MY_NABP | MY_FNABP))
    DBUG_RETURN(0);				/* Read ok */
  DBUG_RETURN(readbytes);
} /* my_fread */


/*
** Write a chunk of bytes to a stream
** Returns (uint) -1 if error as my_write()
** Does retries if interrupted
*/

uint my_fwrite(FILE *stream, const byte *Buffer, uint Count, myf MyFlags)
{
  uint writenbytes=0;
  off_t seekptr;
#if !defined(NO_BACKGROUND) && defined(USE_MY_STREAM)
  uint errors;
#endif
  DBUG_ENTER("my_fwrite");
  DBUG_PRINT("my",("stream: 0x%lx  Buffer: 0x%lx  Count: %u  MyFlags: %d",
		   stream, Buffer, Count, MyFlags));

#if !defined(NO_BACKGROUND) && defined(USE_MY_STREAM)
  errors=0;
#endif
  seekptr=ftell(stream);
  for (;;)
  {
    uint writen;
    if ((writen = (uint) fwrite((char*) Buffer,sizeof(char),
				(size_t) Count, stream)) != Count)
    {
      DBUG_PRINT("error",("Write only %d bytes",writenbytes));
      my_errno=errno;
      if (writen != (uint) -1)
      {
	seekptr+=writen;
	Buffer+=writen;
	writenbytes+=writen;
	Count-=writen;
      }
#ifdef EINTR
      if (errno == EINTR)
      {
	VOID(my_fseek(stream,seekptr,MY_SEEK_SET,MYF(0)));
	continue;
      }
#endif
#if !defined(NO_BACKGROUND) && defined(USE_MY_STREAM)
#ifdef THREAD
      if (my_thread_var->abort)
	MyFlags&= ~ MY_WAIT_IF_FULL;		/* End if aborted by user */
#endif
      if ((errno == ENOSPC || errno == EDQUOT) &&
          (MyFlags & MY_WAIT_IF_FULL))
      {
        if (!(errors++ % MY_WAIT_GIVE_USER_A_MESSAGE))
          my_error(EE_DISK_FULL,MYF(ME_BELL | ME_NOREFRESH),
                   "[stream]",my_errno,MY_WAIT_FOR_USER_TO_FIX_PANIC);
        VOID(sleep(MY_WAIT_FOR_USER_TO_FIX_PANIC));
        VOID(my_fseek(stream,seekptr,MY_SEEK_SET,MYF(0)));
        continue;
      }
#endif
      if (ferror(stream) || (MyFlags & (MY_NABP | MY_FNABP)))
      {
	if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
	{
	  my_error(EE_WRITE, MYF(ME_BELL+ME_WAITTANG),
		   my_filename(fileno(stream)),errno);
	}
	writenbytes=(uint) -1;			/* Return that we got error */
	break;
      }
    }
    if (MyFlags & (MY_NABP | MY_FNABP))
      writenbytes=0;				/* Everything OK */
    else
      writenbytes+=writen;
    break;
  }
  DBUG_RETURN(writenbytes);
} /* my_fwrite */

	/* Seek to position in file */
	/* ARGSUSED */

my_off_t my_fseek(FILE *stream, my_off_t pos, int whence,
		  myf MyFlags __attribute__((unused)))
{
  DBUG_ENTER("my_fseek");
  DBUG_PRINT("my",("stream: 0x%lx  pos: %lu  whence: %d  MyFlags: %d",
		   stream, pos, whence, MyFlags));
  DBUG_RETURN(fseek(stream, (off_t) pos, whence) ?
	      MY_FILEPOS_ERROR : (my_off_t) ftell(stream));
} /* my_seek */


	/* Tell current position of file */
	/* ARGSUSED */

my_off_t my_ftell(FILE *stream, myf MyFlags __attribute__((unused)))
{
  off_t pos;
  DBUG_ENTER("my_ftell");
  DBUG_PRINT("my",("stream: 0x%lx  MyFlags: %d",stream, MyFlags));
  pos=ftell(stream);
  DBUG_PRINT("exit",("ftell: %lu",(ulong) pos));
  DBUG_RETURN((my_off_t) pos);
} /* my_ftell */
