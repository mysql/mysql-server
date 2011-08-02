/* Copyright (c) 2000, 2006, 2007 MySQL AB, 2009 Sun Microsystems, Inc.
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

/* Quicker interface to read & write. Used with my_nosys.h */

#include "mysys_priv.h"
#include "my_nosys.h"


#ifdef _WIN32
extern size_t my_win_read(File Filedes,uchar *Buffer,size_t Count);
#endif

size_t my_quick_read(File Filedes,uchar *Buffer,size_t Count,myf MyFlags)
{
  size_t readbytes;
#ifdef _WIN32
  readbytes= my_win_read(Filedes, Buffer, Count);
#else
  readbytes= read(Filedes, Buffer, Count);
#endif
  if(readbytes != Count)
  {
#ifndef DBUG_OFF
    if ((readbytes == 0 || readbytes == (size_t) -1) && errno == EINTR)
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



size_t my_quick_write(File Filedes, const uchar *Buffer, size_t Count)
{
#ifdef _WIN32
  return my_win_write(Filedes, Buffer, Count);
#else

#ifndef DBUG_OFF
  size_t writtenbytes;
#endif

  if ((
#ifndef DBUG_OFF
       writtenbytes =
#endif
       (size_t) write(Filedes,Buffer,Count)) != Count)
  {
#ifndef DBUG_OFF
    if ((writtenbytes == 0 || writtenbytes == (size_t) -1) && errno == EINTR)
    {  
      DBUG_PRINT("error", ("my_quick_write() was interrupted and returned %d"
                           ".  This function does not retry the write!",
                           (int) writtenbytes));
    }
#endif
    my_errno=errno;
    return (size_t) -1;
  }
  return 0;
#endif
}
