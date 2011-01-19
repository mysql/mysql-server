/* Copyright (c) 2000, 2011, Oracle and/or its affiliates. All rights reserved.

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

/*  File   : strchr.c
    Author : Richard A. O'Keefe.
	     Michael Widenius:	ifdef MC68000
    Updated: 20 April 1984
    Defines: strchr(), index()

    strchr(s, c) returns a pointer to the  first  place  in  s	where  c
    occurs,  or  NullS if c does not occur in s. This function is called
    index in V7 and 4.?bsd systems; while not ideal the name is  clearer
    than  strchr,  so index remains in strings.h as a macro.  NB: strchr
    looks for single characters,  not for sets or strings.   To find the
    NUL character which closes s, use strchr(s, '\0') or strend(s).  The
    parameter 'c' is declared 'int' so it will go in a register; if your
    C compiler is happy with register _char_ change it to that.
*/

#include "strings.h"

#if defined(MC68000) && defined(DS90)

char*	strchr(char *s, pchar c)
{
asm("		movl	4(a7),a0	");
asm("		movl	8(a7),d1	");
asm(".L2:	movb	(a0)+,d0	");
asm("		cmpb	d0,d1		");
asm("		beq	.L1		");
asm("		tstb	d0		");
asm("		bne	.L2		");
asm("		moveq	#0,d0		");
asm("		rts			");
asm(".L1:	movl	a0,d0		");
asm("		subql	#1,d0		");
}
#else

char *strchr(register const char *s, register pchar c)
{
  for (;;)
  {
     if (*s == (char) c) return (char*) s;
     if (!*s++) return NullS;
  }
}

#endif
