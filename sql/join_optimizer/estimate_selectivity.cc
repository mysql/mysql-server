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

#include "sql/join_optimizer/estimate_selectivity.h"

#include <sys/types.h>
#include <algorithm>
#include <initializer_list>
#include <string>

#include "my_bitmap.h"
#include "my_table_map.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/histograms/histogram.h"
#include "sql/item.h"
#include "sql/item_cmpfunc.h"
#include "sql/item_func.h"
#include "sql/join_optimizer/bit_utils.h"
#include "sql/join_optimizer/print_utils.h"
#include "sql/key.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_const.h"
#include "sql/sql_select.h"
#include "sql/table.h"
#include "template_utils.h"

using std::string;

/**
  Estimate the selectivity of (equi)joining a given field to any other
  field. Use cardinality information from indexes, if possible.
  Otherwise, use a histogram if there is one. Assumes equal
  distribution and zero correlation between the two fields, so if
  there are e.g. 100 records and 4 distinct values (A,B,C,D) for the
  field, it assumes 25% of the values will be A, 25% B, etc. (equal
  distribution), and thus, when joining a row from some other table
  against this one, 25% of the records will match (equal distribution,
  zero correlation).

  If there are multiple ones, we choose the one with the largest
  selectivity (least selective). There are two main reasons for this:

   - Databases generally tend to underestimate join cardinality
     (due to assuming uncorrelated relations); if we're wrong, it would
     better be towards overestimation to try to compensate.
   - Overestimating the number of rows generally leads to safer choices
     that are a little slower for few rows (e.g., hash join).
     Underestimating, however, leads to choices that can be catastrophic
     for many rows (e.g., nested loop against table scans). We should
     clearly prefer the least risky choice here.

  Returns -1.0 if no index or no histogram was found. Lifted from
  Item_equal::get_filtering_effect.
 */
static double EstimateFieldSelectivity(Field *field, double *selectivity_cap,
                                       string *trace) {
  const TABLE *table = field->table;
  double selectivity = -1.0;
  for (uint j = 0; j < table->s->keys; j++) {
    KEY *key = &table->key_info[j];

    if (field->key_start.is_set(j)) {
      if (key->has_records_per_key(0)) {
        double field_selectivity =
            static_cast<double>(table->key_info[j].records_per_key(0)) /
            table->file->stats.records;
        if (trace != nullptr) {
          *trace +=
              StringPrintf(" - found candidate index %s with selectivity %f\n",
                           table->key_info[j].name, field_selectivity);
        }
        selectivity = std::max(selectivity, field_selectivity);
      }

      // This is a less precise version of the single-row check in
      // CostingReceiver::ProposeRefAccess(). If true, we know that this index
      // can at most have selectivity 1/N, and we can use that as a global cap.
      // Importantly, unlike the capping in the EQ_REF code, this capping is
      // consistent between nested-loop index plans and hash join. Ideally, we'd
      // also support multi-predicate selectivities here and get rid of the
      // entire EQ_REF-specific code, but that requires a more holistic
      // selectivity handling (for multipart indexes) and pulling out some of
      // the sargable code for precise detection of null-rejecting predicates.
      //
      // Note that since we're called only for field = field here, which
      // is null-rejecting, we don't have a check for HA_NULL_PART_KEY.
      const bool single_row = Overlaps(actual_key_flags(key), HA_NOSAME) &&
                              key->actual_key_parts == 1;
      if (single_row) {
        if (trace != nullptr) {
          *trace += StringPrintf(
              " - capping selectivity to %f since index is unique\n",
              1.0 / table->file->stats.records);
        }
        *selectivity_cap =
            std::min(*selectivity_cap, 1.0 / table->file->stats.records);
      }
    }
  }

  // Look for a histogram if there was no suitable index.
  if (selectivity == -1.0) {
    const histograms::Histogram *const histogram =
        field->table->s->find_histogram(field->field_index());

    if (histogram != nullptr) {
      /*
        Assume that we do "SELECT ... FROM ... WHERE tab.field=<expression>".
        And there is a histogram on 'tab.field' indicating that there are
        N distinct values for that field. Then we estimate the selectivity
        to be 1/N.
      */
      const double distinct_values = histogram->get_num_distinct_values();
      selectivity = 1.0 / std::max(1.0, distinct_values);

      if (trace != nullptr) {
        *trace += StringPrintf(
            " - estimating selectivity %f for field '%s.%s'"
            " from histogram showing %.1f distinct values.\n",
            selectivity, field->table->alias, field->field_name,
            distinct_values);
      }
    }
  }

  /*
    Since rec_per_key and rows_per_table are calculated at
    different times, their values may not be in synch and thus
    it is possible that selectivity is greater than 1.0 if
    rec_per_key is outdated. Force the filter to 1.0 in such
    cases.
   */
  return std::min(selectivity, 1.0);
}

/**
  For the given condition, to try estimate its filtering selectivity,
  on a 0..1 scale (where 1.0 lets all records through).

  TODO(sgunders): In some cases, composite indexes might allow us to do better
  for joins with multiple predicates.
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
  // fields, and pick the least selective (see EstimateFieldSelectivity()
  // for why).
  // TODO(khatlen): Do the same for field <=> field?
  double selectivity_cap = 1.0;
  if (is_function_of_type(condition, Item_func::EQ_FUNC)) {
    Item_func_eq *eq = down_cast<Item_func_eq *>(condition);
    if (eq->source_multiple_equality != nullptr &&
        eq->source_multiple_equality->const_arg() == nullptr) {
      // To get consistent selectivities, we want all equalities that come from
      // the same multiple equality to use information from all of the tables.
      condition = eq->source_multiple_equality;
    } else {
      Item *left = eq->arguments()[0];
      Item *right = eq->arguments()[1];
      if (left->type() == Item::FIELD_ITEM &&
          right->type() == Item::FIELD_ITEM) {
        double selectivity = -1.0;
        for (Field *field : {down_cast<Item_field *>(left)->field,
                             down_cast<Item_field *>(right)->field}) {
          selectivity = std::max(
              selectivity,
              EstimateFieldSelectivity(field, &selectivity_cap, trace));
        }
        if (selectivity >= 0.0) {
          selectivity = std::min(selectivity, selectivity_cap);
          if (trace != nullptr) {
            *trace += StringPrintf(
                " - used an index or a histogram for %s, selectivity = %.3f\n",
                ItemToString(condition).c_str(), selectivity);
          }
          return selectivity;
        }
      } else if (left->type() == Item::FIELD_ITEM) {
        // field = <anything> (except field = field).
        //
        // We ignore the estimated selectivity (the item itself will do index
        // dives if possible, that should be better than what we will get from
        // our field = field estimation), but we want to get the cap if there's
        // a unique index, as this will make us get consistent (if not always
        // correct!) row estimates for all EQ_REF accesses over single-column
        // indexes.
        EstimateFieldSelectivity(down_cast<Item_field *>(left)->field,
                                 &selectivity_cap, trace);
      } else if (right->type() == Item::FIELD_ITEM) {
        // Same, for <anything> = field.
        EstimateFieldSelectivity(down_cast<Item_field *>(right)->field,
                                 &selectivity_cap, trace);
      }
    }
  }

  // For multi-equalities, we do the same thing. This is maybe surprising;
  // one would think that there are more degrees of freedom with more joins.
  // However, given that we want the cardinality of the join ABC to be the
  // same no matter what the join order is and which predicates we select,
  // we can see that
  //
  //   |ABC| = |A| * |B| * |C| * S_ab * S_ac
  //   |ACB| = |A| * |C| * |B| * S_ac * S_bc
  //
  // (where S_ab means selectivity of joining A with B, etc.)
  // which immediately gives S_ab = S_bc, and similar equations give
  // S_ac = S_bc and so on.
  //
  // So all the selectivities in the multi-equality must be the same!
  // However, if you go to a database with real-world data, you will see that
  // they actually differ, despite the mathematics disagreeing.
  // The mystery, however, is resolved when we realize where we've made a
  // simplification; the _real_ cardinality is given by:
  //
  //   |ABC| = (|A| * |B| * S_ab) * |C| * S_{ab,c}
  //
  // The selectivity of joining AB with C is not the same as the selectivity
  // of joining B with C (since the correlation, which we do not model,
  // differs), but we've approximated the former by the latter. And when we do
  // this approximation, we also collapse all the degrees of freedom, and can
  // have only one selectivity.
  //
  // If we get more sophisticated cardinality estimation, e.g. by histograms
  // or the likes, we need to revisit this assumption, and potentially adjust
  // our model here.
  if (is_function_of_type(condition, Item_func::MULT_EQUAL_FUNC)) {
    Item_equal *equal = down_cast<Item_equal *>(condition);

    // These should have been expanded early, before we get here.
    assert(equal->const_arg() == nullptr);

    double selectivity = -1.0;
    for (Item_field &field : equal->get_fields()) {
      selectivity = std::max(
          selectivity,
          EstimateFieldSelectivity(field.field, &selectivity_cap, trace));
    }
    if (selectivity >= 0.0) {
      selectivity = std::min(selectivity, selectivity_cap);
      if (trace != nullptr) {
        *trace += StringPrintf(
            " - used an index or a histogram for %s, selectivity = %.3f\n",
            ItemToString(condition).c_str(), selectivity);
      }
      return selectivity;
    }
  }

  // Index information did not help us, so use Item::get_filtering_effect().
  //
  // There is a challenge in that the Item::get_filtering_effect() API
  // is intrinsically locked to the old join optimizer's way of thinking,
  // where one made a long chain of (left-deep) nested tables, and selectivity
  // estimation would be run for the entire WHERE condition at all points
  // in that chain. In such a situation, it would be necessary to know which
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

  selectivity = std::min(selectivity, selectivity_cap);
  if (trace != nullptr) {
    *trace += StringPrintf(" - fallback selectivity for %s = %.3f\n",
                           ItemToString(condition).c_str(), selectivity);
  }
  return selectivity;
}
