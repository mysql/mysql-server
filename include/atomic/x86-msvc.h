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

#define make_atomic_add_body(REG)				\
  _asm {							\
    _asm mov   REG, v						\
    _asm LOCK  xadd a->val, REG					\
    _asm movzx v, REG						\
  }
#define make_atomic_cas_body(AREG,REG2)				\
  _asm {							\
    _asm mov    AREG, *cmp					\
    _asm mov    REG2, set					\
    _asm LOCK cmpxchg a->val, REG2				\
    _asm mov    *cmp, AREG					\
    _asm setz   al						\
    _asm movzx  ret, al						\
  }
#define make_atomic_swap_body(REG)				\
  _asm {							\
    _asm mov    REG, v						\
    _asm xchg   a->val, REG					\
    _asm mov    v, REG						\
  }

#ifdef MY_ATOMIC_MODE_DUMMY
#define make_atomic_load_body(AREG,REG)   ret=a->val
#define make_atomic_store_body(REG)	  a->val=v
#else
/*
  Actually 32-bit reads/writes are always atomic on x86
  But we add LOCK here anyway to force memory barriers
*/
#define make_atomic_load_body(AREG,REG2)			\
  _asm {							\
    _asm mov    AREG, 0						\
    _asm mov    REG2, AREG					\
    _asm LOCK cmpxchg a->val, REG2				\
    _asm mov    ret, AREG					\
  }
#define make_atomic_store_body(REG)				\
  _asm {							\
    _asm mov    REG, v						\
    _asm xchg   a->val, REG					\
  }
#endif

#define make_atomic_add_body8	 make_atomic_add_body(al)
#define make_atomic_add_body16   make_atomic_add_body(ax)
#define make_atomic_add_body32   make_atomic_add_body(eax)
#define make_atomic_cas_body8    make_atomic_cas_body(al, bl)
#define make_atomic_cas_body16   make_atomic_cas_body(ax, bx)
#define make_atomic_cas_body32   make_atomic_cas_body(eax, ebx)
#define make_atomic_load_body8    make_atomic_load_body(al, bl)
#define make_atomic_load_body16   make_atomic_load_body(ax, bx)
#define make_atomic_load_body32   make_atomic_load_body(eax, ebx)
#define make_atomic_store_body8  make_atomic_store_body(al)
#define make_atomic_store_body16 make_atomic_store_body(ax)
#define make_atomic_store_body32 make_atomic_store_body(eax)
#define make_atomic_swap_body8   make_atomic_swap_body(al)
#define make_atomic_swap_body16  make_atomic_swap_body(ax)
#define make_atomic_swap_body32  make_atomic_swap_body(eax)

