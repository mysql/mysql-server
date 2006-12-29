/* Copyright (C) 2003 MySQL AB

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


#include <ndb_global.h>
#include <my_sys.h>
#include <NdbSleep.h>

int
NdbSleep_MilliSleep(int milliseconds){
  my_sleep(milliseconds*1000);
  return 0;
#if 0
  int result = 0;
  struct timespec sleeptime;
  sleeptime.tv_sec = milliseconds / 1000;
  sleeptime.tv_nsec = (milliseconds - (sleeptime.tv_sec * 1000)) * 1000000;
  result = nanosleep(&sleeptime, NULL);
  return result;
#endif
}

int
NdbSleep_SecSleep(int seconds){
  int result = 0;
  result = sleep(seconds);
  return result;
}


