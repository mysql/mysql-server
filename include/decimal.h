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
#include <m_ctype.h>
#include <my_sys.h> /* for my_alloca */

typedef enum {TRUNCATE=0, EVEN} dec_round_mode;
typedef uint32 decimal_digit;

typedef struct st_decimal {
  int     intg, frac, len;
  my_bool sign;
  decimal_digit *buf;
} decimal;

int decimal2string(decimal *from, char *to, uint *to_len);
int string2decimal(char *from, decimal *to, char **end);
int decimal2ulonglong(decimal *from, ulonglong *to);
int ulonglong2decimal(ulonglong from, decimal *to);
int decimal2longlong(decimal *from, longlong *to);
int longlong2decimal(longlong from, decimal *to);
int decimal2double(decimal *from, double *to);
int double2decimal(double from, decimal *to);

int decimal_add(decimal *from1, decimal *from2, decimal *to);
int decimal_sub(decimal *from1, decimal *from2, decimal *to);
int decimal_mul(decimal *from1, decimal *from2, decimal *to);
int decimal_div(decimal *from1, decimal *from2, decimal *to, int scale_incr);
int decimal_mod(decimal *from1, decimal *from2, decimal *to);
int decimal_result_size(decimal *from1, decimal *from2, char op, int param);
int decimal_round(decimal *dec, int new_scale, dec_round_mode mode);

/*
  conventions:

    decimal_smth() == 0     -- everything's ok
    decimal_smth() <= 0     -- result is usable, precision loss is possible
    decimal_smth() <= 1     -- result is unusable, but most significant digits
                               can be lost
    decimal_smth() >  1     -- no result was generated
*/

#define E_DEC_TRUNCATED        -1
#define E_DEC_OK                0
#define E_DEC_OVERFLOW          1
#define E_DEC_DIV_ZERO          2
#define E_DEC_BAD_NUM           3
#define E_DEC_OOM               4

#endif

