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
#ifdef __EMX__
#define INCL_BASE
#define INCL_NOPMAPI
#include <os2emx.h>
#endif

#ifndef __EMX__
#ifdef HAVE_FCNTL
static struct flock lock;		/* Must be static for sun-sparc */
#endif
#endif

	/* Lock a part of a file */

int my_lock(File fd, int locktype, my_off_t start, my_off_t length,
	    myf MyFlags)
{
#ifdef __EMX__
  FILELOCK LockArea = {0,0}, UnlockArea = {0,0};
  APIRET rc = 0;
  fpos_t oldpos;
  int lockflags = 0;
#endif
#ifdef HAVE_FCNTL
  int value;
  ALARM_VARIABLES;
#endif
  DBUG_ENTER("my_lock");
  DBUG_PRINT("my",("Fd: %d  Op: %d  start: %ld  Length: %ld  MyFlags: %d",
		   fd,locktype,(long) start,(long) length,MyFlags));
#ifdef VMS
  DBUG_RETURN(0);
#else
  if (my_disable_locking)
    DBUG_RETURN(0);
#if defined(__EMX__)
  if (locktype == F_UNLCK) {
    UnlockArea.lOffset = start;
    if (length)
      UnlockArea.lRange = length;
    else
      UnlockArea.lRange = 0x7FFFFFFFL;
  } else
  if (locktype == F_RDLCK || locktype == F_WRLCK) {
    if (locktype == F_RDLCK) lockflags |= 1;
    LockArea.lOffset = start;
    if (length)
      LockArea.lRange = length;
    else
      LockArea.lRange = 0x7FFFFFFFL;
  } else {
    my_errno = EINVAL;
    DBUG_RETURN(-1);
  }
  if (!LockArea.lRange && !UnlockArea.lRange)
    DBUG_RETURN(0);
  if (MyFlags & MY_DONT_WAIT) {
    if (!(rc = DosSetFileLocks(fd,&UnlockArea,&LockArea,0,lockflags)))
      DBUG_RETURN(0);		/* Lock was OK */
    if (rc == 175 && locktype == F_RDLCK) {
      lockflags &= ~1;
      rc = DosSetFileLocks(fd,&UnlockArea,&LockArea,0,lockflags);
    }
    if (rc == 33) {  /* Lock Violation */
      DBUG_PRINT("info",("Was locked, trying with timeout"));
      rc = DosSetFileLocks(fd,&UnlockArea,&LockArea,MY_HOW_OFTEN_TO_ALARM * 1000,lockflags);
    }
    if (!rc) DBUG_RETURN(0);
    if (rc == 33) errno = EAGAIN;
    else {
      errno = EINVAL;
      printf("Error: DosSetFileLocks() == %d\n",rc);
    }
  } else {
    while (rc = DosSetFileLocks(fd,&UnlockArea,&LockArea,
	MY_HOW_OFTEN_TO_ALARM * 1000,lockflags) && (rc == 33 || rc == 175)) {
      printf(".");
      if (rc == 175) lockflags &= ~1;
    }
    if (!rc) DBUG_RETURN(0);
    errno = EINVAL;
    printf("Error: DosSetFileLocks() == %d\n",rc);
  }
#elif defined(HAVE_LOCKING)
  /* Windows */
  {
    my_bool error;
    pthread_mutex_lock(&my_file_info[fd].mutex);
    if (MyFlags & MY_SEEK_NOT_DONE)
      VOID(my_seek(fd,start,MY_SEEK_SET,MYF(MyFlags & ~MY_SEEK_NOT_DONE)));
    error= locking(fd,locktype,(ulong) length) && errno != EINVAL;
    pthread_mutex_unlock(&my_file_info[fd].mutex);
    if (!error)
      DBUG_RETURN(0);
  }
#else
#if defined(HAVE_FCNTL)
  lock.l_type= (short) locktype;
  lock.l_whence=0L;
  lock.l_start=(long) start;
  lock.l_len=(long) length;
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
#else
  if (MyFlags & MY_SEEK_NOT_DONE)
    VOID(my_seek(fd,start,MY_SEEK_SET,MYF(MyFlags & ~MY_SEEK_NOT_DONE)));
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
#endif	/* ! VMS */
} /* my_lock */
