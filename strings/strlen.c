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

size_s strlen(char *s)
{
 asm("locc  $0,$65535,*4(ap)");
 asm("subl3 r0,$65535,r0");
}

#else
#if defined(MC68000) && defined(DS90)

size_s strlen(char *s)
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

size_s strlen(register char *s)
{
  register char *startpos;

  startpos = s;
  while (*s++);
  return ((size_s) (s-startpos-1));
}

#endif
#endif
