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
