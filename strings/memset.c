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

/*  File   : memset.c
    Author : Richard A. O'Keefe.
    Updated: 25 May 1984
    Defines: memset()

    memset(dst, chr, len)
    fills the memory area dst[0..len-1] with len bytes all equal to chr.
    The result is dst.	See also bfill(), which has no return value and
    puts the last two arguments the other way around.

    Note: the VAX assembly code version can only handle 0 <= len < 2^16.
    It is presented for your interest and amusement.
*/

#include "strings.h"

#if	VaxAsm

char *memset(char *dst,int chr, int len)
{
  asm("movc5 $0,*4(ap),8(ap),12(ap),*4(ap)");
  return dst;
}

#else  ~VaxAsm

char *memset(char *dst, register pchar chr, register int len)
{
  register char *d;

  for (d = dst; --len >= 0; *d++ = chr) ;
  return dst;
}

#endif	VaxAsm
