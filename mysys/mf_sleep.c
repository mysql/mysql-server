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

/* Wait a given time (On systems that dont have sleep !!; MSDOS) */

#include "mysys_priv.h"
#include <m_string.h>

#ifdef _MSC_VER

void sleep(sec)
int sec;
{
  ulong start;
  DBUG_ENTER("sleep");

  start=(ulong) time((time_t*) 0);
  while ((ulong) time((time_t*) 0) < start+sec);
  DBUG_VOID_RETURN;
} /* sleep */

#endif /* MSDOS */
