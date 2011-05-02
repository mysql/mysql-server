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

/*  File   : strlen.c
    Author : Richard A. O'Keefe. / Monty
	     Michael Widenius;	ifdef MC68000
    Updated: 1986-11-30
    Defines: strlen()

    strlen(s) returns the number of characters in s, that is, the number
    of non-NUL characters found before the closing NULEosCh.  Note: some
    non-standard C compilers for 32-bit machines take int to be 16 bits,
    either put up with short strings or change int  to	long  throughout
    this package.  Better yet, BOYCOTT such shoddy compilers.
    Beware: the asm version works only if strlen(s) < 65536.
*/

#include "strings.h"

#if VaxAsm

size_t strlen(char *s)
{
 asm("locc  $0,$65535,*4(ap)");
 asm("subl3 r0,$65535,r0");
}

#else
#if defined(MC68000) && defined(DS90)

size_t strlen(char *s)
{
asm("		movl	4(a7),a0	");
asm("		movl	a0,a1		");
asm(".L4:	tstb	(a0)+		");
asm("		jne	.L4		");
asm("		movl	a0,d0		");
asm("		subl	a1,d0		");
asm("		subql	#1,d0		");
}
#else

size_t strlen(register char *s)
{
  register char *startpos;

  startpos = s;
  while (*s++);
  return ((size_t) (s-startpos-1));
}

#endif
#endif
