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

/* Wait a given number of microseconds */

#include "mysys_priv.h"
#include <m_string.h>

void my_sleep(ulong m_seconds)
{
#ifdef __NETWARE__
  delay(m_seconds/1000+1);
#elif defined(__WIN__)
  Sleep(m_seconds/1000+1);      /* Sleep() has millisecond arg */
#elif defined(HAVE_SELECT)
  struct timeval t;
  t.tv_sec=  m_seconds / 1000000L;
  t.tv_usec= m_seconds % 1000000L;
  select(0,0,0,0,&t); /* sleep */
#else
  uint sec=    (uint) (m_seconds / 1000000L);
  ulong start= (ulong) time((time_t*) 0);
  while ((ulong) time((time_t*) 0) < start+sec);
#endif
}
