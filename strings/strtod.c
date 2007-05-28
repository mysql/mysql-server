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
  1e300, 1e301, 1e302, 1e303, 1e304, 1e305, 1e306, 1e307, 1e308, 1e309
};

const double log_01[] = {
  1e-000,1e-001,1e-002,1e-003,1e-004,1e-005,1e-006,1e-007,1e-008,1e-009,
  1e-010,1e-011,1e-012,1e-013,1e-014,1e-015,1e-016,1e-017,1e-018,1e-019,
  1e-020,1e-021,1e-022,1e-023,1e-024,1e-025,1e-026,1e-027,1e-028,1e-029,
  1e-030,1e-031,1e-032,1e-033,1e-034,1e-035,1e-036,1e-037,1e-038,1e-039,
  1e-040,1e-041,1e-042,1e-043,1e-044,1e-045,1e-046,1e-047,1e-048,1e-049,
  1e-050,1e-051,1e-052,1e-053,1e-054,1e-055,1e-056,1e-057,1e-058,1e-059,
  1e-060,1e-061,1e-062,1e-063,1e-064,1e-065,1e-066,1e-067,1e-068,1e-069,
  1e-070,1e-071,1e-072,1e-073,1e-074,1e-075,1e-076,1e-077,1e-078,1e-079,
  1e-080,1e-081,1e-082,1e-083,1e-084,1e-085,1e-086,1e-087,1e-088,1e-089,
  1e-090,1e-091,1e-092,1e-093,1e-094,1e-095,1e-096,1e-097,1e-098,1e-099,
  1e-100,1e-101,1e-102,1e-103,1e-104,1e-105,1e-106,1e-107,1e-108,1e-109,
  1e-110,1e-111,1e-112,1e-113,1e-114,1e-115,1e-116,1e-117,1e-118,1e-119,
  1e-120,1e-121,1e-122,1e-123,1e-124,1e-125,1e-126,1e-127,1e-128,1e-129,
  1e-130,1e-131,1e-132,1e-133,1e-134,1e-135,1e-136,1e-137,1e-138,1e-139,
  1e-140,1e-141,1e-142,1e-143,1e-144,1e-145,1e-146,1e-147,1e-148,1e-149,
  1e-150,1e-151,1e-152,1e-153,1e-154,1e-155,1e-156,1e-157,1e-158,1e-159,
  1e-160,1e-161,1e-162,1e-163,1e-164,1e-165,1e-166,1e-167,1e-168,1e-169,
  1e-170,1e-171,1e-172,1e-173,1e-174,1e-175,1e-176,1e-177,1e-178,1e-179,
  1e-180,1e-181,1e-182,1e-183,1e-184,1e-185,1e-186,1e-187,1e-188,1e-189,
  1e-190,1e-191,1e-192,1e-193,1e-194,1e-195,1e-196,1e-197,1e-198,1e-199,
  1e-200,1e-201,1e-202,1e-203,1e-204,1e-205,1e-206,1e-207,1e-208,1e-209,
  1e-210,1e-211,1e-212,1e-213,1e-214,1e-215,1e-216,1e-217,1e-218,1e-219,
  1e-220,1e-221,1e-222,1e-223,1e-224,1e-225,1e-226,1e-227,1e-228,1e-229,
  1e-230,1e-231,1e-232,1e-233,1e-234,1e-235,1e-236,1e-237,1e-238,1e-239,
  1e-240,1e-241,1e-242,1e-243,1e-244,1e-245,1e-246,1e-247,1e-248,1e-249,
  1e-250,1e-251,1e-252,1e-253,1e-254,1e-255,1e-256,1e-257,1e-258,1e-259,
  1e-260,1e-261,1e-262,1e-263,1e-264,1e-265,1e-266,1e-267,1e-268,1e-269,
  1e-270,1e-271,1e-272,1e-273,1e-274,1e-275,1e-276,1e-277,1e-278,1e-279,
  1e-280,1e-281,1e-282,1e-283,1e-284,1e-285,1e-286,1e-287,1e-288,1e-289,
  1e-290,1e-291,1e-292,1e-293,1e-294,1e-295,1e-296,1e-297,1e-298,1e-299,
  1e-300,1e-301,1e-302,1e-303,1e-304,1e-305,1e-306,1e-307,1e-308,1e-309
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
      scaler= scaler*10.0;
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
      step= array_elements(log_01) - 1;
    }
    else
      step= array_elements(log_10) - 1;
    for (; exponent > step; exponent-= step)
      result*= neg_exp ? log_01[step] : log_10[step];
    result*= neg_exp ? log_01[exponent] : log_10[exponent];
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
