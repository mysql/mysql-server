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

#ifndef _decimal_h
#define _decimal_h

#include <my_global.h>

typedef enum {TRUNCATE=0, EVEN} dec_round_mode;
typedef int32 decimal_digit;

typedef struct st_decimal {
  int    intg, frac, len;
  my_bool sign;
  decimal_digit *buf;
} decimal;

int decimal2string(decimal *from, char *to, int *to_len);
int string2decimal(char *from, decimal *to, char **end);
int string2decimal_fixed(char *from, decimal *to, char **end);
int decimal2ulonglong(decimal *from, ulonglong *to);
int ulonglong2decimal(ulonglong from, decimal *to);
int decimal2longlong(decimal *from, longlong *to);
int longlong2decimal(longlong from, decimal *to);
int decimal2double(decimal *from, double *to);
int double2decimal(double from, decimal *to);
int decimal2bin(decimal *from, char *to, int precision, int scale);
int bin2decimal(char *from, decimal *to, int precision, int scale);

int decimal_size(int precision, int scale);
int decimal_bin_size(int precision, int scale);
int decimal_result_size(decimal *from1, decimal *from2, char op, int param);

int decimal_add(decimal *from1, decimal *from2, decimal *to);
int decimal_sub(decimal *from1, decimal *from2, decimal *to);
int decimal_cmp(decimal *from1, decimal *from2);
int decimal_mul(decimal *from1, decimal *from2, decimal *to);
int decimal_div(decimal *from1, decimal *from2, decimal *to, int scale_incr);
int decimal_mod(decimal *from1, decimal *from2, decimal *to);
int decimal_round(decimal *from, decimal *to, int new_scale, dec_round_mode mode);

/*
  the following works only on special "zero" decimal, not on any
  decimal that happen to evaluate to zero
*/

#define decimal_is_zero(dec) ((dec)->intg1==1 && (dec)->frac1==0 && (dec)->buf[0]==0)

/* set a decimal to zero */

#define decimal_make_zero(dec)        do {                \
                                        (dec)->buf[0]=0;    \
                                        (dec)->intg=1;      \
                                        (dec)->frac=0;      \
                                        (dec)->sign=0;      \
                                      } while(0)

/*
  returns the length of the buffer to hold string representation
  of the decimal
*/

#define decimal_string_size(dec) ((dec)->intg + (dec)->frac + ((dec)->frac > 0) + 1)

/* negate a decimal */
#define decimal_neg(dec) do { (dec)->sign^=1; } while(0)

/*
  conventions:

    decimal_smth() == 0     -- everything's ok
    decimal_smth() <= 1     -- result is usable, but precision loss is possible
    decimal_smth() <= 2     -- result can be unusable, most significant digits
                               could've been lost
    decimal_smth() >  2     -- no result was generated
*/

#define E_DEC_OK                0
#define E_DEC_TRUNCATED         1
#define E_DEC_OVERFLOW          2
#define E_DEC_DIV_ZERO          3
#define E_DEC_BAD_NUM           4
#define E_DEC_OOM               5

#endif

