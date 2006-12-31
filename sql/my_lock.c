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

#if defined(__NETWARE__)
#include "../mysys/my_lock.c"
#else

#undef MAP_TO_USE_RAID			/* Avoid RAID mappings */
#include <my_global.h>
#include <my_sys.h>
#include <mysys_err.h>
#include <my_pthread.h>
#include <thr_alarm.h>
#include <errno.h>

	/* Lock a part of a file */

int my_lock(File fd,int locktype,my_off_t start,my_off_t length,myf MyFlags)
{
  thr_alarm_t alarmed;
  ALARM	alarm_buff;
  uint wait_for_alarm;
  struct flock m_lock;
  DBUG_ENTER("my_lock");
  DBUG_PRINT("my",("Fd: %d  Op: %d  start: %ld  Length: %ld  MyFlags: %d",
		   fd,locktype,(ulong) start,(ulong) length,MyFlags));
  if (my_disable_locking)
    DBUG_RETURN(0); /* purecov: inspected */
  m_lock.l_type=(short) locktype;
  m_lock.l_whence=0L;
  m_lock.l_start=(long) start;
  m_lock.l_len=(long) length;
  wait_for_alarm=(MyFlags & MY_DONT_WAIT ? MY_HOW_OFTEN_TO_ALARM :
		  (uint) 12*60*60);
  if (fcntl(fd,F_SETLK,&m_lock) != -1)	/* Check if we can lock */
    DBUG_RETURN(0);			/* Ok, file locked */
  DBUG_PRINT("info",("Was locked, trying with alarm"));
  if (!thr_alarm(&alarmed,wait_for_alarm,&alarm_buff))
  {
    int value;
    while ((value=fcntl(fd,F_SETLKW,&m_lock)) && !thr_got_alarm(&alarmed) &&
	   errno == EINTR) ;
    thr_end_alarm(&alarmed);
    if (value != -1)
      DBUG_RETURN(0);
  }
  else
  {
    errno=EINTR;
  }
  if (errno == EINTR || errno == EACCES)
    my_errno=EAGAIN;			/* Easier to check for this */
  else
    my_errno=errno;

  if (MyFlags & MY_WME)
  {
    if (locktype == F_UNLCK)
      my_error(EE_CANTUNLOCK,MYF(ME_BELL+ME_WAITTANG),errno);
    else
      my_error(EE_CANTLOCK,MYF(ME_BELL+ME_WAITTANG),errno);
  }
  DBUG_PRINT("error",("errno: %d",errno));
  DBUG_RETURN(-1);
}
#endif
