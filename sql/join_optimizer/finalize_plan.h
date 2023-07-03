/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef SQL_JOIN_OPTIMIZER_FINALIZE_PLAN_H_
#define SQL_JOIN_OPTIMIZER_FINALIZE_PLAN_H_

#include "sql/mem_root_array.h"

struct AccessPath;
class Func_ptr;
struct ORDER;
class Query_block;
class THD;

using Func_ptr_array = Mem_root_array<Func_ptr>;

// See comment in .cc file.
bool FinalizePlanForQueryBlock(THD *thd, Query_block *query_block,
                               AccessPath *root_path);

// Change all items in the ORDER list to point to the temporary table.
// This isn't important for streaming (the items would get the correct
// value anyway -- although possibly with some extra calculations),
// but it is for materialization.
void ReplaceOrderItemsWithTempTableFields(THD *thd, ORDER *order,
                                          const Func_ptr_array &items_to_copy);

#endif  // SQL_JOIN_OPTIMIZER_FINALIZE_PLAN_H_
