/* Copyright Richard A. O'Keefe.
   Copyright (c) 2000 TXT DataKonsult Ab & Monty Program Ab
   Copyright (c) 2009-2011, Monty Program Ab

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

   1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.

   THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND ANY
   EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
   OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.
*/

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
