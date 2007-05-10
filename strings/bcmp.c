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

/*
  bcmp(s1, s2, len) returns 0 if the "len" bytes starting at "s1" are
  identical to the "len" bytes starting at "s2", non-zero if they are
  different.
  Now only used with purify because purify gives wrong warnings when
  comparing a shorter string with bcmp.
*/

#include <my_global.h>
#include "m_string.h"

#ifdef HAVE_purify
#undef bcmp
#undef HAVE_BCMP
#endif

#if !defined(bcmp) && !defined(HAVE_BCMP)

#if defined(MC68000) && defined(DS90)

int bcmp(s1,s2, len)
const char *s1;
const char *s2;
uint len;					/* 0 <= len <= 65535 */
{
  asm("		movl	12(a7),d0	");
  asm("		subqw	#1,d0		");
  asm("		blt	.L5		");
  asm("		movl	4(a7),a1	");
  asm("		movl	8(a7),a0	");
  asm(".L4:	cmpmb	(a0)+,(a1)+	");
  asm("		dbne	d0,.L4		");
  asm(".L5:	addqw	#1,d0		");
}

#else

#ifndef HAVE_purify
size_t bcmp(register const uchar *s1,register const uchar *s2,
            register size_t len)
#else
size_t my_bcmp(register const uchar *s1,register const uchar *s2,
               register size_t len)
#endif
{
  while (len-- != 0 && *s1++ == *s2++) ;
  return len+1;
}

#endif
#endif /* BSD_FUNCS */
