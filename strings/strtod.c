/*
  An alternative implementation of "strtod()" that is both
  simplier, and thread-safe.

  Original code from mit-threads as bundled with MySQL 3.23

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

#include "my_base.h"			/* Includes errno.h + EOVERFLOW */
#include "m_ctype.h"

#define MAX_DBL_EXP	308
#define MAX_RESULT_FOR_MAX_EXP 1.79769313486232
static double scaler10[] = {
  1.0, 1e10, 1e20, 1e30, 1e40, 1e50, 1e60, 1e70, 1e80, 1e90
};
static double scaler1[] = {
  1.0, 10.0, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9
};


/*
  Convert string to double (string doesn't have to be null terminated)

  SYNOPSIS
    my_strtod()
    str		String to convert
    end_ptr	Pointer to pointer that points to end of string
		Will be updated to point to end of double.
    error	Will contain error number in case of error (else 0)

  RETURN
    value of str as double
*/

double my_strtod(const char *str, char **end_ptr, int *error)
{
  double result= 0.0;
  uint negative= 0, ndigits, dec_digits= 0, neg_exp= 0;
  int exp= 0, digits_after_dec_point= 0;
  const char *old_str, *end= *end_ptr, *start_of_number;
  char next_char;
  my_bool overflow=0;

  *error= 0;
  if (str >= end)
    goto done;

  while (my_isspace(&my_charset_latin1, *str))
  {
    if (++str == end)
      goto done;
  }

  start_of_number= str;
  if ((negative= (*str == '-')) || *str=='+')
  {
    if (++str == end)
      goto done;                                /* Could be changed to error */
  }

  /* Skip pre-zero for easier calculation of overflows */
  while (*str == '0')
  {
    if (++str == end)
      goto done;
    start_of_number= 0;                         /* Found digit */
  }

  old_str= str;
  while ((next_char= *str) >= '0' && next_char <= '9')
  {
    result= result*10.0 + (next_char - '0');
    if (++str == end)
    {
      next_char= 0;                             /* Found end of string */
      break;
    }
    start_of_number= 0;                         /* Found digit */
  }
  ndigits= (uint) (str-old_str);

  if (next_char == '.' && str < end-1)
  {
    /*
      Continue to add numbers after decimal point to the result, as if there
      was no decimal point. We will later (in the exponent handling) shift
      the number down with the required number of fractions.  We do it this
      way to be able to get maximum precision for numbers like 123.45E+02,
      which are normal for some ODBC applications.
    */
    old_str= ++str;
    while (my_isdigit(&my_charset_latin1, (next_char= *str)))
    {
      result= result*10.0 + (next_char - '0');
      digits_after_dec_point++;
      if (++str == end)
      {
        next_char= 0;
        break;
      }
    }
    /* If we found just '+.' or '.' then point at first character */
    if (!(dec_digits= (uint) (str-old_str)) && start_of_number)
      str= start_of_number;                     /* Point at '+' or '.' */
  }
  if ((next_char == 'e' || next_char == 'E') &&
      dec_digits + ndigits != 0 && str < end-1)
  {
    const char *old_str= str++;

    if ((neg_exp= (*str == '-')) || *str == '+')
      str++;

    if (str == end || !my_isdigit(&my_charset_latin1, *str))
      str= old_str;
    else
    {
      do
      {
        if (exp < 9999)                         /* prot. against exp overfl. */
          exp= exp*10 + (*str - '0');
        str++;
      } while (str < end && my_isdigit(&my_charset_latin1, *str));
    }
  }
  if ((exp= (neg_exp ? exp + digits_after_dec_point :
             exp - digits_after_dec_point)))
  {
    double scaler;
    if (exp < 0)
    {
      exp= -exp;
      neg_exp= 1;                               /* neg_exp was 0 before */
    }
    if (exp + ndigits >= MAX_DBL_EXP + 1 && result)
    {
      /*
        This is not 100 % as we actually will give an owerflow for
        17E307 but not for 1.7E308 but lets cut some corners to make life
        simpler
      */
      if (exp + ndigits > MAX_DBL_EXP + 1 ||
          result >= MAX_RESULT_FOR_MAX_EXP)
      {
        if (neg_exp)
          result= 0.0;
        else
          overflow= 1;
        goto done;
      }
    }
    scaler= 1.0;
    while (exp >= 100)
    {
      scaler*= 1.0e100;
      exp-= 100;
    }
    scaler*= scaler10[exp/10]*scaler1[exp%10];
    if (neg_exp)
      result/= scaler;
    else
      result*= scaler;
  }

done:
  *end_ptr= (char*) str;                        /* end of number */

  if (overflow || isinf(result))
  {
    result= DBL_MAX;
    *error= EOVERFLOW;
  }

  return negative ? -result : result;
}

double my_atof(const char *nptr)
{
  int error;
  const char *end= nptr+65535;                  /* Should be enough */
  return (my_strtod(nptr, (char**) &end, &error));
}
