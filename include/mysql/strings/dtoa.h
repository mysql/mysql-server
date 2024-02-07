/*
   Copyright (c) 2021, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef INCLUDE_MYSQL_STRINGS_DTOA_H_
#define INCLUDE_MYSQL_STRINGS_DTOA_H_

#include <cfloat>
#include <cstddef>

#include "mysql/strings/api.h"

/*
  We want to use the 'e' format in some cases even if we have enough space
  for the 'f' one just to mimic sprintf("%.15g") behavior for large integers,
  and to improve it for numbers < 10^(-4).
  That is, for |x| < 1 we require |x| >= 10^(-15), and for |x| > 1 we require
  it to be integer and be <= 10^DBL_DIG for the 'f' format to be used.
  We don't lose precision, but make cases like "1e200" or "0.00001" look nicer.
*/
#define MAX_DECPT_FOR_F_FORMAT DBL_DIG

/*
  The maximum possible field width for my_gcvt() conversion.
  (DBL_DIG + 2) significant digits + sign + "." + ("e-NNN" or
  MAX_DECPT_FOR_F_FORMAT zeros for cases when |x|<1 and the 'f' format is used).
*/
#define MY_GCVT_MAX_FIELD_WIDTH \
  (DBL_DIG + 4 + std::max(5, MAX_DECPT_FOR_F_FORMAT))

typedef enum { MY_GCVT_ARG_FLOAT, MY_GCVT_ARG_DOUBLE } my_gcvt_arg_type;

static constexpr int DECIMAL_MAX_SCALE{30};
static constexpr int DECIMAL_NOT_SPECIFIED{DECIMAL_MAX_SCALE + 1};

/*
  The longest string my_fcvt can return is 311 + "precision" bytes.
  Here we assume that we never call my_fcvt() with precision >=
  DECIMAL_NOT_SPECIFIED
  (+ 1 byte for the terminating '\0').
*/
static constexpr int FLOATING_POINT_BUFFER{311 + DECIMAL_NOT_SPECIFIED};

/* Conversion routines */

MYSQL_STRINGS_EXPORT double my_strtod(const char *str, const char **end,
                                      int *error);
MYSQL_STRINGS_EXPORT size_t my_fcvt(double x, int precision, char *to,
                                    bool *error);
MYSQL_STRINGS_EXPORT size_t my_fcvt_compact(double x, char *to, bool *error);
MYSQL_STRINGS_EXPORT size_t my_gcvt(double x, my_gcvt_arg_type type, int width,
                                    char *to, bool *error);

#endif  // INCLUDE_MYSQL_STRINGS_DTOA_H_
