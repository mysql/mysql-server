/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
=======================================================================
  NOTE: this library implements SQL standard "exact numeric" type
  and is not at all generic, but rather intentinally crippled to
  follow the standard :)
=======================================================================
  Quoting the standard
  (SQL:2003, Part 2 Foundations, aka ISO/IEC 9075-2:2003)

4.4.2 Characteristics of numbers, page 27:

  An exact numeric type has a precision P and a scale S. P is a positive
  integer that determines the number of significant digits in a
  particular radix R, where R is either 2 or 10. S is a non-negative
  integer. Every value of an exact numeric type of scale S is of the
  form n*10^{-S}, where n is an integer such that ­-R^P <= n <= R^P.

  [...]

  If an assignment of some number would result in a loss of its most
  significant digit, an exception condition is raised. If least
  significant digits are lost, implementation-defined rounding or
  truncating occurs, with no exception condition being raised.

  [...]

  Whenever an exact or approximate numeric value is assigned to an exact
  numeric value site, an approximation of its value that preserves
  leading significant digits after rounding or truncating is represented
  in the declared type of the target. The value is converted to have the
  precision and scale of the target. The choice of whether to truncate
  or round is implementation-defined.

  [...]

  All numeric values between the smallest and the largest value,
  inclusive, in a given exact numeric type have an approximation
  obtained by rounding or truncation for that type; it is
  implementation-defined which other numeric values have such
  approximations.

5.3 <literal>, page 143

  <exact numeric literal> ::=
    <unsigned integer> [ <period> [ <unsigned integer> ] ]
  | <period> <unsigned integer>

6.1 <data type>, page 165:

  19) The <scale> of an <exact numeric type> shall not be greater than
      the <precision> of the <exact numeric type>.

  20) For the <exact numeric type>s DECIMAL and NUMERIC:

    a) The maximum value of <precision> is implementation-defined.
       <precision> shall not be greater than this value.
    b) The maximum value of <scale> is implementation-defined. <scale>
       shall not be greater than this maximum value.

  21) NUMERIC specifies the data type exact numeric, with the decimal
      precision and scale specified by the <precision> and <scale>.

  22) DECIMAL specifies the data type exact numeric, with the decimal
      scale specified by the <scale> and the implementation-defined
      decimal precision equal to or greater than the value of the
      specified <precision>.

6.26 <numeric value expression>, page 241:

  1) If the declared type of both operands of a dyadic arithmetic
     operator is exact numeric, then the declared type of the result is
     an implementation-defined exact numeric type, with precision and
     scale determined as follows:

   a) Let S1 and S2 be the scale of the first and second operands
      respectively.
   b) The precision of the result of addition and subtraction is
      implementation-defined, and the scale is the maximum of S1 and S2.
   c) The precision of the result of multiplication is
      implementation-defined, and the scale is S1 + S2.
   d) The precision and scale of the result of division are
      implementation-defined.
*/

#include <decimal.h>
#include <m_ctype.h>
#include <myisampack.h>
#include <my_sys.h> /* for my_alloca */

typedef decimal_digit dec1;
typedef longlong      dec2;

#define DIG_PER_DEC1 9
#define DIG_MASK     100000000
#define DIG_BASE     1000000000
#define DIG_BASE2    LL(1000000000000000000)
#define ROUND_UP(X)  (((X)+DIG_PER_DEC1-1)/DIG_PER_DEC1)
static const dec1 powers10[DIG_PER_DEC1+1]={
  1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
static const int dig2bytes[DIG_PER_DEC1+1]={0, 1, 1, 2, 2, 3, 3, 3, 4, 4};

#define sanity(d) DBUG_ASSERT((d)->len >0 && ((d)->buf[0] | \
                              (d)->buf[(d)->len-1] | 1))

#define FIX_INTG_FRAC_ERROR(len, intg1, frac1, error)                   \
        do                                                              \
        {                                                               \
          if (unlikely(intg1+frac1 > (len)))                            \
          {                                                             \
            if (unlikely(intg1 > (len)))                                \
            {                                                           \
              intg1=(len);                                              \
              frac1=0;                                                  \
              error=E_DEC_OVERFLOW;                                     \
            }                                                           \
            else                                                        \
            {                                                           \
              frac1=(len)-intg1;                                        \
              error=E_DEC_TRUNCATED;                                    \
            }                                                           \
          }                                                             \
          else                                                          \
            error=E_DEC_OK;                                             \
        } while(0)

#define ADD(to, from1, from2, carry)  /* assume carry <= 1 */           \
        do                                                              \
        {                                                               \
          dec1 a=(from1)+(from2)+(carry);                               \
          if (((carry)= a >= DIG_BASE)) /* no division here! */         \
            a-=DIG_BASE;                                                \
          (to)=a;                                                       \
        } while(0)

#define ADD2(to, from1, from2, carry)                                   \
        do                                                              \
        {                                                               \
          dec1 a=(from1)+(from2)+(carry);                               \
          if (((carry)= a >= DIG_BASE))                                 \
            a-=DIG_BASE;                                                \
          if (unlikely(a >= DIG_BASE))                                  \
          {                                                             \
            a-=DIG_BASE;                                                \
            carry++;                                                    \
          }                                                             \
          (to)=a;                                                       \
        } while(0)

#define SUB(to, from1, from2, carry) /* to=from1-from2 */               \
        do                                                              \
        {                                                               \
          dec1 a=(from1)-(from2)-(carry);                               \
          if (((carry)= a < 0))                                         \
            a+=DIG_BASE;                                                \
          (to)=a;                                                       \
        } while(0)

#define SUB2(to, from1, from2, carry) /* to=from1-from2 */              \
        do                                                              \
        {                                                               \
          dec1 a=(from1)-(from2)-(carry);                               \
          if (((carry)= a < 0))                                         \
            a+=DIG_BASE;                                                \
          if (unlikely(a < 0))                                          \
          {                                                             \
            a+=DIG_BASE;                                                \
            carry++;                                                    \
          }                                                             \
          (to)=a;                                                       \
        } while(0)

/*
  Convert decimal to its printable string representation

  SYNOPSIS
    decimal2string()
      from    - value to convert
      to      - points to buffer where string representation should be stored
      *to_len - in:  size of to buffer
                out: length of the actually written string

  RETURN VALUE
    E_DEC_OK/E_DEC_TRUNCATED/E_DEC_OVERFLOW
*/

int decimal2string(decimal *from, char *to, int *to_len)
{
  int len, intg=from->intg, frac=from->frac, i;
  int error=E_DEC_OK;
  char *s=to;
  dec1 *buf, *buf0=from->buf, tmp;

  DBUG_ASSERT(*to_len > 2+from->sign);

  /* removing leading zeroes */
  i=((intg-1) % DIG_PER_DEC1)+1;
  while (intg > 0 && *buf0 == 0)
  {
    intg-=i;
    i=DIG_PER_DEC1;
    buf0++;
  }
  if (intg > 0)
  {
    for (i=(intg-1) % DIG_PER_DEC1; *buf0 < powers10[i--]; intg--) ;
    DBUG_ASSERT(intg > 0);
  }
  else
    intg=0;
  if (unlikely(intg+frac==0))
  {
    intg=1;
    tmp=0;
    buf0=&tmp;
  }

  len= from->sign + intg + test(frac) + frac;
  if (unlikely(len > --*to_len)) /* reserve one byte for \0 */
  {
    int i=len-*to_len;
    error= (frac && i <= frac + 1) ? E_DEC_TRUNCATED : E_DEC_OVERFLOW;
    if (frac && i >= frac + 1) i--;
    if (i > frac)
    {
      intg-= i-frac;
      frac= 0;
    }
    else
      frac-=i;
    len= from->sign + intg + test(frac) + frac;
  }
  *to_len=len;
  s[len]=0;

  if (from->sign)
    *s++='-';

  if (frac)
  {
    char *s1=s+intg;
    buf=buf0+ROUND_UP(intg);
    *s1++='.';
    for (; frac>0; frac-=DIG_PER_DEC1)
    {
      dec1 x=*buf++;
      for (i=min(frac, DIG_PER_DEC1); i; i--)
      {
        dec1 y=x/DIG_MASK;
        *s1++='0'+(uchar)y;
        x-=y*DIG_MASK;
        x*=10;
      }
    }
  }

  s+=intg;
  for (buf=buf0+ROUND_UP(intg); intg>0; intg-=DIG_PER_DEC1)
  {
    dec1 x=*--buf;
    for (i=min(intg, DIG_PER_DEC1); i; i--)
    {
      dec1 y=x/10;
      *--s='0'+(uchar)(x-y*10);
      x=y;
    }
  }
  return error;
}

/*
  Convert string to decimal

  SYNOPSIS
    str2decl()
      from    - value to convert
      to      - decimal where where the result will be stored
                to->buf and to->len must be set.
      end     - if not NULL, *end will be set to the char where
                conversion ended
      fixed   - use to->intg, to->frac as limits for input number

  NOTE
    to->intg and to->frac can be modified even when fixed=1
    (but only decreased, in this case)

  RETURN VALUE
    E_DEC_OK/E_DEC_TRUNCATED/E_DEC_OVERFLOW/E_DEC_BAD_NUM/E_DEC_OOM
*/

static int str2dec(char *from, decimal *to, char **end, my_bool fixed)
{
  char *s=from, *s1;
  int i, intg, frac, error, intg1, frac1;
  dec1 x,*buf;

  sanity(to);

  while (my_isspace(&my_charset_latin1, *s))
    s++;
  if ((to->sign= (*s == '-')))
    s++;
  else if (*s == '+')
    s++;

  s1=s;
  while (my_isdigit(&my_charset_latin1, *s))
    s++;
  intg=s-s1;
  if (*s=='.')
  {
    char *s2=s+1;
    while (my_isdigit(&my_charset_latin1, *s2))
      s2++;
    frac=s2-s-1;
  }
  else
    frac=0;
  if (end)
    *end=s1+intg+frac+test(frac);

  if (frac+intg == 0)
    return E_DEC_BAD_NUM;

  if (fixed)
  {
    if (frac > to->frac)
    {
      error=E_DEC_TRUNCATED;
      frac=to->frac;
    }
    if (intg > to->intg)
    {
      error=E_DEC_OVERFLOW;
      intg=to->intg;
    }
    intg1=ROUND_UP(intg);
    frac1=ROUND_UP(frac);
    if (intg1+frac1 > to->len)
      return E_DEC_OOM;
  }
  else
  {
    intg1=ROUND_UP(intg);
    frac1=ROUND_UP(frac);
    FIX_INTG_FRAC_ERROR(to->len, intg1, frac1, error);
    if (unlikely(error))
    {
      frac=frac1*DIG_PER_DEC1;
      if (error == E_DEC_OVERFLOW)
        intg=intg1*DIG_PER_DEC1;
    }
  }
  to->intg=intg;
  to->frac=frac;

  buf=to->buf+intg1;
  s1=s;

  for (x=0, i=0; intg; intg--)
  {
    x+= (*--s - '0')*powers10[i];

    if (unlikely(++i == DIG_PER_DEC1))
    {
      *--buf=x;
      x=0;
      i=0;
    }
  }
  if (i)
    *--buf=x;

  buf=to->buf+intg1;
  for (x=0, i=0; frac; frac--)
  {
    x= (*++s1 - '0') + x*10;

    if (unlikely(++i == DIG_PER_DEC1))
    {
      *buf++=x;
      x=0;
      i=0;
    }
  }
  if (i)
    *buf=x*powers10[DIG_PER_DEC1-i];

  return error;
}

int string2decimal(char *from, decimal *to, char **end)
{
  return str2dec(from, to, end, 0);
}

int string2decimal_fixed(char *from, decimal *to, char **end)
{
  return str2dec(from, to, end, 1);
}

/*
  Convert decimal to double

  SYNOPSIS
    decimal2double()
      from    - value to convert
      to      - result will be stored there

  RETURN VALUE
    E_DEC_OK
*/

int decimal2double(decimal *from, double *to)
{
  double x=0, t=DIG_BASE;
  int intg, frac;
  dec1 *buf=from->buf;

  for (intg=from->intg; intg > 0; intg-=DIG_PER_DEC1)
    x=x*DIG_BASE + *buf++;
  for (frac=from->frac; frac > 0; frac-=DIG_PER_DEC1, t*=DIG_BASE)
    x+=*buf++/t;
  *to=from->sign ? -x : x;
  return E_DEC_OK;
}

/*
  Convert double to decimal

  SYNOPSIS
    double2decimal()
      from    - value to convert
      to      - result will be stored there

  RETURN VALUE
    E_DEC_OK/E_DEC_OVERFLOW/E_DEC_TRUNCATED
*/

int double2decimal(double from, decimal *to)
{
  /* TODO: fix it, when we'll have dtoa */
  char s[400];
  sprintf(s, "%f", from);
  return string2decimal(s, to, 0);
}

static int ull2dec(ulonglong from, decimal *to)
{
  int intg1, error=E_DEC_OK;
  ulonglong x=from;
  dec1 *buf;

  sanity(to);

  for (intg1=1; from >= DIG_BASE; intg1++, from/=DIG_BASE);
  if (unlikely(intg1 > to->len))
  {
    intg1=to->len;
    error=E_DEC_OVERFLOW;
  }
  to->frac=0;
  to->intg=intg1*DIG_PER_DEC1;

  for (buf=to->buf+intg1; intg1; intg1--)
  {
    ulonglong y=x/DIG_BASE;
    *--buf=(dec1)(x-y*DIG_BASE);
    x=y;
  }
  return error;
}

int ulonglong2decimal(ulonglong from, decimal *to)
{
  to->sign=0;
  return ull2dec(from, to);
}

int longlong2decimal(longlong from, decimal *to)
{
  if ((to->sign= from < 0))
    return ull2dec(-from, to);
  return ull2dec(from, to);
}

int decimal2ulonglong(decimal *from, ulonglong *to)
{
  dec1 *buf=from->buf;
  ulonglong x=0;
  int intg;

  if (from->sign)
  {
      *to=ULL(0);
      return E_DEC_OVERFLOW;
  }

  for (intg=from->intg; intg > 0; intg-=DIG_PER_DEC1)
  {
    ulonglong y=x;
    x=x*DIG_BASE + *buf++;
    if (unlikely(x < y))
    {
      *to=y;
      return E_DEC_OVERFLOW;
    }
  }
  *to=x;
  return from->frac ? E_DEC_TRUNCATED : E_DEC_OK;
}

int decimal2longlong(decimal *from, longlong *to)
{
  dec1 *buf=from->buf;
  longlong x=0;
  int intg;

  for (intg=from->intg; intg > 0; intg-=DIG_PER_DEC1)
  {
    longlong y=x;
    /*
      Attention: trick!
      we're calculating -|from| instead of |from| here
      because |MIN_LONGLONG| > MAX_LONGLONG
      so we can convert -9223372036854775808 correctly
    */
    x=x*DIG_BASE - *buf++;
    if (unlikely(x > y))
    {
      *to= from->sign ? y : -y;
      return E_DEC_OVERFLOW;
    }
  }
  /* boundary case: 9223372036854775808 */
  if (unlikely(from->sign==0 && x < 0 && -x < 0))
  {
    *to= -1-x;
    return E_DEC_OVERFLOW;
  }

  *to=from->sign ? x : -x;
  return from->frac ? E_DEC_TRUNCATED : E_DEC_OK;
}

/*
  Convert decimal to its binary fixed-length representation
  two representations of the same length can be compared with memcmp
  with the correct -1/0/+1 result

  SYNOPSIS
    decimal2bin()
      from    - value to convert
      to      - points to buffer where string representation should be stored
      precision/scale - see decimal_bin_size() below

  NOTE
    the buffer is assumed to be of the size decimal_bin_size(precision, scale)

  RETURN VALUE
    E_DEC_OK/E_DEC_TRUNCATED/E_DEC_OVERFLOW
*/
int decimal2bin(decimal *from, char *to, int precision, int frac)
{
  dec1 mask=from->sign ? -1 : 0, *buf1=from->buf, *stop1;
  int error=E_DEC_OK, intg=precision-frac,
      intg0=intg/DIG_PER_DEC1,
      frac0=frac/DIG_PER_DEC1,
      intg0x=intg-intg0*DIG_PER_DEC1,
      frac0x=frac-frac0*DIG_PER_DEC1,
      intg1=from->intg/DIG_PER_DEC1,
      frac1=from->frac/DIG_PER_DEC1,
      intg1x=from->intg-intg1*DIG_PER_DEC1,
      frac1x=from->frac-frac1*DIG_PER_DEC1,
      isize0=intg0*sizeof(dec1)+dig2bytes[intg0x],
      fsize0=frac0*sizeof(dec1)+dig2bytes[frac0x],
      isize1=intg1*sizeof(dec1)+dig2bytes[intg1x],
      fsize1=frac1*sizeof(dec1)+dig2bytes[frac1x];
  if (isize0 < isize1)
  {
    buf1+=intg1-intg0+(intg1x>0)-(intg0x>0);
    intg1=intg0; intg1x=intg0x;
    error=E_DEC_OVERFLOW;
  }
  else if (isize0 > isize1)
  {
    while (isize0-- > isize1)
      *to++= (char)mask;
  }
  if (fsize0 < fsize1)
  {
    frac1=frac0; frac1x=frac0x;
    error=E_DEC_TRUNCATED;
  }
  else if (fsize0 > fsize1 && frac1x)
  {
    if (frac0 == frac1)
      frac1x=frac0x;
    else
    {
      frac1++;
      frac1x=0;
    }
  }

  /* intg1x part */
  if (intg1x)
  {
    int i=dig2bytes[intg1x];
    dec1 x=(*buf1++ % powers10[intg1x]) ^ mask;
    switch (i)
    {
      case 1: mi_int1store(to, x); break;
      case 2: mi_int2store(to, x); break;
      case 3: mi_int3store(to, x); break;
      case 4: mi_int4store(to, x); break;
      default: DBUG_ASSERT(0);
    }
    to+=i;
  }

  /* intg1+frac1 part */
  for (stop1=buf1+intg1+frac1; buf1 < stop1; to+=sizeof(dec1))
  {
    dec1 x=*buf1++ ^ mask;
    DBUG_ASSERT(sizeof(dec1) == 4);
    mi_int4store(to, x);
  }

  /* frac1x part */
  if (frac1x)
  {
    int i=dig2bytes[frac1x];
    dec1 x=(*buf1 / powers10[DIG_PER_DEC1 - frac1x]) ^ mask;
    switch (i)
    {
      case 1: mi_int1store(to, x); break;
      case 2: mi_int2store(to, x); break;
      case 3: mi_int3store(to, x); break;
      case 4: mi_int4store(to, x); break;
      default: DBUG_ASSERT(0);
    }
    to+=i;
  }
  if (fsize0 > fsize1)
  {
    while (fsize0-- > fsize1)
      *to++=(uchar)mask;
  }
  return error;
}

/*
  Restores decimal from its binary fixed-length representation

  SYNOPSIS
    bin2decimal()
      from    - value to convert
      to      - result
      precision/scale - see decimal_bin_size() below

  NOTE
    see decimal2bin()
    the buffer is assumed to be of the size decimal_bin_size(precision, scale)

  RETURN VALUE
    E_DEC_OK/E_DEC_TRUNCATED/E_DEC_OVERFLOW
*/

int bin2decimal(char *from, decimal *to, int precision, int scale)
{
  int error=E_DEC_OK, intg=precision-scale,
      intg0=intg/DIG_PER_DEC1, frac0=scale/DIG_PER_DEC1,
      intg0x=intg-intg0*DIG_PER_DEC1, frac0x=scale-frac0*DIG_PER_DEC1,
      intg1=intg0+(intg0x>0), frac1=frac0+(frac0x>0);
  dec1 *buf=to->buf, mask=(*from <0) ? -1 : 0;
  char *stop;

  sanity(to);

  FIX_INTG_FRAC_ERROR(to->len, intg1, frac1, error);
  if (unlikely(error))
  {
    if (intg1 < intg0+(intg0x>0))
    {
      from+=dig2bytes[intg0x]+sizeof(dec1)*(intg0-intg1);
      frac0=frac0x=intg0x=0;
      intg0=intg1;
    }
    else
    {
      frac0x=0;
      frac0=frac1;
    }
  }

  to->sign=(mask != 0);
  to->intg=intg0*DIG_PER_DEC1+intg0x;
  to->frac=frac0*DIG_PER_DEC1+frac0x;

  if (intg0x)
  {
    int i=dig2bytes[intg0x];
    dec1 x;
    switch (i)
    {
      case 1: x=mi_sint1korr(from); break;
      case 2: x=mi_sint2korr(from); break;
      case 3: x=mi_sint3korr(from); break;
      case 4: x=mi_sint4korr(from); break;
      default: DBUG_ASSERT(0);
    }
    from+=i;
    *buf=x ^ mask;
    if (buf > to->buf || *buf != 0)
      buf++;
    else
      to->intg-=intg0x;
  }
  for (stop=from+intg0*sizeof(dec1); from < stop; from+=sizeof(dec1))
  {
    DBUG_ASSERT(sizeof(dec1) == 4);
    *buf=mi_sint4korr(from) ^ mask;
    if (buf > to->buf || *buf != 0)
      buf++;
    else
      to->intg-=DIG_PER_DEC1;
  }
  DBUG_ASSERT(to->intg >=0);
  for (stop=from+frac0*sizeof(dec1); from < stop; from+=sizeof(dec1))
  {
    DBUG_ASSERT(sizeof(dec1) == 4);
    *buf=mi_sint4korr(from) ^ mask;
    buf++;
  }
  if (frac0x)
  {
    int i=dig2bytes[frac0x];
    dec1 x;
    switch (i)
    {
      case 1: x=mi_sint1korr(from); break;
      case 2: x=mi_sint2korr(from); break;
      case 3: x=mi_sint3korr(from); break;
      case 4: x=mi_sint4korr(from); break;
      default: DBUG_ASSERT(0);
    }
    *buf=(x ^ mask) * powers10[DIG_PER_DEC1 - frac0x];
    buf++;
  }
  return error;
}

/*
  Returns the size of array to hold a decimal with given precision and scale

  RETURN VALUE
    size in dec1
    (multiply by sizeof(dec1) to get the size if bytes)
*/

int decimal_size(int precision, int scale)
{
  DBUG_ASSERT(scale >= 0 && precision > 0 && scale <= precision);
  return ROUND_UP(precision-scale)+ROUND_UP(scale);
}

/*
  Returns the size of array to hold a binary representation of a decimal

  RETURN VALUE
    size in bytes
*/

int decimal_bin_size(int precision, int scale)
{
  int intg=precision-scale,
      intg0=intg/DIG_PER_DEC1, frac0=scale/DIG_PER_DEC1,
      intg0x=intg-intg0*DIG_PER_DEC1, frac0x=scale-frac0*DIG_PER_DEC1;

  DBUG_ASSERT(scale >= 0 && precision > 0 && scale <= precision);
  return intg0*sizeof(dec1)+dig2bytes[intg0x]+
         frac0*sizeof(dec1)+dig2bytes[frac0x];
}

/*
  Rounds the decimal to "scale" digits

  SYNOPSIS
    decimal_round()
      from    - decimal to round,
      to      - result buffer. from==to is allowed
      scale   - to what position to round. can be negative!
      mode    - round to nearest even or truncate

  NOTES
    scale can be negative !
    one TRUNCATED error (line XXX below) isn't treated very logical :(

  RETURN VALUE
    E_DEC_OK/E_DEC_TRUNCATED
*/

int decimal_round(decimal *from, decimal *to, int scale, decimal_round_mode mode)
{
  int frac0=scale>0 ? ROUND_UP(scale) : scale/DIG_PER_DEC1,
      frac1=ROUND_UP(from->frac),
      intg0=ROUND_UP(from->intg), error=E_DEC_OK, len=to->len;
  dec1 *buf0=from->buf, *buf1=to->buf, x, y, carry=0;

  sanity(to);

  if (unlikely(frac0+intg0 > len))
  {
    frac0=len-intg0;
    scale=frac0*DIG_PER_DEC1;
    error=E_DEC_TRUNCATED;
  }

  if (scale+from->intg <=0)
  {
    decimal_make_zero(to);
    return E_DEC_OK;
  }

  if (to != from)
  {
    dec1 *end=buf0+intg0+min(frac1, frac0);
    while (buf0 < end)
      *buf1++ = *buf0++;
    buf0=from->buf;
    buf1=to->buf;
    to->sign=from->sign;
    to->intg=min(from->intg, len*DIG_PER_DEC1);
  }

  if (frac0 > frac1)
  {
    buf1+=intg0+frac1;
    while (frac0-- > frac1)
      *buf1++=0;
    goto done;
  }

  if (scale >= from->frac)
    goto done; /* nothing to do */

  DBUG_ASSERT(frac0+intg0 > 0);
  buf0+=intg0+frac0-1;
  buf1+=intg0+frac0-1;
  if (scale == frac0*DIG_PER_DEC1)
  {
    if (mode != TRUNCATE)
    {
      x=buf0[1]/DIG_MASK;
      if (x > 5 || (x == 5 && (mode == HALF_UP || *buf0 & 1)))
        (*buf1)++;
    }
  }
  else
  {
    int pos=frac0*DIG_PER_DEC1-scale-1;
    if (mode != TRUNCATE)
    {
      x=*buf1 / powers10[pos];
      y=x % 10;
      if (y > 5 || (y == 5 && (mode == HALF_UP || (x/10) & 1)))
        x+=10;
      *buf1=powers10[pos]*(x-y);
    }
    else
      *buf1=(*buf1/powers10[pos+1])*powers10[pos+1];
  }
  if (*buf1 >= DIG_BASE)
  {
    carry=1;
    *buf1-=DIG_BASE;
    while (carry && --buf1 >= to->buf)
      ADD(*buf1, *buf1, 0, carry);
    if (unlikely(carry))
    {
      /* shifting the number to create space for new digit */
      if (frac0+intg0 >= len)
      {
        frac0--;
        scale=frac0*DIG_PER_DEC1;
        error=E_DEC_TRUNCATED; /* XXX */
      }
      for (buf1=to->buf+frac0+intg0; buf1 > to->buf; buf1--)
      {
        buf1[0]=buf1[-1];
      }
      *buf1=1;
    }
  }
  if (scale<0) scale=0;

done:
  to->frac=scale;
  return error;
}

/*
  Returns the size of the result of the operation

  SYNOPSIS
    decimal_result_size()
      from1   - operand of the unary operation or first operand of the
                binary operation
      from2   - second operand of the binary operation
      op      - operation. one char '+', '-', '*', '/' are allowed
                others may be added later
      param   - extra param to the operation. unused for '+', '-', '*'
                scale increment for '/'

  NOTE
    returned valued may be larger than the actual buffer requred
    in the operation, as decimal_result_size, by design, operates on
    precision/scale values only and not on the actual decimal number

  RETURN VALUE
    size of to->buf array in dec1 elements. to get size in bytes
    multiply by sizeof(dec1)
*/

int decimal_result_size(decimal *from1, decimal *from2, char op, int param)
{
  switch (op) {
  case '-':
    return ROUND_UP(max(from1->intg, from2->intg)) +
           ROUND_UP(max(from1->frac, from2->frac));
  case '+':
    return ROUND_UP(max(from1->intg, from2->intg)+1) +
           ROUND_UP(max(from1->frac, from2->frac));
  case '*':
    return ROUND_UP(from1->intg+from2->intg)+
           ROUND_UP(from1->frac)+ROUND_UP(from2->frac);
  case '/':
    return ROUND_UP(from1->intg+from2->intg+1+from1->frac+from2->frac+param);
  default: DBUG_ASSERT(0);
  }
  return -1; /* shut up the warning */
}

static int do_add(decimal *from1, decimal *from2, decimal *to)
{
  int intg1=ROUND_UP(from1->intg), intg2=ROUND_UP(from2->intg),
      frac1=ROUND_UP(from1->frac), frac2=ROUND_UP(from2->frac),
      frac0=max(frac1, frac2), intg0=max(intg1, intg2), error;
  dec1 *buf1, *buf2, *buf0, *stop, *stop2, x, carry;

  sanity(to);

  /* is there a need for extra word because of carry ? */
  x=intg1 > intg2 ? from1->buf[0] :
    intg2 > intg1 ? from2->buf[0] :
    from1->buf[0] + from2->buf[0] ;
  if (unlikely(x > DIG_MASK*9)) /* yes, there is */
  {
    intg0++;
    to->buf[0]=0; /* safety */
  }

  FIX_INTG_FRAC_ERROR(to->len, intg0, frac0, error);
  buf0=to->buf+intg0+frac0;

  to->sign=from1->sign;
  to->frac=max(from1->frac, from2->frac);
  to->intg=intg0*DIG_PER_DEC1;
  if (unlikely(error))
  {
    set_if_smaller(to->frac, frac0*DIG_PER_DEC1);
    set_if_smaller(frac1, frac0);
    set_if_smaller(frac2, frac0);
    set_if_smaller(intg1, intg0);
    set_if_smaller(intg2, intg0);
  }

  /* part 1 - max(frac) ... min (frac) */
  if (frac1 > frac2)
  {
    buf1=from1->buf+intg1+frac1;
    stop=from1->buf+intg1+frac2;
    buf2=from2->buf+intg2+frac2;
    stop2=from1->buf+(intg1 > intg2 ? intg1-intg2 : 0);
  }
  else
  {
    buf1=from2->buf+intg2+frac2;
    stop=from2->buf+intg2+frac1;
    buf2=from1->buf+intg1+frac1;
    stop2=from2->buf+(intg2 > intg1 ? intg2-intg1 : 0);
  }
  while (buf1 > stop)
    *--buf0=*--buf1;

  /* part 2 - min(frac) ... min(intg) */
  carry=0;
  while (buf1 > stop2)
  {
    ADD(*--buf0, *--buf1, *--buf2, carry);
  }

  /* part 3 - min(intg) ... max(intg) */
  buf1= intg1 > intg2 ? ((stop=from1->buf)+intg1-intg2) :
                        ((stop=from2->buf)+intg2-intg1) ;
  while (buf1 > stop)
  {
    ADD(*--buf0, *--buf1, 0, carry);
  }

  if (unlikely(carry))
    *--buf0=1;
  DBUG_ASSERT(buf0 == to->buf || buf0 == to->buf+1);

  return error;
}

/* to=from1-from2.
   if to==0, return -1/0/+1 - the result of the comparison */
static int do_sub(decimal *from1, decimal *from2, decimal *to)
{
  int intg1=ROUND_UP(from1->intg), intg2=ROUND_UP(from2->intg),
      frac1=ROUND_UP(from1->frac), frac2=ROUND_UP(from2->frac);
  int frac0=max(frac1, frac2), error;
  dec1 *buf1, *buf2, *buf0, *stop1, *stop2, *start1, *start2, carry=0;

  /* let carry:=1 if from2 > from1 */
  start1=buf1=from1->buf; stop1=buf1+intg1;
  start2=buf2=from2->buf; stop2=buf2+intg2;
  if (unlikely(*buf1 == 0))
  {
    while (buf1 < stop1 && *buf1 == 0)
      buf1++;
    start1=buf1;
    intg1=stop1-buf1;
  }
  if (unlikely(*buf2 == 0))
  {
    while (buf2 < stop2 && *buf2 == 0)
      buf2++;
    start2=buf2;
    intg2=stop2-buf2;
  }
  if (intg2 > intg1)
    carry=1;
  else if (intg2 == intg1)
  {
    while (unlikely(stop1[frac1-1] == 0))
      frac1--;
    while (unlikely(stop2[frac2-1] == 0))
      frac2--;
    while (buf1 < stop1+frac1 && buf2 < stop2+frac2 && *buf1 == *buf2)
      buf1++, buf2++;
    if (buf1 < stop1+frac1)
      if (buf2 < stop2+frac2)
        carry= *buf2 > *buf1;
      else
        carry= 0;
    else
      if (buf2 < stop2+frac2)
        carry=1;
      else /* short-circuit everything: from1 == from2 */
      {
        if (to == 0) /* decimal_cmp() */
          return 0;
        decimal_make_zero(to);
        return E_DEC_OK;
      }
  }

  if (to == 0) /* decimal_cmp() */
    return carry == from1->sign ? 1 : -1;

  sanity(to);

  to->sign=from1->sign;

  /* ensure that always from1 > from2 (and intg1 >= intg2) */
  if (carry)
  {
    swap_variables(decimal *,from1,from1);
    swap_variables(dec1 *,start1, start2);
    swap_variables(int,intg1,intg2);
    swap_variables(int,frac1,frac2);
    to->sign= 1 - to->sign;
  }

  FIX_INTG_FRAC_ERROR(to->len, intg1, frac0, error);
  buf0=to->buf+intg1+frac0;

  to->frac=max(from1->frac, from2->frac);
  to->intg=intg1*DIG_PER_DEC1;
  if (unlikely(error))
  {
    set_if_smaller(to->frac, frac0*DIG_PER_DEC1);
    set_if_smaller(frac1, frac0);
    set_if_smaller(frac2, frac0);
    set_if_smaller(intg2, intg1);
  }
  carry=0;

  /* part 1 - max(frac) ... min (frac) */
  if (frac1 > frac2)
  {
    buf1=start1+intg1+frac1;
    stop1=start1+intg1+frac2;
    buf2=start2+intg2+frac2;
    while (buf1 > stop1)
      *--buf0=*--buf1;
  }
  else
  {
    buf1=start1+intg1+frac1;
    buf2=start2+intg2+frac2;
    stop2=start2+intg2+frac1;
    while (buf2 > stop2)
    {
      SUB(*--buf0, 0, *--buf2, carry);
    }
  }

  /* part 2 - min(frac) ... intg2 */
  while (buf2 > start2)
  {
    SUB(*--buf0, *--buf1, *--buf2, carry);
  }

  /* part 3 - intg2 ... intg1 */
  while (carry && buf1 > start1)
  {
    SUB(*--buf0, *--buf1, 0, carry);
  }

  while (buf1 > start1)
    *--buf0=*--buf1;

  while (buf0 > to->buf)
    *--buf0=0;

  return error;
}

int decimal_add(decimal *from1, decimal *from2, decimal *to)
{
  if (likely(from1->sign == from2->sign))
    return do_add(from1, from2, to);
  return do_sub(from1, from2, to);
}

int decimal_sub(decimal *from1, decimal *from2, decimal *to)
{
  if (likely(from1->sign == from2->sign))
    return do_sub(from1, from2, to);
  return do_add(from1, from2, to);
}

int decimal_cmp(decimal *from1, decimal *from2)
{
  if (likely(from1->sign == from2->sign))
    return do_sub(from1, from2, 0);
  return from1->sign > from2->sign ? -1 : 1;
}

/*
  multiply two decimals

  SYNOPSIS
    decimal_mul()
      from1, from2 - factors
      to      - product

  RETURN VALUE
    E_DEC_OK/E_DEC_TRUNCATED/E_DEC_OVERFLOW;

  NOTES
    in this implementation, with sizeof(dec1)=4 we have DIG_PER_DEC1=9,
    and 63-digit number will take only 7 dec1 words (basically a 7-digit
    "base 999999999" number).  Thus there's no need in fast multiplication
    algorithms, 7-digit numbers can be multiplied with a naive O(n*n)
    method.

    XXX if this library is to be used with huge numbers of thousands of
    digits, fast multiplication must be implemented.
*/
int decimal_mul(decimal *from1, decimal *from2, decimal *to)
{
  int intg1=ROUND_UP(from1->intg), intg2=ROUND_UP(from2->intg),
      frac1=ROUND_UP(from1->frac), frac2=ROUND_UP(from2->frac),
      intg0=ROUND_UP(from1->intg+from2->intg),
      frac0=frac1+frac2, error, i, j;
  dec1 *buf1=from1->buf+intg1, *buf2=from2->buf+intg2, *buf0,
       *start2, *stop2, *stop1, *start0, carry;

  sanity(to);

  i=intg0;
  j=frac0;
  FIX_INTG_FRAC_ERROR(to->len, intg0, frac0, error);
  to->sign=from1->sign != from2->sign;
  to->frac=from1->frac+from2->frac;
  to->intg=intg0*DIG_PER_DEC1;

  if (unlikely(error))
  {
    set_if_smaller(to->frac, frac0*DIG_PER_DEC1);
    set_if_smaller(to->intg, intg0*DIG_PER_DEC1);
    if (unlikely(i > intg0))
    {
      i-=intg0;
      j=i >> 1;
      intg1-= j;
      intg2-=i-j;
      frac1=frac2=0; /* frac0 is already 0 here */
    }
    else
    {
      j-=frac0;
      i=j >> 1;
      frac1-= i;
      frac2-=j-i;
    }
  }
  start0=to->buf+intg0+frac0-1;
  start2=buf2+frac2-1;
  stop1=buf1-intg1;
  stop2=buf2-intg2;

  bzero(to->buf, (intg0+frac0)*sizeof(dec1));

  for (buf1+=frac1-1; buf1 >= stop1; buf1--, start0--)
  {
    carry=0;
    for (buf0=start0, buf2=start2; buf2 >= stop2; buf2--, buf0--)
    {
      dec1 hi, lo;
      dec2 p= ((dec2)*buf1) * ((dec2)*buf2);
      hi=(dec1)(p/DIG_BASE);
      lo=(dec1)(p-((dec2)hi)*DIG_BASE);
      ADD2(*buf0, *buf0, lo, carry);
      carry+=hi;
    }
    for (; carry; buf0--)
      ADD(*buf0, *buf0, 0, carry);
  }
  return error;
}

/*
  naive division algorithm (Knuth's Algorithm D in 4.3.1) -
  it's ok for short numbers
  also we're using alloca() to allocate a temporary buffer

  XXX if this library is to be used with huge numbers of thousands of
  digits, fast division must be implemented and alloca should be
  changed to malloc (or at least fallback to malloc if alloca() fails)
  but then, decimal_mod() should be rewritten too :(
*/
static int do_div_mod(decimal *from1, decimal *from2,
                       decimal *to, decimal *mod, int scale_incr)
{
  int frac1=ROUND_UP(from1->frac)*DIG_PER_DEC1, prec1=from1->intg+frac1,
      frac2=ROUND_UP(from2->frac)*DIG_PER_DEC1, prec2=from2->intg+frac2,
      error, i, intg0, frac0, len1, len2, dlen1, dintg;
  dec1 *buf0, *buf1=from1->buf, *buf2=from2->buf, *tmp1,
       *start2, *stop2, *stop1, *stop0, norm2, carry, *start1;
  dec2 norm_factor, x, guess, y;

  if (mod)
    to=mod;

  sanity(to);

  /* removing all the leading zeroes */
  i=((prec1-1) % DIG_PER_DEC1)+1;
  while (prec1 > 0 && *buf1 == 0)
  {
    prec1-=i;
    i=DIG_PER_DEC1;
    buf1++;
  }
  if (prec1 <= 0)
  { /* short-circuit everything: from1 == 0 */
    decimal_make_zero(to);
    return E_DEC_OK;
  }
  for (i=(prec1-1) % DIG_PER_DEC1; *buf1 < powers10[i--]; prec1--) ;
  DBUG_ASSERT(prec1 > 0);

  i=((prec2-1) % DIG_PER_DEC1)+1;
  while (prec2 > 0 && *buf2 == 0)
  {
    prec2-=i;
    i=DIG_PER_DEC1;
    buf2++;
  }
  if (prec2 <= 0) /* short-circuit everything: from2 == 0 */
    return E_DEC_DIV_ZERO;

  for (i=(prec2-1) % DIG_PER_DEC1; *buf2 < powers10[i--]; prec2--) ;
  DBUG_ASSERT(prec2 > 0);

  /* let's fix scale_incr, taking into account frac1,frac2 increase */
  if ((scale_incr-= frac1 - from1->frac + frac2 - from2->frac) < 0)
    scale_incr=0;

  dintg=(prec1-frac1)-(prec2-frac2)+(*buf1 >= *buf2);
  if (dintg < 0)
  {
    dintg/=DIG_PER_DEC1;
    intg0=0;
  }
  else
    intg0=ROUND_UP(dintg);
  if (mod)
  {
    /* we're calculating N1 % N2.
       The result will have
         frac=max(frac1, frac2), as for subtraction
         intg=intg2
    */
    to->sign=from1->sign;
    to->frac=max(from1->frac, from2->frac);
    frac0=0;
  }
  else
  {
    /*
      we're calculating N1/N2. N1 is in the buf1, has prec1 digits
      N2 is in the buf2, has prec2 digits. Scales are frac1 and
      frac2 accordingly.
      Thus, the result will have
         frac = ROUND_UP(frac1+frac2+scale_incr)
      and
         intg = (prec1-frac1) - (prec2-frac2) + 1
         prec = intg+frac
    */
    frac0=ROUND_UP(frac1+frac2+scale_incr);
    FIX_INTG_FRAC_ERROR(to->len, intg0, frac0, error);
    to->sign=from1->sign != from2->sign;
    to->intg=intg0*DIG_PER_DEC1;
    to->frac=frac0*DIG_PER_DEC1;
  }
  buf0=to->buf;
  stop0=buf0+intg0+frac0;
  while (dintg++ < 0)
    *buf0++=0;

  len1=(i=ROUND_UP(prec1))+ROUND_UP(2*frac2+scale_incr+1);
  if (!(tmp1=my_alloca(len1*sizeof(dec1))))
    return E_DEC_OOM;
  memcpy(tmp1, buf1, i*sizeof(dec1));
  bzero(tmp1+i, (len1-i)*sizeof(dec1));

  start1=tmp1;
  stop1=start1+len1;
  start2=buf2;
  stop2=buf2+ROUND_UP(prec2)-1;

  /* removing end zeroes */
  while (*stop2 == 0 && stop2 >= start2)
    stop2--;
  len2= ++stop2 - start2;

  /*
    calculating norm2 (normalized *start2) - we need *start2 to be large
    (at least > DIG_BASE/2), but unlike Knuth's Alg. D we don't want to
    normalize input numbers (as we don't make a copy of the divisor).
    Thus we normalize first dec1 of buf2 only, and we'll normalize *start1
    on the fly for the purpose of guesstimation only.
    It's also faster, as we're saving on normalization of buf2
  */
  norm_factor=DIG_BASE/(*start2+1);
  norm2=(dec1)(norm_factor*start2[0]);
  if (likely(len2>1))
    norm2+=(dec1)(norm_factor*start2[1]/DIG_BASE);

  /* main loop */
  for ( ; buf0 < stop0; buf0++)
  {
    /* short-circuit, if possible */
    if (unlikely(*start1 == 0))
    {
      start1++;
      *buf0=0;
      continue;
    }

    /* D3: make a guess */
    if (*start1 >= *start2)
    {
      x=start1[0];
      y=start1[1];
      dlen1=len2-1;
    }
    else
    {
      x=((dec2)start1[0])*DIG_BASE+start1[1];
      y=start1[2];
      dlen1=len2;
    }
    guess=(norm_factor*x+norm_factor*y/DIG_BASE)/norm2;
    if (unlikely(guess >= DIG_BASE))
      guess=DIG_BASE-1;
    if (likely(len2>1))
    {
      /* hmm, this is a suspicious trick - I removed normalization here */
      if (start2[1]*guess > (x-guess*start2[0])*DIG_BASE+y)
        guess--;
      if (unlikely(start2[1]*guess > (x-guess*start2[0])*DIG_BASE+y))
        guess--;
      DBUG_ASSERT(start2[1]*guess <= (x-guess*start2[0])*DIG_BASE+y);
    }

    /* D4: multiply and subtract */
    buf2=stop2;
    buf1=start1+dlen1;
    DBUG_ASSERT(buf1 < stop1);
    for (carry=0; buf2 > start2; buf1--)
    {
      dec1 hi, lo;
      x=guess * (*--buf2);
      hi=(dec1)(x/DIG_BASE);
      lo=(dec1)(x-((dec2)hi)*DIG_BASE);
      SUB2(*buf1, *buf1, lo, carry);
      carry+=hi;
    }
    for (; buf1 >= start1; buf1--)
    {
      SUB2(*buf1, *buf1, 0, carry);
    }

    /* D5: check the remainder */
    if (unlikely(carry))
    {
      DBUG_ASSERT(carry==1);
      /* D6: correct the guess */
      guess--;
      buf2=stop2;
      buf1=start1+dlen1;
      for (carry=0; buf2 > start2; buf1--)
      {
        ADD(*buf1, *buf1, *--buf2, carry);
      }
      for (; buf1 >= start1; buf1--)
      {
        SUB2(*buf1, *buf1, 0, carry);
      }
      DBUG_ASSERT(carry==1);
    }
    *buf0=(dec1)guess;
    if (*start1 == 0)
      start1++;
  }
  if (mod)
  {
    /*
      now the result is in tmp1, it has
        intg=prec1-frac1
        frac=max(frac1, frac2)=to->frac
    */
    buf0=to->buf;
    intg0=ROUND_UP(prec1-frac1)-(start1-tmp1);
    frac0=ROUND_UP(to->frac);
    error=E_DEC_OK;
    if (intg0<=0)
    {
      if (unlikely(-intg0 >= to->len))
      {
        decimal_make_zero(to);
        error=E_DEC_TRUNCATED;
        goto done;
      }
      stop1=start1+frac0;
      frac0+=intg0;
      to->intg=0;
      while (intg0++ < 0)
        *buf0++=0;
    }
    else
    {
      if (unlikely(intg0 > to->len))
      {
        frac0=0;
        intg0=to->len;
        error=E_DEC_OVERFLOW;
        goto done;
      }
      DBUG_ASSERT(intg0 <= ROUND_UP(from2->intg));
      stop1=start1+frac0+intg0;
      to->intg=min(intg0*DIG_PER_DEC1, from2->intg);
    }
    if (unlikely(intg0+frac0 > to->len))
    {
      stop1-=to->len-frac0-intg0;
      frac0=to->len-intg0;
      to->frac=frac0*DIG_PER_DEC1;
        error=E_DEC_TRUNCATED;
    }
    while (start1 < stop1)
        *buf0++=*start1++;
  }
done:
  my_afree(tmp1);
  return error;
}

/*
  division of two decimals

  SYNOPSIS
    decimal_div()
      from1   - dividend
      from2   - divisor
      to      - quotient

  RETURN VALUE
    E_DEC_OK/E_DEC_TRUNCATED/E_DEC_OVERFLOW/E_DEC_DIV_ZERO;

  NOTES
    see do_div_mod()
*/

int decimal_div(decimal *from1, decimal *from2, decimal *to, int scale_incr)
{
  return do_div_mod(from1, from2, to, 0, scale_incr);
}

/*
  modulus

  SYNOPSIS
    decimal_mod()
      from1   - dividend
      from2   - divisor
      to      - modulus

  RETURN VALUE
    E_DEC_OK/E_DEC_TRUNCATED/E_DEC_OVERFLOW/E_DEC_DIV_ZERO;

  NOTES
    see do_div_mod()

  DESCRIPTION
    the modulus R in    R = M mod N

   is defined as

     0 <= |R| < |M|
     sign R == sign M
     R = M - k*N, where k is integer

   thus, there's no requirement for M or N to be integers
*/

int decimal_mod(decimal *from1, decimal *from2, decimal *to)
{
  return do_div_mod(from1, from2, 0, to, 0);
}

#ifdef MAIN

int full=0;
decimal a, b, c;
char buf1[100], buf2[100], buf3[100];

void dump_decimal(decimal *d)
{
  int i;
  printf("/* intg=%d, frac=%d, sign=%d, buf[]={", d->intg, d->frac, d->sign);
  for (i=0; i < ROUND_UP(d->frac)+ROUND_UP(d->intg)-1; i++)
    printf("%09d, ", d->buf[i]);
  printf("%09d} */ ", d->buf[i]);
}

void print_decimal(decimal *d)
{
  char s[100];
  int slen=sizeof(s);

  if (full) dump_decimal(d);
  decimal2string(d, s, &slen);
  printf("'%s'", s);
}

void test_d2s()
{
  char s[100];
  int slen, res;

  /***********************************/
  printf("==== decimal2string ====\n");
  a.buf[0]=12345; a.intg=5; a.frac=0; a.sign=0;
  slen=sizeof(s);
  res=decimal2string(&a, s, &slen);
  dump_decimal(&a); printf("  -->  res=%d str='%s' len=%d\n", res, s, slen);

  a.buf[1]=987000000; a.frac=3;
  slen=sizeof(s);
  res=decimal2string(&a, s, &slen);
  dump_decimal(&a); printf("  -->  res=%d str='%s' len=%d\n", res, s, slen);

  a.sign=1;
  slen=sizeof(s);
  res=decimal2string(&a, s, &slen);
  dump_decimal(&a); printf("  -->  res=%d str='%s' len=%d\n", res, s, slen);

  slen=8;
  res=decimal2string(&a, s, &slen);
  dump_decimal(&a); printf("  -->  res=%d str='%s' len=%d\n", res, s, slen);

  slen=5;
  res=decimal2string(&a, s, &slen);
  dump_decimal(&a); printf("  -->  res=%d str='%s' len=%d\n", res, s, slen);

  a.buf[0]=987000000; a.frac=3; a.intg=0;
  slen=sizeof(s);
  res=decimal2string(&a, s, &slen);
  dump_decimal(&a); printf("  -->  res=%d str='%s' len=%d\n", res, s, slen);
}

void test_s2d(char *s)
{
  char s1[100];
  sprintf(s1, "'%s'", s);
  printf("len=%2d %-30s => res=%d    ", a.len, s1, string2decimal(s, &a, 0));
  print_decimal(&a);
  printf("\n");
}

void test_d2f(char *s)
{
  char s1[100];
  double x;
  int res;

  sprintf(s1, "'%s'", s);
  string2decimal(s, &a, 0);
  res=decimal2double(&a, &x);
  if (full) dump_decimal(&a);
  printf("%-40s => res=%d    %.*g\n", s1, res, a.intg+a.frac, x);
}

void test_d2b2d(char *str, int p, int s)
{
  char s1[100], buf[100];
  double x;
  int res, i, size=decimal_bin_size(p, s);

  sprintf(s1, "'%s'", str);
  string2decimal(str, &a, 0);
  res=decimal2bin(&a, buf, p, s);
  printf("%-31s {%2d, %2d} => res=%d size=%-2d ", s1, p, s, res, size);
  if (full)
  {
    printf("0x");
    for (i=0; i < size; i++)
      printf("%02x", ((uchar *)buf)[i]);
  }
  res=bin2decimal(buf, &a, p, s);
  printf(" => res=%d ", res);
  print_decimal(&a);
  printf("\n");
}
void test_f2d(double from)
{
  int res;

  res=double2decimal(from, &a);
  printf("%-40.*f => res=%d    ", DBL_DIG-2, from, res);
  print_decimal(&a);
  printf("\n");
}

void test_ull2d(ulonglong from)
{
  char s[100];
  int res;

  res=ulonglong2decimal(from, &a);
  longlong10_to_str(from,s,10);
  printf("%-40s => res=%d    ", s, res);
  print_decimal(&a);
  printf("\n");
}

void test_ll2d(longlong from)
{
  char s[100];
  int res;

  res=longlong2decimal(from, &a);
  longlong10_to_str(from,s,-10);
  printf("%-40s => res=%d    ", s, res);
  print_decimal(&a);
  printf("\n");
}

void test_d2ull(char *s)
{
  char s1[100];
  ulonglong x;
  int res;

  string2decimal(s, &a, 0);
  res=decimal2ulonglong(&a, &x);
  if (full) dump_decimal(&a);
  longlong10_to_str(x,s1,10);
  printf("%-40s => res=%d    %s\n", s, res, s1);
}

void test_d2ll(char *s)
{
  char s1[100];
  longlong x;
  int res;

  string2decimal(s, &a, 0);
  res=decimal2longlong(&a, &x);
  if (full) dump_decimal(&a);
  longlong10_to_str(x,s1,-10);
  printf("%-40s => res=%d    %s\n", s, res, s1);
}

void test_da(char *s1, char *s2)
{
  char s[100];
  int res;
  sprintf(s, "'%s' + '%s'", s1, s2);
  string2decimal(s1, &a, 0);
  string2decimal(s2, &b, 0);
  res=decimal_add(&a, &b, &c);
  printf("%-40s => res=%d    ", s, res);
  print_decimal(&c);
  printf("\n");
}

void test_ds(char *s1, char *s2)
{
  char s[100];
  int res;
  sprintf(s, "'%s' - '%s'", s1, s2);
  string2decimal(s1, &a, 0);
  string2decimal(s2, &b, 0);
  res=decimal_sub(&a, &b, &c);
  printf("%-40s => res=%d    ", s, res);
  print_decimal(&c);
  printf("\n");
}

void test_dc(char *s1, char *s2)
{
  char s[100];
  int res;
  sprintf(s, "'%s' <=> '%s'", s1, s2);
  string2decimal(s1, &a, 0);
  string2decimal(s2, &b, 0);
  res=decimal_cmp(&a, &b);
  printf("%-40s => res=%d\n", s, res);
}

void test_dm(char *s1, char *s2)
{
  char s[100];
  int res;
  sprintf(s, "'%s' * '%s'", s1, s2);
  string2decimal(s1, &a, 0);
  string2decimal(s2, &b, 0);
  res=decimal_mul(&a, &b, &c);
  printf("%-40s => res=%d    ", s, res);
  print_decimal(&c);
  printf("\n");
}

void test_dv(char *s1, char *s2)
{
  char s[100];
  int res;
  sprintf(s, "'%s' / '%s'", s1, s2);
  string2decimal(s1, &a, 0);
  string2decimal(s2, &b, 0);
  res=decimal_div(&a, &b, &c, 5);
  printf("%-40s => res=%d    ", s, res);
  if (res == E_DEC_DIV_ZERO)
    printf("E_DEC_DIV_ZERO");
  else
    print_decimal(&c);
  printf("\n");
}

void test_md(char *s1, char *s2)
{
  char s[100];
  int res;
  sprintf(s, "'%s' %% '%s'", s1, s2);
  string2decimal(s1, &a, 0);
  string2decimal(s2, &b, 0);
  res=decimal_mod(&a, &b, &c);
  printf("%-40s => res=%d    ", s, res);
  if (res == E_DEC_DIV_ZERO)
    printf("E_DEC_DIV_ZERO");
  else
    print_decimal(&c);
  printf("\n");
}

void test_ro(char *s1, int n, decimal_round_mode mode)
{
  char s[100];
  int res;
  sprintf(s, "%s('%s', %d)", (mode == TRUNCATE ? "truncate" : "round"),
                             s1, n);
  string2decimal(s1, &a, 0);
  res=decimal_round(&a, &b, n, mode);
  printf("%-40s => res=%d    ", s, res);
  print_decimal(&b);
  printf("\n");
}

main()
{
  a.buf=(void*)buf1;
  a.len=sizeof(buf1)/sizeof(dec1);
  b.buf=(void*)buf2;
  b.len=sizeof(buf2)/sizeof(dec1);
  c.buf=(void*)buf3;
  c.len=sizeof(buf3)/sizeof(dec1);

  if (full)
    test_d2s();

  printf("==== string2decimal ====\n");
  test_s2d("12345");
  test_s2d("12345.");
  test_s2d("123.45");
  test_s2d("-123.45");
  test_s2d(".00012345000098765");
  test_s2d(".12345000098765");
  test_s2d("-.000000012345000098765");
  test_s2d("1234500009876.5");
  a.len=1;
  test_s2d("123450000098765");
  test_s2d("123450.000098765");
  a.len=sizeof(buf1)/sizeof(dec1);

  printf("==== decimal2double ====\n");
  test_d2f("12345");
  test_d2f("123.45");
  test_d2f("-123.45");
  test_d2f(".00012345000098765");
  test_d2f("1234500009876.5");

  printf("==== double2decimal ====\n");
  test_f2d(12345);
  test_f2d(1.0/3);
  test_f2d(-123.45);
  test_f2d(0.00012345000098765);
  test_f2d(1234500009876.5);

  printf("==== ulonglong2decimal ====\n");
  test_ull2d(ULL(12345));
  test_ull2d(ULL(0));
  test_ull2d(ULL(18446744073709551615));

  printf("==== decimal2ulonglong ====\n");
  test_d2ull("12345");
  test_d2ull("0");
  test_d2ull("18446744073709551615");
  test_d2ull("18446744073709551616");
  test_d2ull("-1");
  test_d2ull("1.23");

  printf("==== longlong2decimal ====\n");
  test_ll2d(LL(-12345));
  test_ll2d(LL(-1));
  test_ll2d(LL(-9223372036854775807));
  test_ll2d(ULL(9223372036854775808));

  printf("==== decimal2longlong ====\n");
  test_d2ll("18446744073709551615");
  test_d2ll("-1");
  test_d2ll("-1.23");
  test_d2ll("-9223372036854775807");
  test_d2ll("-9223372036854775808");
  test_d2ll("9223372036854775808");

  printf("==== do_add ====\n");
  test_da(".00012345000098765" ,"123.45");
  test_da(".1" ,".45");
  test_da("1234500009876.5" ,".00012345000098765");
  test_da("9999909999999.5" ,".555");
  test_da("99999999" ,"1");
  test_da("989999999" ,"1");
  test_da("999999999" ,"1");
  test_da("12345" ,"123.45");
  test_da("-12345" ,"-123.45");
  test_ds("-12345" ,"123.45");
  test_ds("12345" ,"-123.45");

  printf("==== do_sub ====\n");
  test_ds(".00012345000098765", "123.45");
  test_ds("1234500009876.5", ".00012345000098765");
  test_ds("9999900000000.5", ".555");
  test_ds("1111.5551", "1111.555");
  test_ds(".555", ".555");
  test_ds("10000000", "1");
  test_ds("1000001000", ".1");
  test_ds("1000000000", ".1");
  test_ds("12345", "123.45");
  test_ds("-12345", "-123.45");
  test_da("-12345", "123.45");
  test_da("12345", "-123.45");
  test_ds("123.45", "12345");
  test_ds("-123.45", "-12345");
  test_da("123.45", "-12345");
  test_da("-123.45", "12345");

  printf("==== decimal_mul ====\n");
  test_dm("12", "10");
  test_dm("-123.456", "98765.4321");
  test_dm("-123456000000", "98765432100000");
  test_dm("123456", "987654321");
  test_dm("123456", "9876543210");
  test_dm("123", "0.01");
  test_dm("123", "0");

  printf("==== decimal_div ====\n");
  test_dv("120", "10");
  test_dv("123", "0.01");
  test_dv("120", "100000000000.00000");
  test_dv("123", "0");
  test_dv("-12193185.1853376", "98765.4321");
  test_dv("121931851853376", "987654321");
  test_dv("0", "987");
  test_dv("1", "3");
  test_dv("1.000000000000", "3");
  test_dv("1", "1");
  test_dv("0.0123456789012345678912345", "9999999999");

  printf("==== decimal_round ====\n");
  test_ro("15.1",0,HALF_UP);
  test_ro("15.5",0,HALF_UP);
  test_ro("15.5",0,HALF_UP);
  test_ro("15.9",0,HALF_UP);
  test_ro("-15.1",0,HALF_UP);
  test_ro("-15.5",0,HALF_UP);
  test_ro("-15.9",0,HALF_UP);
  test_ro("15.1",1,HALF_UP);
  test_ro("-15.1",1,HALF_UP);
  test_ro("15.17",1,HALF_UP);
  test_ro("15.4",-1,HALF_UP);
  test_ro("-15.4",-1,HALF_UP);
  test_ro("5678.123451",-4,TRUNCATE);
  test_ro("5678.123451",-3,TRUNCATE);
  test_ro("5678.123451",-2,TRUNCATE);
  test_ro("5678.123451",-1,TRUNCATE);
  test_ro("5678.123451",0,TRUNCATE);
  test_ro("5678.123451",1,TRUNCATE);
  test_ro("5678.123451",2,TRUNCATE);
  test_ro("5678.123451",3,TRUNCATE);
  test_ro("5678.123451",4,TRUNCATE);
  test_ro("5678.123451",5,TRUNCATE);
  test_ro("5678.123451",6,TRUNCATE);
  test_ro("-5678.123451",-4,TRUNCATE);
  test_ro("99999999999999999999999999999999999999",-31,TRUNCATE);

  printf("==== decimal_mod ====\n");
  test_md("234","10");
  test_md("234.567","10.555");
  test_md("-234.567","10.555");
  test_md("234.567","-10.555");

  printf("==== decimal2bin/bin2decimal ====\n");
  test_d2b2d("12345", 5, 0);
  test_d2b2d("12345", 10, 3);
  test_d2b2d("123.45", 10, 3);
  test_d2b2d("-123.45", 20, 10);
  test_d2b2d(".00012345000098765", 15, 14);
  test_d2b2d(".00012345000098765", 22, 20);
  test_d2b2d(".12345000098765", 30, 20);
  test_d2b2d("-.000000012345000098765", 30, 20);
  test_d2b2d("1234500009876.5", 30, 5);

  printf("==== decimal_cmp ====\n");
  test_dc("12","13");
  test_dc("13","12");
  test_dc("-10","10");
  test_dc("10","-10");
  test_dc("-12","-13");
  test_dc("0","12");
  test_dc("-10","0");
  test_dc("4","4");
  return 0;
}
#endif
