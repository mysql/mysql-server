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

/* Do udiv and urem if machine dosn't have it */

#include "strings_def.h"
#include <math.h>

unsigned long udiv(long unsigned int a, long unsigned int b)
{
  if (a < INT_MAX32 && b < INT_MAX32)
    return (unsigned long) ((long) a / (long) b);
  if (!(b & 1))
    return (unsigned long) ((long) (a >> 1) / (long) (b >> 1));

  return (unsigned long) floor(((double) a / (double) b));
}

unsigned long urem(long unsigned int a, long unsigned int b)
{
  if (a < INT_MAX32 && b < INT_MAX32)
    return (unsigned long) ((long) a % (long) b);
  return a-udiv(a,b)*b;
}
