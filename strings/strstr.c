/*  File   : strstr.c
    Author : Monty
    Updated: 1986.11.24
    Defines: strstr()

    strstr(src, pat) looks for an instance of pat in src.  pat is not a
    regex(3) pattern, it is a literal string which must be matched exactly.
    The result is a pointer to the first character of the located instance,
    or NullS if pat does not occur in src.

*/

#include <my_global.h>
#include "m_string.h"

#ifndef HAVE_STRSTR

char *strstr(register const char *str,const char *search)
{
 register char *i,*j;
 register char first= *search;

skipp:
  while (*str != '\0') {
    if (*str++ == first) {
      i=(char*) str; j=(char*) search+1;
      while (*j)
	if (*i++ != *j++) goto skipp;
      return ((char*) str-1);
    }
  }
  return ((char*) 0);
} /* strstr */

#endif
