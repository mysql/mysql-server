/* Copyright (C) 2004 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/* number of digits on which we increase scale of devision result */
#define DECIMAL_DIV_SCALE_INCREASE 5

/* maximum length of buffer in our big digits (uint32) */
#define DECIMAL_BUFF_LENGTH 8
/*
  maximum guaranteed length of number in decimal digits (number of our
  digits * number of decimal digits in one our big digit - number of decimal
  digits in one our big digit decreased on 1 (because we always put decimal
  point on the border of our big digits))
*/
#define DECIMAL_MAX_LENGTH ((8 * 9) - 8)
/*
  maximum length of string representation (number of maximum decimal
  digits + 1 position for sign + 1 position for decimal point)
*/
#define DECIMAL_MAX_STR_LENGTH (DECIMAL_MAX_LENGTH + 2)
/*
  maximum size of packet length
*/
#define DECIMAL_MAX_FIELD_SIZE DECIMAL_MAX_LENGTH


inline uint my_decimal_size(uint precision, uint scale)
{
  /*
    Always allocate more space to allow library to put decimal point
    where it want
  */
  return decimal_size(precision, scale) + 1;
}


/*
  my_decimal class limits 'decimal' type to what we need in MySQL
  It contains internally all necessary space needed by the instance so
  no extra memory is needed. One should call fix_buffer_pointer() function
  when he moves my_decimal objects in memory
*/

class my_decimal :public decimal
{
  decimal_digit buffer[DECIMAL_BUFF_LENGTH];

public:

  void init()
  {
    len= DECIMAL_BUFF_LENGTH;
    buf= buffer;
#if !defined(HAVE_purify) && !defined(DBUG_OFF)
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

  bool sign() const { return decimal::sign; }
  void sign(bool s) { decimal::sign= s; }
};


#ifndef DBUG_OFF
void print_decimal(const my_decimal *dec);
void print_decimal_buff(const my_decimal *dec, const byte* ptr, int length);
void dbug_print_decimal(const char *tag, const char *format, my_decimal *val);
#else
#define dbug_print_decimal(A,B,C)
#endif

#ifndef MYSQL_CLIENT
int decimal_operation_results(int result);
#else
inline int decimal_operation_results(int result)
{
  return result;
}
#endif /*MYSQL_CLIENT*/

inline int check_result(uint mask, int result)
{
  if (result & mask)
    decimal_operation_results(result);
  return result;
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


int my_decimal2binary(uint mask, const my_decimal *d, byte *bin, int prec,
		      int scale);


inline
int binary2my_decimal(uint mask, const byte *bin, my_decimal *d, int prec,
		      int scale)
{
  return check_result(mask, bin2decimal((char *)bin, (decimal*) d, prec,
					scale));
}


inline
int my_decimal_set_zero(my_decimal *d)
{
  decimal_make_zero(((decimal*) d));
  return 0;
}


inline
bool my_decimal_is_zero(const my_decimal *decimal_value)
{
  return decimal_is_zero((decimal*) decimal_value);
}


inline
int my_decimal_round(uint mask, const my_decimal *from, int scale,
                     bool truncate, my_decimal *to)
{
  return check_result(mask, decimal_round((decimal*) from, to, scale,
					  (truncate ? TRUNCATE : HALF_UP)));
}


inline
int my_decimal_floor(uint mask, const my_decimal *from, my_decimal *to)
{
  return check_result(mask, decimal_round((decimal*) from, to, 0, FLOOR));
}


inline
int my_decimal_ceiling(uint mask, const my_decimal *from, my_decimal *to)
{
  return check_result(mask, decimal_round((decimal*) from, to, 0, CEILING));
}


#ifndef MYSQL_CLIENT
int my_decimal2string(uint mask, const my_decimal *d, int fixed_prec,
		      int fixed_dec, char filler, String *str);
#endif

inline
int my_decimal2int(uint mask, const my_decimal *d, my_bool unsigned_flag,
		   longlong *l)
{
  my_decimal rounded;
  /* decimal_round can return only E_DEC_TRUNCATED */
  decimal_round((decimal*)d, &rounded, 0, HALF_UP);
  return check_result(mask, (unsigned_flag ?
			     decimal2ulonglong(&rounded, (ulonglong *)l) :
			     decimal2longlong(&rounded, l)));
}


inline
int my_decimal2double(uint mask, const my_decimal *d, double *result)
{
  /* No need to call check_result as this will always succeed */
  return decimal2double((decimal*) d, result);
}


inline
int str2my_decimal(uint mask, const char *str, my_decimal *d, char **end)
{
  return check_result(mask, string2decimal(str, (decimal*) d, end));
}


int str2my_decimal(uint mask, const char *from, uint length,
                   CHARSET_INFO *charset, my_decimal *decimal_value);


#ifdef MYSQL_SERVER
inline
int string2my_decimal(uint mask, const String *str, my_decimal *d)
{
  return str2my_decimal(mask, str->ptr(), str->length(), str->charset(), d);
}
#endif


inline
int double2my_decimal(uint mask, double val, my_decimal *d)
{
  return check_result(mask, double2decimal(val, (decimal*) d));
}


inline
int int2my_decimal(uint mask, longlong i, my_bool unsigned_flag, my_decimal *d)
{
  return check_result(mask, (unsigned_flag ?
			     ulonglong2decimal((ulonglong)i, d) :
			     longlong2decimal(i, d)));
}


inline
void my_decimal_neg(st_decimal *arg)
{
  decimal_neg(arg);
}


inline
int my_decimal_add(uint mask, my_decimal *res, const my_decimal *a,
		   const my_decimal *b)
{
  return check_result(mask, decimal_add((decimal*) a, (decimal*) b, res));
}


inline
int my_decimal_sub(uint mask, my_decimal *res, const my_decimal *a,
		   const my_decimal *b)
{
  return check_result(mask, decimal_sub((decimal*) a, (decimal*) b, res));
}


inline
int my_decimal_mul(uint mask, my_decimal *res, const my_decimal *a,
		   const my_decimal *b)
{
  return check_result(mask, decimal_mul((decimal*) a, (decimal*) b, res));
}


inline
int my_decimal_div(uint mask, my_decimal *res, const my_decimal *a,
		   const my_decimal *b, int div_scale_inc)
{
  return check_result(mask, decimal_div((decimal*) a, (decimal*) b, res,
					div_scale_inc));
}


inline
int my_decimal_mod(uint mask, my_decimal *res, const my_decimal *a,
		   const my_decimal *b)
{
  return check_result(mask, decimal_mod((decimal*) a, (decimal*) b, res));
}


/* Returns -1 if a<b, 1 if a>b and 0 if a==b */
inline
int my_decimal_cmp(const my_decimal *a, const my_decimal *b)
{
  return decimal_cmp((decimal*) a, (decimal*) b);
}

inline
void max_my_decimal(my_decimal *to, int precision, int frac)
{
  DBUG_ASSERT(precision <= DECIMAL_MAX_LENGTH);
  max_decimal(precision, frac, (decimal*) to);
}

#endif /*my_decimal_h*/

