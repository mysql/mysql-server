/*
  An alternative implementation of "strtod()" that is both
  simplier, and thread-safe.

  From mit-threads as bundled with MySQL 3.22

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

#include "my_base.h"
#include "m_ctype.h"

static double scaler10[] = {
  1.0, 1e10, 1e20, 1e30, 1e40, 1e50, 1e60, 1e70, 1e80, 1e90
};
static double scaler1[] = {
  1.0, 10.0, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9
};

/* let's use a static array for not to accumulate the error */
static double pastpoint[] = {
  1e-1,  1e-2,  1e-3,  1e-4,  1e-5,  1e-6,  1e-7,  1e-8,  1e-9,
  1e-10, 1e-11, 1e-12, 1e-13, 1e-14, 1e-15, 1e-16, 1e-17, 1e-18,  1e-19,
  1e-20, 1e-21, 1e-22, 1e-23, 1e-24, 1e-25, 1e-26, 1e-27, 1e-28,  1e-29,
  1e-30, 1e-31, 1e-32, 1e-33, 1e-34, 1e-35, 1e-36, 1e-37, 1e-38,  1e-39,
  1e-40, 1e-41, 1e-42, 1e-43, 1e-44, 1e-45, 1e-46, 1e-47, 1e-48,  1e-49,
  1e-50, 1e-51, 1e-52, 1e-53, 1e-54, 1e-55, 1e-56, 1e-57, 1e-58,  1e-59,
};

double my_strtod(const char *str, char **end)
{
  double result= 0.0;
  int negative, ndigits;
  const char *old_str;

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
    int n= 0;
    str++;
    old_str= str;
    while (my_isdigit (&my_charset_latin1, *str))
    {
      if (n < sizeof(pastpoint)/sizeof(pastpoint[0]))
      {
        result+= pastpoint[n] * (*str - '0');
        n++;
      }
      str++;
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
        exp= exp*10 + *str - '0';
        str++;
      }
      if (exp >= 1000)
      {
        if (neg)
          result= 0.0;
        else
          result= DBL_MAX;
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

  if (isinf(result))
    result=DBL_MAX;

  return negative ? -result : result;
}

double my_atof(const char *nptr)
{
  return (my_strtod(nptr, 0));
}

