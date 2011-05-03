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

#define IFACTOR 4

void dcopy(char *chardest, char *charsrc, int size)
{
  register int *src, *dest, intcount ;
  int startcharcpy, intoffset, numints2cpy, i ;

  numints2cpy = size >> 2 ;
  startcharcpy = numints2cpy << 2 ;
  intcount = numints2cpy & ~(IFACTOR-1) ;
  intoffset = numints2cpy - intcount ;

  src = (int *)(((int) charsrc) + intcount*sizeof(int*)) ;
  dest = (int *)(((int) chardest) + intcount*sizeof(int*)) ;

  /* copy the ints */
  switch(intoffset)
    do
    {
    case 0: dest[3] = src[3] ;
    case 3: dest[2] = src[2] ;
    case 2: dest[1] = src[1] ;
    case 1: dest[0] = src[0] ;
      intcount -= IFACTOR ;
      dest -= IFACTOR ;
      src -= IFACTOR ;
    } while (intcount >= 0) ;

  /* copy the chars left over by the int copy at the end */
  for(i=startcharcpy ; i<size ; i++)
    chardest[i] = charsrc[i] ;
}
