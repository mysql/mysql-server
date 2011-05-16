/* Copyright (c) 2000 TXT DataKonsult Ab & Monty Program Ab
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

/* memcmp(lhs, rhs, len)
   compares the two memory areas lhs[0..len-1]	??  rhs[0..len-1].   It
   returns  an integer less than, equal to, or greater than 0 according
   as lhs[-] is lexicographically less than, equal to, or greater  than
   rhs[-].  Note  that this is not at all the same as bcmp, which tells
   you *where* the difference is but not what.

   Note:  suppose we have int x, y;  then memcmp(&x, &y, sizeof x) need
   not bear any relation to x-y.  This is because byte order is machine
   dependent, and also, some machines have integer representations that
   are shorter than a machine word and two equal  integers  might  have
   different  values  in the spare bits.  On a ones complement machine,
   -0 == 0, but the bit patterns are different.
*/

#include "strings.h"

#if !defined(HAVE_MEMCPY)

int memcmp(lhs, rhs, len)
     register char *lhs, *rhs;
     register int len;
{
  while (--len >= 0)
    if (*lhs++ != *rhs++) return (uchar) lhs[-1] - (uchar) rhs[-1];
  return 0;
}

#endif
