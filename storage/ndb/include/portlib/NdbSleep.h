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

#ifndef NDBSLEEP_H
#define NDBSLEEP_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <ndb_global.h>
#include <my_sys.h>

static inline void NdbSleep_MilliSleep(int milliseconds)
{
  my_sleep(ulong(milliseconds)*1000UL);
}
static inline void NdbSleep_SecSleep(int seconds)
{
  NdbSleep_MilliSleep(seconds*1000);
}

#ifdef	__cplusplus
}
#endif


#endif
