/* Copyright (C) 2000 MySQL AB

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

#include "my_global.h"

#if !defined(_MSC_VER) && !defined(__BORLANDC__) && !defined(__NETWARE__)
#include "mysys_priv.h"
#include <sys/times.h>
#endif

long my_clock(void)
{
#if !defined(__WIN__) && !defined(__NETWARE__)
  struct tms tmsbuf;
  (void) times(&tmsbuf);
  return (tmsbuf.tms_utime + tmsbuf.tms_stime);
#else
  return clock();
#endif
}
