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

	/* Seek to position in file */
	/*ARGSUSED*/

my_off_t my_seek(File fd, my_off_t pos, int whence, myf MyFlags)
{
  reg1 os_off_t newpos;
  DBUG_ENTER("my_seek");
  DBUG_PRINT("my",("Fd: %d  Hpos: %lu  Pos: %lu  Whence: %d  MyFlags: %d",
		   fd, ((ulonglong) pos) >> 32, (ulong) pos, whence, MyFlags));
  newpos=lseek(fd, pos, whence);
  if (newpos == (os_off_t) -1)
  {
    my_errno=errno;
    DBUG_PRINT("error",("lseek: %lu, errno: %d",newpos,errno));
    DBUG_RETURN(MY_FILEPOS_ERROR);
  }
  DBUG_RETURN((my_off_t) newpos);
} /* my_seek */


	/* Tell current position of file */
	/* ARGSUSED */

my_off_t my_tell(File fd, myf MyFlags)
{
  os_off_t pos;
  DBUG_ENTER("my_tell");
  DBUG_PRINT("my",("Fd: %d  MyFlags: %d",fd, MyFlags));
#ifdef HAVE_TELL
  pos=tell(fd);
#else
  pos=lseek(fd, 0L, MY_SEEK_CUR);
#endif
  if (pos == (os_off_t) -1)
    my_errno=errno;
  DBUG_PRINT("exit",("pos: %lu",pos));
  DBUG_RETURN((my_off_t) pos);
} /* my_tell */
