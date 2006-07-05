/* Copyright (C) 2006 MySQL AB

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

/*
  XXX 64-bit atomic operations can be implemented using
  cmpxchg8b, if necessary
*/

// Would it be better to use intrinsics ?
// (InterlockedCompareExchange, InterlockedCompareExchange16
// InterlockedExchangeAdd, InterlockedExchange)

#ifndef _atomic_h_cleanup_
#define _atomic_h_cleanup_ "atomic/x86-msvc.h"

#define MY_ATOMIC_MODE "msvc-x86" LOCK

#define make_atomic_add_body(S)				\
  _asm {						\
    _asm mov   reg_ ## S, v				\
    _asm LOCK  xadd *a, reg_ ## S			\
    _asm movzx v, reg_ ## S				\
  }
#define make_atomic_cas_body(S)				\
  _asm {						\
    _asm mov    areg_ ## S, *cmp			\
    _asm mov    reg2_ ## S, set				\
    _asm LOCK cmpxchg *a, reg2_ ## S			\
    _asm mov    *cmp, areg_ ## S			\
    _asm setz   al					\
    _asm movzx  ret, al					\
  }
#define make_atomic_swap_body(S)			\
  _asm {						\
    _asm mov    reg_ ## S, v				\
    _asm xchg   *a, reg_ ## S				\
    _asm mov    v, reg_ ## S				\
  }

#ifdef MY_ATOMIC_MODE_DUMMY
#define make_atomic_load_body(S)        ret=*a
#define make_atomic_store_body(S)       *a=v
#else
/*
  Actually 32-bit reads/writes are always atomic on x86
  But we add LOCK here anyway to force memory barriers
*/
#define make_atomic_load_body(S)			\
  _asm {						\
    _asm mov    areg_ ## S, 0				\
    _asm mov    reg2_ ## S, areg_ ## S			\
    _asm LOCK cmpxchg *a, reg2_ ## S			\
    _asm mov    ret, areg_ ## S				\
  }
#define make_atomic_store_body(S)			\
  _asm {						\
    _asm mov    reg_ ## S, v				\
    _asm xchg   *a, reg_ ## S				\
  }
#endif

#define reg_8           al
#define reg_16          ax
#define reg_32          eax
#define areg_8          al
#define areg_16         ax
#define areg_32         eax
#define reg2_8          bl
#define reg2_16         bx
#define reg2_32         ebx

#else /* cleanup */

#undef reg_8
#undef reg_16
#undef reg_32
#undef areg_8
#undef areg_16
#undef areg_32
#undef reg2_8
#undef reg2_16
#undef reg2_32
#endif

