/*  File   : bzero.c
    Author : Richard A. O'Keefe.
	     Michael Widenius;	ifdef MC68000
    Updated: 23 April 1984
    Defines: bzero()

    bzero(dst, len) moves "len" 0 bytes to "dst".
    Thus to clear a disc buffer to 0s do bzero(buffer, BUFSIZ).

    Note: the "b" routines are there to exploit certain VAX order codes,
    The asm code is presented for your interest and amusement.
*/

#ifndef BSD_FUNCS
#include "strings.h"

#ifdef bzero
#undef bzero					/* remove macro */
#endif

#if VaxAsm

static void _bzero64 _A((char *dst,int	len));

void bzero(dst, len)
char *dst;
uint len;
{
  while ((int) len >= 64*K)
  {
    _bzero64(dst, 64*K-1);
    dst += 64*K-1;
    len -= 64*K-1;
  }
  _bzero64(dst, len);
}

_bzero64(dst, len)
char *dst;
int  len;
{
  asm("movc5 $0,*4(ap),$0,8(ap),*4(ap)");
}

#else

#if defined(MC68000) && defined(DS90)

void bzero(dst, len)
char *dst;
uint len;
{
  bfill(dst,len,0);				/* This is very optimized ! */
} /* bzero */

#else

void bzero(dst, len)
register char *dst;
register uint len;
{
  while (len-- != 0) *dst++ = 0;
} /* bzero */

#endif
#endif
#endif /* BSD_FUNCS */
