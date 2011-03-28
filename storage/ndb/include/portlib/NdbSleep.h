/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDBSLEEP_H
#define NDBSLEEP_H

#include <ndb_global.h>

static inline
void NdbSleep_MilliSleep(int milliseconds)
{
#ifdef _WIN32
  Sleep(milliseconds);
#elif defined(HAVE_SELECT)
  struct timeval t;
  t.tv_sec =  milliseconds / 1000L;
  t.tv_usec = (milliseconds % 1000L) * 1000L;
  select(0,0,0,0,&t);
#else
#error No suitable function found to implement millisecond sleep.
#endif
}

static inline
void NdbSleep_SecSleep(int seconds)
{
  NdbSleep_MilliSleep(seconds*1000);
}

#endif
