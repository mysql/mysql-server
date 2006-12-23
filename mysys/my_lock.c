/* Copyright (C) 2000-2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

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
#undef MY_HOW_OFTEN_TO_ALARM
#define MY_HOW_OFTEN_TO_ALARM ((int) my_time_to_wait_for_lock)
#ifdef NO_ALARM_LOOP
#undef NO_ALARM_LOOP
#endif
#include <my_alarm.h>
#ifdef __WIN__
#include <sys/locking.h>
#endif
#ifdef __NETWARE__
#include <nks/fsio.h>
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
#ifdef __NETWARE__
  int nxErrno;
#endif
  DBUG_ENTER("my_lock");
  DBUG_PRINT("my",("Fd: %d  Op: %d  start: %ld  Length: %ld  MyFlags: %d",
		   fd,locktype,(long) start,(long) length,MyFlags));
#ifdef VMS
  DBUG_RETURN(0);
#else
  if (my_disable_locking)
    DBUG_RETURN(0);

#if defined(__NETWARE__)
  {
    NXSOffset_t nxLength = length;
    unsigned long nxLockFlags = 0;

    if (length == F_TO_EOF)
    {
      /* EOF is interpreted as a very large length. */
      nxLength = 0x7FFFFFFFFFFFFFFF;
    }

    if (locktype == F_UNLCK)
    {
      /* The lock flags are currently ignored by NKS. */
      if (!(nxErrno= NXFileRangeUnlock(fd, 0L, start, nxLength)))
        DBUG_RETURN(0);
    }
    else
    {
      if (locktype == F_RDLCK)
      {
        /* A read lock is mapped to a shared lock. */
        nxLockFlags = NX_RANGE_LOCK_SHARED;
      }
      else
      {
        /* A write lock is mapped to an exclusive lock. */
        nxLockFlags = NX_RANGE_LOCK_EXCL;
      }

      if (MyFlags & MY_DONT_WAIT)
      {
        /* Don't block on the lock. */
        nxLockFlags |= NX_RANGE_LOCK_TRYLOCK;
      }

      if (!(nxErrno= NXFileRangeLock(fd, nxLockFlags, start, nxLength)))
        DBUG_RETURN(0);
    }
  }
#elif defined(HAVE_LOCKING)
  /* Windows */
  {
    my_bool error= FALSE;
    pthread_mutex_lock(&my_file_info[fd].mutex);
    if (MyFlags & MY_SEEK_NOT_DONE) 
    {
      if( my_seek(fd,start,MY_SEEK_SET,MYF(MyFlags & ~MY_SEEK_NOT_DONE))
           == MY_FILEPOS_ERROR )
      {
        /*
          If my_seek fails my_errno will already contain an error code;
          just unlock and return error code.
         */
        DBUG_PRINT("error",("my_errno: %d (%d)",my_errno,errno));
        pthread_mutex_unlock(&my_file_info[fd].mutex);
        DBUG_RETURN(-1);
      }
    }
    error= locking(fd,locktype,(ulong) length) && errno != EINVAL;
    pthread_mutex_unlock(&my_file_info[fd].mutex);
    if (!error)
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

#ifdef __NETWARE__
  my_errno = nxErrno;
#else
	/* We got an error. We don't want EACCES errors */
  my_errno=(errno == EACCES) ? EAGAIN : errno ? errno : -1;
#endif
  if (MyFlags & MY_WME)
  {
    if (locktype == F_UNLCK)
      my_error(EE_CANTUNLOCK,MYF(ME_BELL+ME_WAITTANG),my_errno);
    else
      my_error(EE_CANTLOCK,MYF(ME_BELL+ME_WAITTANG),my_errno);
  }
  DBUG_PRINT("error",("my_errno: %d (%d)",my_errno,errno));
  DBUG_RETURN(-1);
#endif	/* ! VMS */
} /* my_lock */
