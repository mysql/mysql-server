/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/join_optimizer/print_utils.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <algorithm>
#include <string>
#include <vector>

#include "sql/item_cmpfunc.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/mem_root_array.h"

using std::string;
using std::vector;

std::string StringPrintf(const char *fmt, ...) {
  std::string result;
  char buf[256];

  va_list ap;
  va_start(ap, fmt);
  int bytes_needed = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (bytes_needed >= static_cast<int>(sizeof(buf))) {
    // Too large for our stack buffer, so we need to allocate.
    result.resize(bytes_needed + 1);
    va_start(ap, fmt);
    vsnprintf(&result[0], bytes_needed + 1, fmt, ap);
    va_end(ap);
    result.resize(bytes_needed);
  } else {
    result.assign(buf, bytes_needed);
  }
  return result;
}

std::string GenerateExpressionLabel(const RelationalExpression *expr) {
  vector<Item *> all_join_conditions;
  all_join_conditions.insert(all_join_conditions.end(),
                             expr->equijoin_conditions.begin(),
                             expr->equijoin_conditions.end());
  all_join_conditions.insert(all_join_conditions.end(),
                             expr->join_conditions.begin(),
                             expr->join_conditions.end());

  string label = ItemsToString(all_join_conditions);
  switch (expr->type) {
    case RelationalExpression::MULTI_INNER_JOIN:
    case RelationalExpression::TABLE:
      assert(false);
    case RelationalExpression::INNER_JOIN:
    case RelationalExpression::STRAIGHT_INNER_JOIN:
      break;
    case RelationalExpression::LEFT_JOIN:
      label = "[left] " + label;
      break;
    case RelationalExpression::SEMIJOIN:
      label = "[semi] " + label;
      break;
    case RelationalExpression::ANTIJOIN:
      label = "[anti] " + label;
      break;
    case RelationalExpression::FULL_OUTER_JOIN:
      label = "[full] " + label;
      break;
  }
  return label;
}
