/*
  An alternative implementation of "strtod()" that is both
  simplier, and thread-safe.

  From mit-threads as bundled with MySQL 3.23

  SQL:2003 specifies a number as

<signed numeric literal> ::= [ <sign> ] <unsigned numeric literal>

<unsigned numeric literal> ::=
                    <exact numeric literal>
                  | <approximate numeric literal>

<exact numeric literal> ::=
                    <unsigned integer> [ <period> [ <unsigned integer> ] ]
                  | <period> <unsigned integer>

<approximate numeric literal> ::= <mantissa> E <exponent>

<mantissa> ::= <exact numeric literal>

<exponent> ::= <signed integer>

  So do we.

 */

#include "my_base.h"				/* Includes errno.h */
#include "m_ctype.h"

static double scaler10[] = {
  1.0, 1e10, 1e20, 1e30, 1e40, 1e50, 1e60, 1e70, 1e80, 1e90
};
static double scaler1[] = {
  1.0, 10.0, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9
};


double my_strtod(const char *str, char **end)
{
  double result= 0.0;
  int negative, ndigits;
  const char *old_str;
  my_bool overflow=0;

  while (my_isspace(&my_charset_latin1, *str))
    str++;

  if ((negative= (*str == '-')) || *str=='+')
    str++;

  old_str= str;
  while (my_isdigit (&my_charset_latin1, *str))
  {
    result= result*10.0 + (*str - '0');
    str++;
  }
  ndigits= str-old_str;

  if (*str == '.')
  {
    double p10=10;
    str++;
    old_str= str;
    while (my_isdigit (&my_charset_latin1, *str))
    {
      result+= (*str++ - '0')/p10;
      p10*=10;
    }
    ndigits+= str-old_str;
    if (!ndigits) str--;
  }
  if (ndigits && (*str=='e' || *str=='E'))
  {
    int exp= 0;
    int neg= 0;
    const char *old_str= str++;

    if ((neg= (*str == '-')) || *str == '+')
      str++;

    if (!my_isdigit (&my_charset_latin1, *str))
      str= old_str;
    else
    {
      double scaler= 1.0;
      while (my_isdigit (&my_charset_latin1, *str))
      {
        if (exp < 9999) /* protection against exp overflow */
          exp= exp*10 + *str - '0';
        str++;
      }
      if (exp >= 1000)
      {
	if (neg)
	  result= 0.0;
	else
          overflow= 1;
        goto done;
      }
      while (exp >= 100)
      {
        scaler*= 1.0e100;
        exp-= 100;
      }
      scaler*= scaler10[exp/10]*scaler1[exp%10];
      if (neg)
        result/= scaler;
      else
        result*= scaler;
    }
  }

done:
  if (end)
    *end = (char *)str;

  if (overflow || isinf(result))
  {
    result= DBL_MAX;
    errno= EOVERFLOW;
  }

  return negative ? -result : result;
}

double my_atof(const char *nptr)
{
  return (my_strtod(nptr, 0));
}

