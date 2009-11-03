/* Copyright (C) 2005-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/**
  @file

  It is interface module to fixed precision decimals library.

  Most functions use 'uint mask' as parameter, if during operation error
  which fit in this mask is detected then it will be processed automatically
  here. (errors are E_DEC_* constants, see include/decimal.h)

  Most function are just inline wrappers around library calls
*/

#ifndef my_decimal_h
#define my_decimal_h

C_MODE_START
#include <decimal.h>
C_MODE_END

#define DECIMAL_LONGLONG_DIGITS 22
#define DECIMAL_LONG_DIGITS 10
#define DECIMAL_LONG3_DIGITS 8

/** maximum length of buffer in our big digits (uint32). */
#define DECIMAL_BUFF_LENGTH 9

/* the number of digits that my_decimal can possibly contain */
#define DECIMAL_MAX_POSSIBLE_PRECISION (DECIMAL_BUFF_LENGTH * 9)


/**
  maximum guaranteed precision of number in decimal digits (number of our
  digits * number of decimal digits in one our big digit - number of decimal
  digits in one our big digit decreased by 1 (because we always put decimal
  point on the border of our big digits))
*/
#define DECIMAL_MAX_PRECISION (DECIMAL_MAX_POSSIBLE_PRECISION - 8*2)
#define DECIMAL_MAX_SCALE 30
#define DECIMAL_NOT_SPECIFIED 31

/**
  maximum length of string representation (number of maximum decimal
  digits + 1 position for sign + 1 position for decimal point)
*/
#define DECIMAL_MAX_STR_LENGTH (DECIMAL_MAX_POSSIBLE_PRECISION + 2)

/**
  maximum size of packet length.
*/
#define DECIMAL_MAX_FIELD_SIZE DECIMAL_MAX_PRECISION


inline uint my_decimal_size(uint precision, uint scale)
{
  /*
    Always allocate more space to allow library to put decimal point
    where it want
  */
  return decimal_size(precision, scale) + 1;
}


inline int my_decimal_int_part(uint precision, uint decimals)
{
  return precision - ((decimals == DECIMAL_NOT_SPECIFIED) ? 0 : decimals);
}


/**
  my_decimal class limits 'decimal_t' type to what we need in MySQL.

  It contains internally all necessary space needed by the instance so
  no extra memory is needed. One should call fix_buffer_pointer() function
  when he moves my_decimal objects in memory.
*/

class my_decimal :public decimal_t
{
  decimal_digit_t buffer[DECIMAL_BUFF_LENGTH];

public:

  void init()
  {
    len= DECIMAL_BUFF_LENGTH;
    buf= buffer;
#if !defined (HAVE_purify) && !defined(DBUG_OFF)
    /* Set buffer to 'random' value to find wrong buffer usage */
    for (uint i= 0; i < DECIMAL_BUFF_LENGTH; i++)
      buffer[i]= i;
#endif
  }
  my_decimal()
  {
    init();
  }
  void fix_buffer_pointer() { buf= buffer; }

  bool sign() const { return decimal_t::sign; }
  void sign(bool s) { decimal_t::sign= s; }
  uint precision() const { return intg + frac; }

  /** Swap two my_decimal values */
  void swap(my_decimal &rhs)
  {
    swap_variables(my_decimal, *this, rhs);
    /* Swap the buffer pointers back */
    swap_variables(decimal_digit_t *, buf, rhs.buf);
  }
};


#ifndef DBUG_OFF
void print_decimal(const my_decimal *dec);
void print_decimal_buff(const my_decimal *dec, const uchar* ptr, int length);
const char *dbug_decimal_as_string(char *buff, const my_decimal *val);
#else
#define dbug_decimal_as_string(A) NULL
#endif

#ifndef MYSQL_CLIENT
int decimal_operation_results(int result);
#else
inline int decimal_operation_results(int result)
{
  return result;
}
#endif /*MYSQL_CLIENT*/

inline
void max_my_decimal(my_decimal *to, int precision, int frac)
{
  DBUG_ASSERT((precision <= DECIMAL_MAX_PRECISION)&&
              (frac <= DECIMAL_MAX_SCALE));
  max_decimal(precision, frac, (decimal_t*) to);
}

inline void max_internal_decimal(my_decimal *to)
{
  max_my_decimal(to, DECIMAL_MAX_PRECISION, 0);
}

inline int check_result(uint mask, int result)
{
  if (result & mask)
    decimal_operation_results(result);
  return result;
}

inline int check_result_and_overflow(uint mask, int result, my_decimal *val)
{
  if (check_result(mask, result) & E_DEC_OVERFLOW)
  {
    bool sign= val->sign();
    val->fix_buffer_pointer();
    max_internal_decimal(val);
    val->sign(sign);
  }
  return result;
}

inline uint my_decimal_length_to_precision(uint length, uint scale,
                                           bool unsigned_flag)
{
  /* Precision can't be negative thus ignore unsigned_flag when length is 0. */
  DBUG_ASSERT(length || !scale);
  return (uint) (length - (scale>0 ? 1:0) -
                 (unsigned_flag || !length ? 0:1));
}

inline uint32 my_decimal_precision_to_length_no_truncation(uint precision,
                                                           uint8 scale,
                                                           bool unsigned_flag)
{
  /*
    When precision is 0 it means that original length was also 0. Thus
    unsigned_flag is ignored in this case.
  */
  DBUG_ASSERT(precision || !scale);
  return (uint32)(precision + (scale > 0 ? 1 : 0) +
                  (unsigned_flag || !precision ? 0 : 1));
}

inline uint32 my_decimal_precision_to_length(uint precision, uint8 scale,
                                             bool unsigned_flag)
{
  /*
    When precision is 0 it means that original length was also 0. Thus
    unsigned_flag is ignored in this case.
  */
  DBUG_ASSERT(precision || !scale);
  set_if_smaller(precision, DECIMAL_MAX_PRECISION);
  return my_decimal_precision_to_length_no_truncation(precision, scale,
                                                      unsigned_flag);
}

inline
int my_decimal_string_length(const my_decimal *d)
{
  return decimal_string_size(d);
}


inline
int my_decimal_max_length(const my_decimal *d)
{
  /* -1 because we do not count \0 */
  return decimal_string_size(d) - 1;
}


inline
int my_decimal_get_binary_size(uint precision, uint scale)
{
  return decimal_bin_size((int)precision, (int)scale);
}


inline
void my_decimal2decimal(const my_decimal *from, my_decimal *to)
{
  *to= *from;
  to->fix_buffer_pointer();
}


int my_decimal2binary(uint mask, const my_decimal *d, uchar *bin, int prec,
		      int scale);


inline
int binary2my_decimal(uint mask, const uchar *bin, my_decimal *d, int prec,
		      int scale)
{
  return check_result(mask, bin2decimal(bin, (decimal_t*) d, prec, scale));
}


inline
int my_decimal_set_zero(my_decimal *d)
{
  decimal_make_zero(((decimal_t*) d));
  return 0;
}


inline
bool my_decimal_is_zero(const my_decimal *decimal_value)
{
  return decimal_is_zero((decimal_t*) decimal_value);
}


inline
int my_decimal_round(uint mask, const my_decimal *from, int scale,
                     bool truncate, my_decimal *to)
{
  return check_result(mask, decimal_round((decimal_t*) from, to, scale,
					  (truncate ? TRUNCATE : HALF_UP)));
}


inline
int my_decimal_floor(uint mask, const my_decimal *from, my_decimal *to)
{
  return check_result(mask, decimal_round((decimal_t*) from, to, 0, FLOOR));
}


inline
int my_decimal_ceiling(uint mask, const my_decimal *from, my_decimal *to)
{
  return check_result(mask, decimal_round((decimal_t*) from, to, 0, CEILING));
}


#ifndef MYSQL_CLIENT
int my_decimal2string(uint mask, const my_decimal *d, uint fixed_prec,
		      uint fixed_dec, char filler, String *str);
#endif

inline
int my_decimal2int(uint mask, const my_decimal *d, my_bool unsigned_flag,
		   longlong *l)
{
  my_decimal rounded;
  /* decimal_round can return only E_DEC_TRUNCATED */
  decimal_round((decimal_t*)d, &rounded, 0, HALF_UP);
  return check_result(mask, (unsigned_flag ?
			     decimal2ulonglong(&rounded, (ulonglong *)l) :
			     decimal2longlong(&rounded, l)));
}


inline
int my_decimal2double(uint mask, const my_decimal *d, double *result)
{
  /* No need to call check_result as this will always succeed */
  return decimal2double((decimal_t*) d, result);
}


inline
int str2my_decimal(uint mask, const char *str, my_decimal *d, char **end)
{
  return check_result_and_overflow(mask, string2decimal(str,(decimal_t*)d,end),
                                   d);
}


int str2my_decimal(uint mask, const char *from, uint length,
                   CHARSET_INFO *charset, my_decimal *decimal_value);

#if defined(MYSQL_SERVER) || defined(EMBEDDED_LIBRARY)
inline
int string2my_decimal(uint mask, const String *str, my_decimal *d)
{
  return str2my_decimal(mask, str->ptr(), str->length(), str->charset(), d);
}


my_decimal *date2my_decimal(MYSQL_TIME *ltime, my_decimal *dec);


#endif /*defined(MYSQL_SERVER) || defined(EMBEDDED_LIBRARY) */

inline
int double2my_decimal(uint mask, double val, my_decimal *d)
{
  return check_result_and_overflow(mask, double2decimal(val, (decimal_t*)d), d);
}


inline
int int2my_decimal(uint mask, longlong i, my_bool unsigned_flag, my_decimal *d)
{
  return check_result(mask, (unsigned_flag ?
			     ulonglong2decimal((ulonglong)i, d) :
			     longlong2decimal(i, d)));
}


inline
void my_decimal_neg(decimal_t *arg)
{
  if (decimal_is_zero(arg))
  {
    arg->sign= 0;
    return;
  }
  decimal_neg(arg);
}


inline
int my_decimal_add(uint mask, my_decimal *res, const my_decimal *a,
		   const my_decimal *b)
{
  return check_result_and_overflow(mask,
                                   decimal_add((decimal_t*)a,(decimal_t*)b,res),
                                   res);
}


inline
int my_decimal_sub(uint mask, my_decimal *res, const my_decimal *a,
		   const my_decimal *b)
{
  return check_result_and_overflow(mask,
                                   decimal_sub((decimal_t*)a,(decimal_t*)b,res),
                                   res);
}


inline
int my_decimal_mul(uint mask, my_decimal *res, const my_decimal *a,
		   const my_decimal *b)
{
  return check_result_and_overflow(mask,
                                   decimal_mul((decimal_t*)a,(decimal_t*)b,res),
                                   res);
}


inline
int my_decimal_div(uint mask, my_decimal *res, const my_decimal *a,
		   const my_decimal *b, int div_scale_inc)
{
  return check_result_and_overflow(mask,
                                   decimal_div((decimal_t*)a,(decimal_t*)b,res,
                                               div_scale_inc),
                                   res);
}


inline
int my_decimal_mod(uint mask, my_decimal *res, const my_decimal *a,
		   const my_decimal *b)
{
  return check_result_and_overflow(mask,
                                   decimal_mod((decimal_t*)a,(decimal_t*)b,res),
                                   res);
}


/**
  @return
    -1 if a<b, 1 if a>b and 0 if a==b
*/
inline
int my_decimal_cmp(const my_decimal *a, const my_decimal *b)
{
  return decimal_cmp((decimal_t*) a, (decimal_t*) b);
}


inline
int my_decimal_intg(const my_decimal *a)
{
  return decimal_intg((decimal_t*) a);
}


void my_decimal_trim(ulong *precision, uint *scale);


#endif /*my_decimal_h*/

