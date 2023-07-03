/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef SQL_JOIN_OPTIMIZER_COMMON_SUBEXPRESSION_ELIMINATION
#define SQL_JOIN_OPTIMIZER_COMMON_SUBEXPRESSION_ELIMINATION 1

class Item;

/**
  Do simple CSE (common subexpression elimination) on “item”, and return
  the answer. The CSE done is exclusively moving common expressions out
  of conjunctions-of-disjunctions, ie. it rewrites

    (a AND b) OR (a AND c)

  into

    a AND (b OR c)

  The primary motivation is that such split-out items are more versatile;
  they can be pushed independently, be made into hash join conditions etc.
  However, an added bonus is that the expressions will simply execute faster.

  This function does not descend into subexpressions that are not AND/OR
  conjunctions, so e.g. an expression like

    1 + ((a AND b) OR (a AND c))

  will be left as-is.
 */
Item *CommonSubexpressionElimination(Item *cond);

#endif  // SQL_JOIN_OPTIMIZER_COMMON_SUBEXPRESSION_ELIMINATION
