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
  @file mysys/my_fstream.cc
*/

#include "my_config.h"

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysys_err.h"
#if defined(_WIN32)
#include "mysys/mysys_priv.h"
#endif


#ifdef HAVE_FSEEKO
#undef ftell
#undef fseek
#define ftell(A) ftello(A)
#define fseek(A,B,C) fseeko((A),(B),(C))
#endif

/*
  Read a chunk of bytes from a FILE

  SYNOPSIS
   my_fread()
   stream	File descriptor
   Buffer	Buffer to read to
   Count	Number of bytes to read
   MyFlags	Flags on what to do on error

  RETURN
    (size_t) -1 Error
    #		Number of bytes read
 */

size_t my_fread(FILE *stream, uchar *Buffer, size_t Count, myf MyFlags)
{
  size_t readbytes;
  DBUG_ENTER("my_fread");
  DBUG_PRINT("my",("stream: %p  Buffer: %p  Count: %u  MyFlags: %d",
		   stream, Buffer, (uint) Count, MyFlags));

  if ((readbytes= fread(Buffer, sizeof(char), Count, stream)) != Count)
  {
    DBUG_PRINT("error",("Read only %d bytes", (int) readbytes));
    if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
    {
      if (ferror(stream))
      {
        char errbuf[MYSYS_STRERROR_SIZE];
        my_error(EE_READ, MYF(0),
                 my_filename(my_fileno(stream)),
                 errno, my_strerror(errbuf, sizeof(errbuf), errno));
      }
      else
      if (MyFlags & (MY_NABP | MY_FNABP))
      {
        char errbuf[MYSYS_STRERROR_SIZE];
        my_error(EE_EOFERR, MYF(0),
                 my_filename(my_fileno(stream)), errno,
                 my_strerror(errbuf, sizeof(errbuf), errno));
      }
    }
    set_my_errno(errno ? errno : -1);
    if (ferror(stream) || MyFlags & (MY_NABP | MY_FNABP))
      DBUG_RETURN((size_t) -1);			/* Return with error */
  }
  if (MyFlags & (MY_NABP | MY_FNABP))
    DBUG_RETURN(0);				/* Read ok */
  DBUG_RETURN(readbytes);
} /* my_fread */


/*
  Write a chunk of bytes to a stream

   my_fwrite()
   stream	File descriptor
   Buffer	Buffer to write from
   Count	Number of bytes to write
   MyFlags	Flags on what to do on error

  RETURN
    (size_t) -1 Error
    #		Number of bytes written
*/

size_t my_fwrite(FILE *stream, const uchar *Buffer, size_t Count, myf MyFlags)
{
  size_t writtenbytes =0;
  my_off_t seekptr;

  DBUG_ENTER("my_fwrite");
  DBUG_PRINT("my",("stream: %p  Buffer: %p  Count: %u  MyFlags: %d",
		   stream, Buffer, (uint) Count, MyFlags));
  DBUG_EXECUTE_IF("simulate_fwrite_error",  DBUG_RETURN(-1););

  seekptr= ftell(stream);
  for (;;)
  {
    size_t written;
    if ((written = (size_t) fwrite((char*) Buffer,sizeof(char),
                                   Count, stream)) != Count)
    {
      DBUG_PRINT("error",("Write only %d bytes", (int) writtenbytes));
      set_my_errno(errno);
      if (written != (size_t) -1)
      {
	seekptr+=written;
	Buffer+=written;
	writtenbytes+=written;
	Count-=written;
      }
      if (errno == EINTR)
      {
	(void) my_fseek(stream,seekptr,MY_SEEK_SET);
	continue;
      }
      if (ferror(stream) || (MyFlags & (MY_NABP | MY_FNABP)))
      {
        if (MyFlags & (MY_WME | MY_FAE | MY_FNABP))
        {
          char errbuf[MYSYS_STRERROR_SIZE];
          my_error(EE_WRITE, MYF(0),
                   my_filename(my_fileno(stream)),
                   errno, my_strerror(errbuf, sizeof(errbuf), errno));
        }
        writtenbytes= (size_t) -1;        /* Return that we got error */
        break;
      }
    }
    if (MyFlags & (MY_NABP | MY_FNABP))
      writtenbytes= 0;				/* Everything OK */
    else
      writtenbytes+= written;
    break;
  }
  DBUG_RETURN(writtenbytes);
} /* my_fwrite */


/* Seek to position in file */

my_off_t my_fseek(FILE *stream, my_off_t pos, int whence)
{
  DBUG_ENTER("my_fseek");
  DBUG_PRINT("my",("stream: %p  pos: %lu  whence: %d" ,
                   stream, (long) pos, whence));
  DBUG_RETURN(fseek(stream, (off_t) pos, whence) ?
	      MY_FILEPOS_ERROR : (my_off_t) ftell(stream));
} /* my_seek */


/* Tell current position of file */

my_off_t my_ftell(FILE *stream)
{
  off_t pos;
  DBUG_ENTER("my_ftell");
  DBUG_PRINT("my",("stream: %p", stream));
  pos=ftell(stream);
  DBUG_PRINT("exit",("ftell: %lu",(ulong) pos));
  DBUG_RETURN((my_off_t) pos);
} /* my_ftell */


/* Get a File corresponding to the stream*/
int my_fileno(FILE *f)
{
#ifdef _WIN32
  return my_win_fileno(f);
#else
 return fileno(f);
#endif
}
