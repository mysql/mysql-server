/*  File   : strcmp.c
    Author : Richard A. O'Keefe.
    Updated: 10 April 1984
    Defines: strcmp()

    strcmp(s, t) returns > 0, = 0,  or < 0  when s > t, s = t,	or s < t
    according  to  the	ordinary  lexicographical  order.   To	test for
    equality, the macro streql(s,t) is clearer than  !strcmp(s,t).  Note
    that  if the string contains characters outside the range 0..127 the
    result is machine-dependent; PDP-11s and  VAXen  use  signed  bytes,
    some other machines use unsigned bytes.
*/

#include "strings.h"

int strcmp(register const char *s, register const char *t)
{
  while (*s == *t++) if (!*s++) return 0;
  return s[0]-t[-1];
}
