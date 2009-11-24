/* Copyright (C) 2008 MySQL AB

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

#include <atomic.h>

#define	MY_ATOMIC_MODE	"solaris-atomic"

/*
 * This is defined to indicate we fully define the my_atomic_* (inline)
 * functions here, so there is no need to "make" them in my_atomic.h
 * using make_atomic_* and make_atomic_*_body.
 */
#define	MY_ATOMICS_MADE

STATIC_INLINE int
my_atomic_cas8(int8 volatile *a, int8 *cmp, int8 set)
{
	int ret;
	int8 sav;
	sav = (int8) atomic_cas_8((volatile uint8_t *)a, (uint8_t)*cmp,
		(uint8_t)set);
	if (! (ret = (sav == *cmp)))
		*cmp = sav;
	return ret;
}

STATIC_INLINE int
my_atomic_cas16(int16 volatile *a, int16 *cmp, int16 set)
{
	int ret;
	int16 sav;
	sav = (int16) atomic_cas_16((volatile uint16_t *)a, (uint16_t)*cmp,
		(uint16_t)set);
	if (! (ret = (sav == *cmp)))
		*cmp = sav;
	return ret;
}

STATIC_INLINE int
my_atomic_cas32(int32 volatile *a, int32 *cmp, int32 set)
{
	int ret;
	int32 sav;
	sav = (int32) atomic_cas_32((volatile uint32_t *)a, (uint32_t)*cmp,
		(uint32_t)set);
	if (! (ret = (sav == *cmp)))
		*cmp = sav;
	return ret;
}

STATIC_INLINE int
my_atomic_casptr(void * volatile *a, void **cmp, void *set)
{
	int ret;
	void *sav;
	sav = atomic_cas_ptr(a, *cmp, set);
	if (! (ret = (sav == *cmp)))
		*cmp = sav;
	return ret;
}

/* ------------------------------------------------------------------------ */

STATIC_INLINE int8
my_atomic_add8(int8 volatile *a, int8 v)
{
	int8 nv;
	nv = atomic_add_8_nv((volatile uint8_t *)a, v);
	return (nv - v);
}

STATIC_INLINE int16
my_atomic_add16(int16 volatile *a, int16 v)
{
	int16 nv;
	nv = atomic_add_16_nv((volatile uint16_t *)a, v);
	return (nv - v);
}

STATIC_INLINE int32
my_atomic_add32(int32 volatile *a, int32 v)
{
	int32 nv;
	nv = atomic_add_32_nv((volatile uint32_t *)a, v);
	return (nv - v);
}

/* ------------------------------------------------------------------------ */

#ifdef MY_ATOMIC_MODE_DUMMY

STATIC_INLINE int8
my_atomic_load8(int8 volatile *a)	{ return (*a); }

STATIC_INLINE int16
my_atomic_load16(int16 volatile *a)	{ return (*a); }

STATIC_INLINE int32
my_atomic_load32(int32 volatile *a)	{ return (*a); }

STATIC_INLINE void *
my_atomic_loadptr(void * volatile *a)	{ return (*a); }

/* ------------------------------------------------------------------------ */

STATIC_INLINE void
my_atomic_store8(int8 volatile *a, int8 v)	{ *a = v; }

STATIC_INLINE void
my_atomic_store16(int16 volatile *a, int16 v)	{ *a = v; }

STATIC_INLINE void
my_atomic_store32(int32 volatile *a, int32 v)	{ *a = v; }

STATIC_INLINE void
my_atomic_storeptr(void * volatile *a, void *v)	{ *a = v; }

/* ------------------------------------------------------------------------ */

#else /* MY_ATOMIC_MODE_DUMMY */

STATIC_INLINE int8
my_atomic_load8(int8 volatile *a)
{
	return ((int8) atomic_or_8_nv((volatile uint8_t *)a, 0));
}

STATIC_INLINE int16
my_atomic_load16(int16 volatile *a)
{
	return ((int16) atomic_or_16_nv((volatile uint16_t *)a, 0));
}

STATIC_INLINE int32
my_atomic_load32(int32 volatile *a)
{
	return ((int32) atomic_or_32_nv((volatile uint32_t *)a, 0));
}

STATIC_INLINE void *
my_atomic_loadptr(void * volatile *a)
{
	return ((void *) atomic_or_ulong_nv((volatile ulong_t *)a, 0));
}

/* ------------------------------------------------------------------------ */

STATIC_INLINE void
my_atomic_store8(int8 volatile *a, int8 v)
{
	(void) atomic_swap_8((volatile uint8_t *)a, (uint8_t)v);
}

STATIC_INLINE void
my_atomic_store16(int16 volatile *a, int16 v)
{
	(void) atomic_swap_16((volatile uint16_t *)a, (uint16_t)v);
}

STATIC_INLINE void
my_atomic_store32(int32 volatile *a, int32 v)
{
	(void) atomic_swap_32((volatile uint32_t *)a, (uint32_t)v);
}

STATIC_INLINE void
my_atomic_storeptr(void * volatile *a, void *v)
{
	(void) atomic_swap_ptr(a, v);
}

#endif

/* ------------------------------------------------------------------------ */

STATIC_INLINE int8
my_atomic_fas8(int8 volatile *a, int8 v)
{
	return ((int8) atomic_swap_8((volatile uint8_t *)a, (uint8_t)v));
}

STATIC_INLINE int16
my_atomic_fas16(int16 volatile *a, int16 v)
{
	return ((int16) atomic_swap_16((volatile uint16_t *)a, (uint16_t)v));
}

STATIC_INLINE int32
my_atomic_fas32(int32 volatile *a, int32 v)
{
	return ((int32) atomic_swap_32((volatile uint32_t *)a, (uint32_t)v));
}

STATIC_INLINE void *
my_atomic_fasptr(void * volatile *a, void *v)
{
	return (atomic_swap_ptr(a, v));
}
