/* Copyright (C) 2003 MySQL AB

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
  Sync data in file to disk

  SYNOPSIS
    my_sync()
    fd			File descritor to sync
    my_flags		Flags (now only MY_WME is supported)

  NOTE
    If file system supports its, only file data is synced, not inode date

  RETURN
    0 ok
    -1 error
*/

int my_sync(File fd, myf my_flags)
{
  int res;
  DBUG_ENTER("my_sync");
  DBUG_PRINT("my",("Fd: %d  my_flags: %d", fd, my_flags));

  do
  {
#if defined(HAVE_FDATASYNC)
    res= fdatasync(fd);
#elif defined(HAVE_FSYNC)
    res= fsync(fd);
#elif defined(__WIN__)
    res= _commit(fd);
#else
    res= 0;					/* No sync (strange OS) */
#endif
  } while (res == -1 && errno == EINTR);

  if (res)
  {
    if (!(my_errno= errno))
      my_errno= -1;				/* Unknown error */
    if (my_flags & MY_WME)
      my_error(EE_SYNC, MYF(ME_BELL+ME_WAITTANG), my_filename(fd), my_errno);
  }
  DBUG_RETURN(res);
} /* my_read */
