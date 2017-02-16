/* Copyright (c) 2003 TXT DataKonsult Ab
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

#include "strings_def.h"
#include <my_sys.h>            /* Needed for MY_ERRNO_ERANGE */

#define MAX_NEGATIVE_NUMBER	((ulonglong) LL(0x8000000000000000))
#define INIT_CNT  9
#define LFACTOR   ULL(1000000000)
#define LFACTOR1  ULL(10000000000)
#define LFACTOR2  ULL(100000000000)

static unsigned long lfactor[9]=
{
  1L, 10L, 100L, 1000L, 10000L, 100000L, 1000000L, 10000000L, 100000000L
};

/*
  Convert a string to an to unsigned long long integer value
  
  SYNOPSYS
    my_strtoll10()
      nptr     in       pointer to the string to be converted
      endptr   in/out   pointer to the end of the string/
                        pointer to the stop character
      error    out      returned error code
 
  DESCRIPTION
    This function takes the decimal representation of integer number
    from string nptr and converts it to an signed or unsigned
    long long integer value.
    Space characters and tab are ignored.
    A sign character might precede the digit characters. The number
    may have any number of pre-zero digits.

    The function stops reading the string nptr at the first character
    that is not a decimal digit. If endptr is not NULL then the function
    will not read characters after *endptr.
 
  RETURN VALUES
    Value of string as a signed/unsigned longlong integer

    if no error and endptr != NULL, it will be set to point at the character
    after the number

    The error parameter contains information how things went:
    -1		Number was an ok negative number
    0	 	ok
    ERANGE	If the the value of the converted number exceeded the
	        maximum negative/unsigned long long integer.
		In this case the return value is ~0 if value was
		positive and LONGLONG_MIN if value was negative.
    EDOM	If the string didn't contain any digits. In this case
    		the return value is 0.

    If endptr is not NULL the function will store the end pointer to
    the stop character here.
*/


longlong my_strtoll10(const char *nptr, char **endptr, int *error)
{
  const char *s, *end, *start, *n_end, *true_end;
  char *dummy;
  uchar c;
  unsigned long i, j, k;
  ulonglong li;
  int negative;
  ulong cutoff, cutoff2, cutoff3;

  s= nptr;
  /* If fixed length string */
  if (endptr)
  {
    end= *endptr;
    while (s != end && (*s == ' ' || *s == '\t'))
      s++;
    if (s == end)
      goto no_conv;
  }
  else
  {
    endptr= &dummy;				/* Easier end test */
    while (*s == ' ' || *s == '\t')
      s++;
    if (!*s)
      goto no_conv;
    /* This number must be big to guard against a lot of pre-zeros */
    end= s+65535;				/* Can't be longer than this */
  }

  /* Check for a sign.	*/
  negative= 0;
  if (*s == '-')
  {
    *error= -1;					/* Mark as negative number */
    negative= 1;
    if (++s == end)
      goto no_conv;
    cutoff=  MAX_NEGATIVE_NUMBER / LFACTOR2;
    cutoff2= (MAX_NEGATIVE_NUMBER % LFACTOR2) / 100;
    cutoff3=  MAX_NEGATIVE_NUMBER % 100;
  }
  else
  {
    *error= 0;
    if (*s == '+')
    {
      if (++s == end)
	goto no_conv;
    }
    cutoff=  ULONGLONG_MAX / LFACTOR2;
    cutoff2= ULONGLONG_MAX % LFACTOR2 / 100;
    cutoff3=  ULONGLONG_MAX % 100;
  }

  /* Handle case where we have a lot of pre-zero */
  if (*s == '0')
  {
    i= 0;
    do
    {
      if (++s == end)
	goto end_i;				/* Return 0 */
    }
    while (*s == '0');
    n_end= s+ INIT_CNT;
  }
  else
  {
    /* Read first digit to check that it's a valid number */
    if ((c= (*s-'0')) > 9)
      goto no_conv;
    i= c;
    n_end= ++s+ INIT_CNT-1;
  }

  /* Handle first 9 digits and store them in i */
  if (n_end > end)
    n_end= end;
  for (; s != n_end ; s++)
  {
    if ((c= (*s-'0')) > 9)
      goto end_i;
    i= i*10+c;
  }
  if (s == end)
    goto end_i;

  /* Handle next 9 digits and store them in j */
  j= 0;
  start= s;				/* Used to know how much to shift i */
  n_end= true_end= s + INIT_CNT;
  if (n_end > end)
    n_end= end;
  do
  {
    if ((c= (*s-'0')) > 9)
      goto end_i_and_j;
    j= j*10+c;
  } while (++s != n_end);
  if (s == end)
  {
    if (s != true_end)
      goto end_i_and_j;
    goto end3;
  }
  if ((c= (*s-'0')) > 9)
    goto end3;

  /* Handle the next 1 or 2 digits and store them in k */
  k=c;
  if (++s == end || (c= (*s-'0')) > 9)
    goto end4;
  k= k*10+c;
  *endptr= (char*) ++s;

  /* number string should have ended here */
  if (s != end && (c= (*s-'0')) <= 9)
    goto overflow;

  /* Check that we didn't get an overflow with the last digit */
  if (i > cutoff || (i == cutoff && (j > cutoff2 || (j == cutoff2 &&
                                     k > cutoff3))))
    goto overflow;
  li=i*LFACTOR2+ (ulonglong) j*100 + k;
  return (longlong) li;

overflow:					/* *endptr is set here */
  *error= MY_ERRNO_ERANGE;
  return negative ? LONGLONG_MIN : (longlong) ULONGLONG_MAX;

end_i:
  *endptr= (char*) s;
  return (negative ? ((longlong) -(long) i) : (longlong) i);

end_i_and_j:
  li= (ulonglong) i * lfactor[(uint) (s-start)] + j;
  *endptr= (char*) s;
  return (negative ? -((longlong) li) : (longlong) li);

end3:
  li=(ulonglong) i*LFACTOR+ (ulonglong) j;
  *endptr= (char*) s;
  return (negative ? -((longlong) li) : (longlong) li);

end4:
  li=(ulonglong) i*LFACTOR1+ (ulonglong) j * 10 + k;
  *endptr= (char*) s;
  if (negative)
  {
   if (li > MAX_NEGATIVE_NUMBER)
     goto overflow;
   return -((longlong) li);
  }
  return (longlong) li;

no_conv:
  /* There was no number to convert.  */
  *error= MY_ERRNO_EDOM;
  *endptr= (char *) nptr;
  return 0;
}
