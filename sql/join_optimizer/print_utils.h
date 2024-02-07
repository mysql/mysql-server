/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef SQL_JOIN_OPTIMIZER_PRINT_UTILS
#define SQL_JOIN_OPTIMIZER_PRINT_UTILS 1

#include "my_compiler.h"
#include "sql/item.h"

#include <string>

struct RelationalExpression;

/**
  Like sprintf, but returns an std::string.
  This is not the most efficient of formatting functions,
  but it is only intended for debugging/tracing use.
 */
std::string StringPrintf(const char *fmt, ...)
    MY_ATTRIBUTE((format(printf, 1, 2)));

template <class T>
std::string ItemsToString(const T &items) {
  std::string result = "(none)";
  bool first = true;
  for (Item *item : items) {
    if (first) {
      result = ItemToString(item);
      first = false;
    } else {
      result += " AND ";
      result += ItemToString(item);
    }
  }
  return result;
}

std::string GenerateExpressionLabel(const RelationalExpression *expr);

/*
 These functions format a number such that it has reasonable precision
 without becoming so long that it is hard to read. This is used for
 EXPLAIN/EXPLAIN ANALYZE, and for describing access paths in optimizer
 trace.

 * Numbers in the range [0.001, 999999.5> are printed as decimal numbers.

 * All decimal numbers have three significant digits, except for numbers in
   the range [1000, 999999.5> that have four to six.

 * Numbers outside the range [0.001, 999999.5> are printed on engineering
   format, i.e. <mantissa>e<sign><exponent> where "mantissa" is a number in
   the range [1, 999], with three significant digits, and "exponent" is a
   multiple of three, e.g.: "1.23e+9" and "934e-6".

 * Trailing fractional zeros are not printed. For example, we print "2.3"
   rather than "2.30", and "1.2e+6" rather than "1.20e+6".

 * Numbers below 1e-12 are printed as "0".
*/
std::string FormatNumberReadably(double d);
std::string FormatNumberReadably(uint64_t l);

#endif  // SQL_JOIN_OPTIMIZER_PRINT_UTILS
