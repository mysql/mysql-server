/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef PREFETCH_H
#define PREFETCH_H

#ifdef NDB_FORTE6
#include <sun_prefetch.h>
#endif

#ifdef USE_PREFETCH
#define PREFETCH(addr) prefetch(addr)
#else
#define PREFETCH(addr)
#endif

#ifdef USE_PREFETCH
#define WRITEHINT(addr) writehint(addr)
#else
#define WRITEHINT(addr)
#endif

#include "PortDefs.h"

#ifdef NDB_FORTE6
#pragma optimize("", off)
#endif
inline void prefetch(void* p)
{
#ifdef NDB_ALPHA
   __asm(" ldl r31,0(a0);", p);
#endif /* NDB_ALPHA */
#ifdef NDB_FORTE6
  sparc_prefetch_read_once(p);
#else 
  (void)p;
#endif
}

inline void writehint(void* p)
{
#ifdef NDB_ALPHA
   __asm(" wh64 (a0);", p);
#endif /* NDB_ALPHA */
#ifdef NDB_FORTE6
  sparc_prefetch_write_once(p);
#else
  (void)p;
#endif
}
#ifdef NDB_FORTE6
#pragma optimize("", on)
#endif

#endif

