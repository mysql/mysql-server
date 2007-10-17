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
#ifdef HAVE_IEEEFP_H
#include <ieeefp.h>
#endif

#define MAX_DBL_EXP	308
#define MAX_RESULT_FOR_MAX_EXP 1.7976931348623157

const double log_10[] = {
  1e000, 1e001, 1e002, 1e003, 1e004, 1e005, 1e006, 1e007, 1e008, 1e009,
  1e010, 1e011, 1e012, 1e013, 1e014, 1e015, 1e016, 1e017, 1e018, 1e019,
  1e020, 1e021, 1e022, 1e023, 1e024, 1e025, 1e026, 1e027, 1e028, 1e029,
  1e030, 1e031, 1e032, 1e033, 1e034, 1e035, 1e036, 1e037, 1e038, 1e039,
  1e040, 1e041, 1e042, 1e043, 1e044, 1e045, 1e046, 1e047, 1e048, 1e049,
  1e050, 1e051, 1e052, 1e053, 1e054, 1e055, 1e056, 1e057, 1e058, 1e059,
  1e060, 1e061, 1e062, 1e063, 1e064, 1e065, 1e066, 1e067, 1e068, 1e069,
  1e070, 1e071, 1e072, 1e073, 1e074, 1e075, 1e076, 1e077, 1e078, 1e079,
  1e080, 1e081, 1e082, 1e083, 1e084, 1e085, 1e086, 1e087, 1e088, 1e089,
  1e090, 1e091, 1e092, 1e093, 1e094, 1e095, 1e096, 1e097, 1e098, 1e099,
  1e100, 1e101, 1e102, 1e103, 1e104, 1e105, 1e106, 1e107, 1e108, 1e109,
  1e110, 1e111, 1e112, 1e113, 1e114, 1e115, 1e116, 1e117, 1e118, 1e119,
  1e120, 1e121, 1e122, 1e123, 1e124, 1e125, 1e126, 1e127, 1e128, 1e129,
  1e130, 1e131, 1e132, 1e133, 1e134, 1e135, 1e136, 1e137, 1e138, 1e139,
  1e140, 1e141, 1e142, 1e143, 1e144, 1e145, 1e146, 1e147, 1e148, 1e149,
  1e150, 1e151, 1e152, 1e153, 1e154, 1e155, 1e156, 1e157, 1e158, 1e159,
  1e160, 1e161, 1e162, 1e163, 1e164, 1e165, 1e166, 1e167, 1e168, 1e169,
  1e170, 1e171, 1e172, 1e173, 1e174, 1e175, 1e176, 1e177, 1e178, 1e179,
  1e180, 1e181, 1e182, 1e183, 1e184, 1e185, 1e186, 1e187, 1e188, 1e189,
  1e190, 1e191, 1e192, 1e193, 1e194, 1e195, 1e196, 1e197, 1e198, 1e199,
  1e200, 1e201, 1e202, 1e203, 1e204, 1e205, 1e206, 1e207, 1e208, 1e209,
  1e210, 1e211, 1e212, 1e213, 1e214, 1e215, 1e216, 1e217, 1e218, 1e219,
  1e220, 1e221, 1e222, 1e223, 1e224, 1e225, 1e226, 1e227, 1e228, 1e229,
  1e230, 1e231, 1e232, 1e233, 1e234, 1e235, 1e236, 1e237, 1e238, 1e239,
  1e240, 1e241, 1e242, 1e243, 1e244, 1e245, 1e246, 1e247, 1e248, 1e249,
  1e250, 1e251, 1e252, 1e253, 1e254, 1e255, 1e256, 1e257, 1e258, 1e259,
  1e260, 1e261, 1e262, 1e263, 1e264, 1e265, 1e266, 1e267, 1e268, 1e269,
  1e270, 1e271, 1e272, 1e273, 1e274, 1e275, 1e276, 1e277, 1e278, 1e279,
  1e280, 1e281, 1e282, 1e283, 1e284, 1e285, 1e286, 1e287, 1e288, 1e289,
  1e290, 1e291, 1e292, 1e293, 1e294, 1e295, 1e296, 1e297, 1e298, 1e299,
  1e300, 1e301, 1e302, 1e303, 1e304, 1e305, 1e306, 1e307, 1e308
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
  uint negative= 0, neg_exp= 0;
  size_t ndigits, dec_digits= 0;
  int exponent= 0, digits_after_dec_point= 0, tmp_exp, step;
  const char *old_str, *end= *end_ptr, *start_of_number;
  char next_char;
  my_bool overflow=0;
  double scaler= 1.0;

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
    scaler= scaler*10.0;
    if (++str == end)
    {
      next_char= 0;                             /* Found end of string */
      break;
    }
    start_of_number= 0;                         /* Found digit */
  }
  ndigits= (size_t) (str-old_str);

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
      scaler= scaler*10.0;
      if (++str == end)
      {
        next_char= 0;
        break;
      }
    }
    /* If we found just '+.' or '.' then point at first character */
    if (!(dec_digits= (size_t) (str-old_str)) && start_of_number)
      str= start_of_number;                     /* Point at '+' or '.' */
  }
  if ((next_char == 'e' || next_char == 'E') &&
      dec_digits + ndigits != 0 && str < end-1)
  {
    const char *old_str2= str++;

    if ((neg_exp= (*str == '-')) || *str == '+')
      str++;

    if (str == end || !my_isdigit(&my_charset_latin1, *str))
      str= old_str2;
    else
    {
      do
      {
        if (exponent < 9999)                    /* prot. against exp overfl. */
          exponent= exponent*10 + (*str - '0');
        str++;
      } while (str < end && my_isdigit(&my_charset_latin1, *str));
    }
  }
  tmp_exp= (neg_exp ? exponent + digits_after_dec_point :
            exponent - digits_after_dec_point);
  if (tmp_exp)
  {
    int order;
    /*
      Check for underflow/overflow.
      order is such an integer number that f = C * 10 ^ order,
      where f is the resulting floating point number and 1 <= C < 10.
      Here we compute the modulus
    */
    order= exponent + (neg_exp ? -1 : 1) * (ndigits - 1);
    if (order < 0)
      order= -order;
    if (order >= MAX_DBL_EXP && !neg_exp && result)
    {
      double c;
      /* Compute modulus of C (see comment above) */
      c= result / scaler * 10.0;
      if (order > MAX_DBL_EXP || c > MAX_RESULT_FOR_MAX_EXP)
      {
        overflow= 1;
        goto done;
      }
    }

    exponent= tmp_exp;
    if (exponent < 0)
    {
      exponent= -exponent;
      neg_exp= 1;                               /* neg_exp was 0 before */
    }
    step= array_elements(log_10) - 1;
    for (; exponent > step; exponent-= step)
      result= neg_exp ? result / log_10[step] : result * log_10[step];
    result= neg_exp ? result / log_10[exponent] : result * log_10[exponent];
  }

done:
  *end_ptr= (char*) str;                        /* end of number */

  if (overflow || my_isinf(result))
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
