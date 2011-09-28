/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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
#include <errno.h>
#undef MY_HOW_OFTEN_TO_ALARM
#define MY_HOW_OFTEN_TO_ALARM ((int) my_time_to_wait_for_lock)
#ifdef NO_ALARM_LOOP
#undef NO_ALARM_LOOP
#endif
#include <my_alarm.h>

#ifdef _WIN32
#define WIN_LOCK_INFINITE -1
#define WIN_LOCK_SLEEP_MILLIS 100

static int win_lock(File fd, int locktype, my_off_t start, my_off_t length,
                int timeout_sec)
{
  LARGE_INTEGER liOffset,liLength;
  DWORD dwFlags;
  OVERLAPPED ov= {0};
  HANDLE hFile= (HANDLE)my_get_osfhandle(fd);
  DWORD  lastError= 0;
  int i;
  int timeout_millis= timeout_sec * 1000;

  DBUG_ENTER("win_lock");

  liOffset.QuadPart= start;
  liLength.QuadPart= length;

  ov.Offset=      liOffset.LowPart;
  ov.OffsetHigh=  liOffset.HighPart;

  if (locktype == F_UNLCK)
  {
    if (UnlockFileEx(hFile, 0, liLength.LowPart, liLength.HighPart, &ov))
      DBUG_RETURN(0);
    /*
      For compatibility with fcntl implementation, ignore error,
      if region was not locked
    */
    if (GetLastError() == ERROR_NOT_LOCKED)
    {
      SetLastError(0);
      DBUG_RETURN(0);
    }
    goto error;
  }
  else if (locktype == F_RDLCK)
    /* read lock is mapped to a shared lock. */
    dwFlags= 0;
  else
    /* write lock is mapped to an exclusive lock. */
    dwFlags= LOCKFILE_EXCLUSIVE_LOCK;

  /*
    Drop old lock first to avoid double locking.
    During analyze of Bug#38133 (Myisamlog test fails on Windows)
    I met the situation that the program myisamlog locked the file
    exclusively, then additionally shared, then did one unlock, and
    then blocked on an attempt to lock it exclusively again.
    Unlocking before every lock fixed the problem.
    Note that this introduces a race condition. When the application
    wants to convert an exclusive lock into a shared one, it will now
    first unlock the file and then lock it shared. A waiting exclusive
    lock could step in here. For reasons described in Bug#38133 and
    Bug#41124 (Server hangs on Windows with --external-locking after
    INSERT...SELECT) and in the review thread at
    http://lists.mysql.com/commits/60721 it seems to be the better
    option than not to unlock here.
    If one day someone notices a way how to do file lock type changes
    on Windows without unlocking before taking the new lock, please
    change this code accordingly to fix the race condition.
  */
  if (!UnlockFileEx(hFile, 0, liLength.LowPart, liLength.HighPart, &ov) &&
      (GetLastError() != ERROR_NOT_LOCKED))
    goto error;

  if (timeout_sec == WIN_LOCK_INFINITE)
  {
    if (LockFileEx(hFile, dwFlags, 0, liLength.LowPart, liLength.HighPart, &ov))
      DBUG_RETURN(0);
    goto error;
  }
  
  dwFlags|= LOCKFILE_FAIL_IMMEDIATELY;
  timeout_millis= timeout_sec * 1000;
  /* Try lock in a loop, until the lock is acquired or timeout happens */
  for(i= 0; ;i+= WIN_LOCK_SLEEP_MILLIS)
  {
    if (LockFileEx(hFile, dwFlags, 0, liLength.LowPart, liLength.HighPart, &ov))
     DBUG_RETURN(0);

    if (GetLastError() != ERROR_LOCK_VIOLATION)
      goto error;

    if (i >= timeout_millis)
      break;
    Sleep(WIN_LOCK_SLEEP_MILLIS);
  }

  /* timeout */
  errno= EAGAIN;
  DBUG_RETURN(-1);

error:
   my_osmaperr(GetLastError());
   DBUG_RETURN(-1);
}
#endif



/* 
  Lock a part of a file 

  RETURN VALUE
    0   Success
    -1  An error has occured and 'my_errno' is set
        to indicate the actual error code.
*/

int my_lock(File fd, int locktype, my_off_t start, my_off_t length,
	    myf MyFlags)
{
#ifdef HAVE_FCNTL
  int value;
  ALARM_VARIABLES;
#endif

  DBUG_ENTER("my_lock");
  DBUG_PRINT("my",("fd: %d  Op: %d  start: %ld  Length: %ld  MyFlags: %d",
		   fd,locktype,(long) start,(long) length,MyFlags));
  if (my_disable_locking)
    DBUG_RETURN(0);

#if defined(_WIN32)
  {
    int timeout_sec;
    if (MyFlags & MY_DONT_WAIT)
      timeout_sec= 0;
    else
      timeout_sec= WIN_LOCK_INFINITE;

    if (win_lock(fd, locktype, start, length, timeout_sec) == 0)
      DBUG_RETURN(0);
  }
#else
#if defined(HAVE_FCNTL)
  {
    struct flock lock;

    lock.l_type=   (short) locktype;
    lock.l_whence= SEEK_SET;
    lock.l_start=  (off_t) start;
    lock.l_len=    (off_t) length;

    if (MyFlags & MY_DONT_WAIT)
    {
      if (fcntl(fd,F_SETLK,&lock) != -1)	/* Check if we can lock */
	DBUG_RETURN(0);			/* Ok, file locked */
      DBUG_PRINT("info",("Was locked, trying with alarm"));
      ALARM_INIT;
      while ((value=fcntl(fd,F_SETLKW,&lock)) && ! ALARM_TEST &&
	     errno == EINTR)
      {			/* Setup again so we don`t miss it */
	ALARM_REINIT;
      }
      ALARM_END;
      if (value != -1)
	DBUG_RETURN(0);
      if (errno == EINTR)
	errno=EAGAIN;
    }
    else if (fcntl(fd,F_SETLKW,&lock) != -1) /* Wait until a lock */
      DBUG_RETURN(0);
  }
#else
  if (MyFlags & MY_SEEK_NOT_DONE)
  {
    if (my_seek(fd,start,MY_SEEK_SET,MYF(MyFlags & ~MY_SEEK_NOT_DONE))
        == MY_FILEPOS_ERROR)
    {
      /*
        If an error has occured in my_seek then we will already
        have an error code in my_errno; Just return error code.
      */
      DBUG_RETURN(-1);
    }
  }
  if (lockf(fd,locktype,length) != -1)
    DBUG_RETURN(0);
#endif /* HAVE_FCNTL */
#endif /* HAVE_LOCKING */

  /* We got an error. We don't want EACCES errors */
  my_errno=(errno == EACCES) ? EAGAIN : errno ? errno : -1;

  if (MyFlags & MY_WME)
  {
    if (locktype == F_UNLCK)
      my_error(EE_CANTUNLOCK,MYF(ME_BELL+ME_WAITTANG),my_errno);
    else
      my_error(EE_CANTLOCK,MYF(ME_BELL+ME_WAITTANG),my_errno);
  }
  DBUG_PRINT("error",("my_errno: %d (%d)",my_errno,errno));
  DBUG_RETURN(-1);
} /* my_lock */
