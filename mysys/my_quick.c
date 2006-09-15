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

/* Quicker interface to read & write. Used with my_nosys.h */

#include "mysys_priv.h"
#include "my_nosys.h"


uint my_quick_read(File Filedes,byte *Buffer,uint Count,myf MyFlags)
{
  uint readbytes;

  if ((readbytes = (uint) read(Filedes, Buffer, Count)) != Count)
  {
#ifndef DBUG_OFF
    if ((readbytes == 0 || (int) readbytes == -1) && errno == EINTR)
    {  
      DBUG_PRINT("error", ("my_quick_read() was interrupted and returned %d"
                           ".  This function does not retry the read!",
                           (int) readbytes));
    }
#endif
    my_errno=errno;
    return readbytes;
  }
  return (MyFlags & (MY_NABP | MY_FNABP)) ? 0 : readbytes;
}


uint my_quick_write(File Filedes,const byte *Buffer,uint Count)
{
#ifndef DBUG_OFF
  uint writtenbytes;
#endif

  if ((
#ifndef DBUG_OFF
       writtenbytes =
#endif
       (uint) write(Filedes,Buffer,Count)) != Count)
  {
#ifndef DBUG_OFF
    if ((writtenbytes == 0 || (int) writtenbytes == -1) && errno == EINTR)
    {  
      DBUG_PRINT("error", ("my_quick_write() was interrupted and returned %d"
                           ".  This function does not retry the write!",
                           (int) writtenbytes));
    }
#endif
    my_errno=errno;
    return (uint) -1;
  }
  return 0;
}
