/* Copyright (c) 2020, Oracle and/or its affiliates.

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

#include "sql/join_optimizer/estimate_selectivity.h"

#include <sys/types.h>
#include <algorithm>
#include <initializer_list>
#include <string>

#include "my_bitmap.h"
#include "my_table_map.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/print_utils.h"
#include "sql/key.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_const.h"
#include "sql/table.h"
#include "template_utils.h"

using std::string;

/**
  Estimate the selectivity of (equi)joining a given field to any other field
  using cardinality information from indexes, if possible. Assumes equal
  distribution and zero correlation between the two fields, so if there are
  e.g. 100 records and 4 distinct values (A,B,C,D) for the field, it assumes
  25% of the values will be A, 25% B, etc. (equal distribution), and thus,
  when joining a row from some other table against this one, 25% of the records
  will match (equal distribution, zero correlation).

  Returns -1.0 if no index was found. Lifted from
  Item_equal::get_filtering_effect.
 */
static double EstimateFieldSelectivity(Field *field) {
  const TABLE *table = field->table;
  for (uint j = 0; j < table->s->keys; j++) {
    if (field->key_start.is_set(j) &&
        table->key_info[j].has_records_per_key(0)) {
      double selectivity =
          table->key_info[j].records_per_key(0) / table->file->stats.records;

      /*
        Since rec_per_key and rows_per_table are calculated at
        different times, their values may not be in synch and thus
        it is possible that cur_filter is greater than 1.0 if
        rec_per_key is outdated. Force the filter to 1.0 in such
        cases.
       */
      return std::min(selectivity, 1.0);
    }
  }
  return -1.0;
}

/**
  For the given condition, to try estimate its filtering selectivity,
  on a 0..1 scale (where 1.0 lets all records through).
 */
double EstimateSelectivity(THD *thd, Item *condition, string *trace) {
  // If the item is a true constant, we can say immediately whether it passes
  // or filters all rows. (Actually, calling get_filtering_effect() below
  // would crash if used_tables() is zero, which it is for const items.)
  if (condition->const_item()) {
    return (condition->val_int() != 0) ? 1.0 : 0.0;
  }

  // For field = field (e.g. t1.x = t2.y), we try to use index information
  // to find a better selectivity estimate. We look for indexes on both
  // fields. If there are multiple ones, we arbitrarily choose the first one;
  // we have no reason to believe that one is better than the other.
  // (This means that t1.x = t2.y might get a different estimate from
  // t2.y = t1.x, but at least we call EstimateSelectivity() only once per
  // is only called once per join edge.)
  //
  // For get_filtering_effect(), there is similar code in Item_func_equal,
  // but as we currently don't support multiple equalities in the hypergraph
  // join optimizer, it will never be called.
  if (condition->type() == Item::FUNC_ITEM &&
      down_cast<Item_func *>(condition)->functype() == Item_func::EQ_FUNC) {
    Item_func_eq *eq = down_cast<Item_func_eq *>(condition);
    Item *left = eq->arguments()[0];
    Item *right = eq->arguments()[1];
    if (left->type() == Item::FIELD_ITEM && right->type() == Item::FIELD_ITEM) {
      for (Field *field : {down_cast<Item_field *>(left)->field,
                           down_cast<Item_field *>(right)->field}) {
        double field_selectivity = EstimateFieldSelectivity(field);
        if (field_selectivity >= 0.0) {
          if (trace != nullptr) {
            *trace += StringPrintf(
                " - found an index in %s.%s for %s, selectivity = %.3f\n",
                field->table->alias, field->field_name,
                ItemToString(condition).c_str(), field_selectivity);
          }
          return field_selectivity;
        }
      }
    }
  }

  // No such thing, so use Item::get_filtering_effect().
  //
  // There is a challenge in that the Item::get_filtering_effect() API
  // is intrinsically locked to the old join optimizer's way of thinking,
  // where one made a long chain of (left-deep) nested tables, and selectivity
  // estimation would be run for the entire WHERE condition at all points
  // in that chain. In such a situation, it would be neccessary to know which
  // tables were already in the chain and which would not, and multiple
  // equalities would also be resolved through this mechanism. In the hypergraph
  // optimizer, we no longer have a chain, and always estimate selectivity for
  // applicable conditions only; thus, we need to fake that chain for the API.
  table_map used_tables = condition->used_tables() & ~PSEUDO_TABLE_BITS;
  table_map this_table = IsolateLowestBit(used_tables);
  MY_BITMAP empty;
  my_bitmap_map bitbuf[bitmap_buffer_size(MAX_FIELDS) / sizeof(my_bitmap_map)];
  bitmap_init(&empty, bitbuf, MAX_FIELDS);
  double selectivity = condition->get_filtering_effect(
      thd, this_table, used_tables & ~this_table,
      /*fields_to_ignore=*/&empty,
      /*rows_in_table=*/1000.0);

  if (trace != nullptr) {
    *trace += StringPrintf(" - fallback selectivity for %s = %.3f\n",
                           ItemToString(condition).c_str(), selectivity);
  }
  return selectivity;
}
