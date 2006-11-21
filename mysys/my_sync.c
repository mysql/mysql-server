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
    If file system supports its, only file data is synced, not inode data.

    MY_IGNORE_BADFD is useful when fd is "volatile" - not protected by a
    mutex. In this case by the time of fsync(), fd may be already closed by
    another thread, or even reassigned to a different file. With this flag -
    MY_IGNORE_BADFD - such a situation will not be considered an error.
    (which is correct behaviour, if we know that the other thread synced the
    file before closing)

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
#if defined(F_FULLFSYNC)
    /* Recent Mac OS X versions insist this call is safer than fsync() */
    if (!(res= fcntl(fd, F_FULLFSYNC, 0)))
      break; /* ok */
    /* Some fs don't support F_FULLFSYNC and fail above, fallback: */
#endif
#if defined(HAVE_FDATASYNC)
    res= fdatasync(fd);
#elif defined(HAVE_FSYNC)
    res= fsync(fd);
#elif defined(__WIN__)
    res= _commit(fd);
#else
#warning Cannot find a way to sync a file, durability in danger
    res= 0;					/* No sync (strange OS) */
#endif
  } while (res == -1 && errno == EINTR);

  if (res)
  {
    int er= errno;
    if (!(my_errno= er))
      my_errno= -1;                             /* Unknown error */
    if ((my_flags & MY_IGNORE_BADFD) &&
        (er == EBADF || er == EINVAL || er == EROFS))
      res= 0;
    else if (my_flags & MY_WME)
      my_error(EE_SYNC, MYF(ME_BELL+ME_WAITTANG), my_filename(fd), my_errno);
  }
  DBUG_RETURN(res);
} /* my_sync */


/*
  Force directory information to disk. Only Linux is known to need this to
  make sure a file creation/deletion/renaming in(from,to) this directory
  durable.

  SYNOPSIS
    my_sync_dir()
    dir_name             the name of the directory
    my_flags             unused

  RETURN
    nothing (the sync may fail sometimes).
*/
void my_sync_dir(const char *dir_name, myf my_flags __attribute__((unused)))
{
#ifdef TARGET_OS_LINUX
  DBUG_ENTER("my_sync_dir");
  DBUG_PRINT("my",("Dir: '%s'  my_flags: %d", dir_name, my_flags));
  File dir_fd;
  int error= 0;
  /*
    Syncing a dir does not work on all filesystems (e.g. tmpfs->EINVAL) :
    ignore errors. But print them to the debug log.
  */
  if (((dir_fd= my_open(dir_name, O_RDONLY, MYF(0))) >= 0))
  {
    if (my_sync(dir_fd, MYF(0)))
    {
      error= errno;
      DBUG_PRINT("info",("my_sync failed errno: %d", error));
    }
    my_close(dir_fd, MYF(0));
  }
  else
  {
    error= errno;
    DBUG_PRINT("info",("my_open failed errno: %d", error));
  }
  DBUG_VOID_RETURN;
#endif
}


/*
  Force directory information to disk. Only Linux is known to need this to
  make sure a file creation/deletion/renaming in(from,to) this directory
  durable.

  SYNOPSIS
    my_sync_dir_by_file()
    file_name            the name of a file in the directory
    my_flags             unused

  RETURN
    nothing (the sync may fail sometimes).
*/
void my_sync_dir_by_file(const char *file_name,
                         myf my_flags __attribute__((unused)))
{
#ifdef TARGET_OS_LINUX
  char dir_name[FN_REFLEN];
  dirname_part(dir_name, file_name);
  return my_sync_dir(dir_name, my_flags);
#endif
}

