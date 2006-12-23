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

/* Utilities with are missing on some systems */

#include "mysys_priv.h"
#ifdef __ZTC__
#include <dos.h>
#endif

#ifdef __ZTC__

	/* On ZORTECH C we don't have a getpid() call */

int getpid(void)
{
  return (int) _psp;
}

#ifndef M_IC80386

	/* Define halloc and hfree in as in MSC */

void *	__CDECL halloc(long count,size_t length)
{
   return (void*) MK_FP(dos_alloc((uint) ((count*length+15) >> 4)),0);
}

void __CDECL hfree(void *ptr)
{
   dos_free(FP_SEG(ptr));
}

#endif /* M_IC80386 */
#endif /* __ZTC__ */
