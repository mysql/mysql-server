/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.

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

#ifndef SQL_OPTIMIZER_INTERNAL_INCLUDED
#define SQL_OPTIMIZER_INTERNAL_INCLUDED

#include "sql/sql_optimizer.h"

#include "my_inttypes.h"
#include "sql/item.h"

class JOIN;
class THD;

struct SARGABLE_PARAM;

/**
  @defgroup RefOptimizerModule Ref Optimizer

  @{

  This module analyzes all equality predicates to determine the best
  independent ref/eq_ref/ref_or_null index access methods.

  The 'ref' optimizer determines the columns (and expressions over them) that
  reference columns in other tables via an equality, and analyzes which keys
  and key parts can be used for index lookup based on these references. The
  main outcomes of the 'ref' optimizer are:

  - A bi-directional graph of all equi-join conditions represented as an
    array of Key_use elements. This array is stored in JOIN::keyuse_array in
    table, key, keypart order. Each JOIN_TAB::keyuse points to the
    first Key_use element with the same table as JOIN_TAB::table.

  - The table dependencies needed by the optimizer to determine what
    tables must be before certain table so that they provide the
    necessary column bindings for the equality predicates.

  - Computed properties of the equality predicates such as null_rejecting
    and the result size of each separate condition.

  Updates in JOIN_TAB:
  - JOIN_TAB::keys       Bitmap of all used keys.
  - JOIN_TAB::const_keys Bitmap of all keys that may be used with quick_select.
  - JOIN_TAB::keyuse     Pointer to possible keys.
*/

/**
  A Key_field is a descriptor of a predicate of the form (column @<op@> val).
  Currently 'op' is one of {'=', '<=>', 'IS [NOT] NULL', 'arg1 IN arg2'},
  and 'val' can be either another column or an expression (including constants).

  Key_field's are used to analyze columns that may potentially serve as
  parts of keys for index lookup. If 'field' is part of an index, then
  add_key_part() creates a corresponding Key_use object and inserts it
  into the JOIN::keyuse_array which is passed by update_ref_and_keys().

  The structure is used only during analysis of the candidate columns for
  index 'ref' access.
*/
struct Key_field {
  Key_field(Item_field *item_field, Item *val, uint level, uint optimize,
            bool eq_func, bool null_rejecting, bool *cond_guard,
            uint sj_pred_no)
      : item_field(item_field),
        val(val),
        level(level),
        optimize(optimize),
        eq_func(eq_func),
        null_rejecting(null_rejecting),
        cond_guard(cond_guard),
        sj_pred_no(sj_pred_no) {}
  Item_field *item_field;  ///< Item representing the column
  Item *val;               ///< May be empty if diff constant
  uint level;
  uint optimize;  ///< KEY_OPTIMIZE_*
  bool eq_func;
  /**
    If true, the condition this struct represents will not be satisfied
    when val IS NULL.
    @sa Key_use::null_rejecting .
  */
  bool null_rejecting;
  bool *cond_guard;  ///< @sa Key_use::cond_guard
  uint sj_pred_no;   ///< @sa Key_use::sj_pred_no
};

bool add_key_fields(THD *thd, JOIN *join, Key_field **key_fields,
                    uint *and_level, Item *cond, table_map usable_tables,
                    SARGABLE_PARAM **sargables);

#endif  // SQL_OPTIMIZER_INTERNAL_INCLUDED
