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

#include "sql/join_optimizer/estimate_selectivity.h"

#include <sys/types.h>
#include <algorithm>
#include <bit>
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
#include "sql/join_optimizer/optimizer_trace.h"
#include "sql/join_optimizer/print_utils.h"
#include "sql/join_optimizer/relational_expression.h"
#include "sql/key.h"
#include "sql/sql_bitmap.h"
#include "sql/sql_const.h"
#include "sql/sql_select.h"
#include "sql/table.h"
#include "template_utils.h"

using std::string;

namespace {

/**
   Return the selectivity of 'field' derived from a histogram, or -1.0 if there
   was no histogram.
*/
double HistogramSelectivity(THD *thd, const Field &field) {
  const histograms::Histogram *const histogram =
      field.table->find_histogram(field.field_index());

  if (histogram != nullptr && !empty(*histogram)) {
    /*
      Assume that we do "SELECT ... FROM ... WHERE tab.field=<expression>".
      And there is a histogram on 'tab.field' indicating that there are
      N distinct values for that field. Then we estimate the selectivity
      to be 'fraction of non-null values'/N.
    */
    const double selectivity =
        histogram->get_non_null_values_fraction() /
        std::max<double>(1.0, histogram->get_num_distinct_values());

    if (TraceStarted(thd)) {
      Trace(thd) << " - estimating selectivity " << selectivity << " for field "
                 << field.table->alias << "." << field.field_name
                 << " from histogram showing "
                 << histogram->get_num_distinct_values()
                 << " distinct values and non-null fraction "
                 << histogram->get_non_null_values_fraction() << ".\n";
    }
    return selectivity;
  } else {
    return -1.0;
  }
}

/**
  Check if there is a unique index on key number 'key_no' of
  'field'. If so, use it to calculate an upper bound on the
  selectivity of 'field' (i.e. 1/'number of rows in table') and return
  that. If there is no such index, return 1.0.
*/
double KeyCap(THD *thd, const Field &field, uint key_no) {
  assert(key_no < field.table->s->keys);
  const KEY &key = field.table->key_info[key_no];

  // This is a less precise version of the single-row check in
  // CostingReceiver::ProposeRefAccess(). If true, we know that this index
  // can at most have selectivity 1/N, and we can use that as a global cap.
  // Importantly, unlike the capping in the EQ_REF code, this capping is
  // consistent between nested-loop index plans and hash join. Ideally, we'd
  // also support multi-predicate selectivities here and get rid of the
  // entire EQ_REF-specific code, but that requires a more holistic
  // selectivity handling (for multipart indexes) and pulling out some of
  // the sargable code for precise detection of null-rejecting predicates.
  if (!field.key_start.is_set(key_no) ||
      !Overlaps(actual_key_flags(&key), HA_NOSAME) ||
      key.actual_key_parts != 1) {
    return 1.0;
  }

  const double field_cap =
      1.0 / std::max<double>(1.0, field.table->file->stats.records);

  if (TraceStarted(thd)) {
    Trace(thd) << StringPrintf(
        " - capping selectivity to %g since index is unique\n", field_cap);
  }

  return field_cap;
}

/**
  Check if there is a unique index on 'field'. If so, use it to calculate an
  upper bound on the selectivity of field (i.e. 1/'number of rows in table'). If
  there is no such index, return 1.0.
*/
double FindSelectivityCap(THD *thd, const Field &field) {
  for (uint i = field.key_start.get_first_set(); i != MY_BIT_NONE;
       i = field.key_start.get_next_set(i)) {
    const double key_cap = KeyCap(thd, field, i);

    if (key_cap < 1.0) {
      return key_cap;
    }
  }

  return 1.0;
}

/**
   Check if any other key in 'keys' starts with the same 'prefix_length'
   fields as the last key.
   @param keys The set of keys to examine.
   @param prefix_length The length of the key prefix to examine.
   @returns true if there is another key starting with the same prefix.
*/
bool HasEarlierPermutedPrefix(Bounds_checked_array<const KEY> keys,
                              uint prefix_length) {
  const KEY &target{keys[keys.size() - 1]};

  // Check if 'field' is present in 'target'.
  const auto field_in_target = [&](const Field &field) {
    return std::any_of(target.key_part, target.key_part + prefix_length,
                       [&](const KEY_PART_INFO &target_part) {
                         return &field == target_part.field;
                       });
  };

  // Check if all fields 0..prefix_length-1 in 'key' are present in 'target'.
  const auto key_matches = [&](const KEY &key) {
    return std::all_of(key.key_part, key.key_part + prefix_length,
                       [&](const KEY_PART_INFO &part) {
                         return field_in_target(*part.field);
                       });
  };

  return std::any_of(keys.cbegin(), keys.cend() - 1, [&](const KEY &key) {
    return key.user_defined_key_parts >= prefix_length &&
           // Without records_per_key, we cannot use it to calculate
           // selectivity.
           key.has_records_per_key(prefix_length - 1) && key_matches(key);
  });
}

/// Return type for EstimateSelectivityFromIndexStatistics().
struct KeySelectivityResult {
  /// The estimated selectivity (or -1.0 if there was no suitable index).
  double selectivity;

  /// The length of the index prefix from which we derived the selectivity.
  uint prefix_length;
};

/// The set of fields that are equal in an equijoin predicate.
using EqualFieldArray = Bounds_checked_array<const Field *const>;

/*
  Check if there is a prefix of 'key' where:
  * 'equal_field' is the last key field in the prefix.
  * equal_field->table and another table t2 in 'companion_set' are
  joined on each field of the prefix.

  If so, we assume that the projection from t2 corresponding to the
  prefix is evenly distributed over the corresponding projection
  from equal_field->table. If equal_field is field N in the prefix,
  we then calculate its selectivity as:

  records_per_key(N) / records_per_key(N-1)

  Note that this will give a larger and hopefully more accurate
  selectivity value than just dividing 1 by the number of distinct
  values for 'equal_field' (derived from a histogram), as we now
  exploit the correlation between the fields in the prefix.
*/
KeySelectivityResult EstimateSelectivityFromIndexStatistics(
    THD *thd, const Field &equal_field, const CompanionSet &companion_set,
    const TABLE &table, uint key_no) {
  const KEY &key = table.key_info[key_no];
  table_map joined_tables{~PSEUDO_TABLE_BITS};

  /*
    Now loop over the fields in 'key' until either of:
    1) The current field does not have records_per_key statistics.
    2) We no longer have two tables in companion_set joined on every key
         field so far.
    3) We find the key field.
    4) We reach the end.

    In case 3, we can use this key to estimate the selectivity of equal_terms.
  */
  for (uint part_no = 0; part_no < key.user_defined_key_parts; part_no++) {
    if (!key.has_records_per_key(part_no)) {
      assert(part_no > 0);
      break;
    }

    const Field &key_field = *key.key_part[part_no].field;
    joined_tables &= companion_set.GetEqualityMap(key_field);

    /*
      Check that at least two tables are joined on each key field up
      to field part_no.  "part_no > 0" covers the case of equality
      between two fields from the same table, since these may not be
      present in companion_set. Then we still want to use the first
      key field.
    */
    if (part_no > 0 && std::popcount(joined_tables) < 2) {
      break;
    }

    if (&equal_field == &key_field &&
        !HasEarlierPermutedPrefix(
            Bounds_checked_array<const KEY>(table.key_info, key_no + 1),
            part_no + 1)) {
      const double field_selectivity = [&]() {
        if (part_no == 0) {
          if (key.table->file->stats.records == 0) {
            return 1.0;
          } else {
            // We need std::min() since records_per_key() and stats.records
            // may be updated at different points in time.
            return std::min(1.0, double{key.records_per_key(part_no)} /
                                     key.table->file->stats.records);
          }
        } else {
          return double{key.records_per_key(part_no)} /
                 key.records_per_key(part_no - 1);
        }
      }();

      if (TraceStarted(thd)) {
        Trace(thd) << " - found " << (part_no + 1)
                   << "-field prefix of candidate index " << key.name
                   << " with selectivity " << field_selectivity
                   << " for last field " << key_field.table->alias << "."
                   << key_field.field_name << "\n";
      }

      return {field_selectivity, part_no + 1};
    }
  }  // for (uint part_no = 0; part_no < key.user_defined_key_parts; part_no++)

  return {-1.0, 0};
}

/**
  Estimate the selectivity of (equi)joining a set of fields.
  Use cardinality information from indexes, if possible.
  Otherwise, use histograms, if available. Assumes equal
  distribution and zero correlation between pairs of fields, so if
  there are e.g. 100 records and 4 distinct values (A,B,C,D) for the
  field, it assumes 25% of the values will be A, 25% B, etc. (equal
  distribution), and thus, when joining a row from some other table
  against this one, 25% of the records will match (equal distribution,
  zero correlation).

  If there are multiple indexes, we choose the one with the largest
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

  @param[in] thd The current thread.
  @param[in] equal_fields  The equijoined fields for which we calculate
  selectivity.
  @param[in] companion_set The CompanionSet of the join.
  @returns The estimated selectivity of 'field' (or -1.0 if there was no
  suitable index or histogram).
*/
double EstimateEqualPredicateSelectivity(THD *thd,
                                         const EqualFieldArray &equal_fields,
                                         const CompanionSet &companion_set) {
  uint longest_prefix = 0;
  double selectivity = -1.0;
  double selectivity_cap = 1.0;

  for (const Field *equal_field : equal_fields) {
    for (uint key_no = equal_field->part_of_key.get_first_set();
         key_no != MY_BIT_NONE;
         key_no = equal_field->part_of_key.get_next_set(key_no)) {
      const KEY &key = equal_field->table->key_info[key_no];
      KeySelectivityResult key_data{-1.0, 0};

      const double key_cap = KeyCap(thd, *equal_field, key_no);
      if (key_cap < 1.0) {
        key_data = {key_cap, 1};
      } else if (key.has_records_per_key(0)) {
        key_data = EstimateSelectivityFromIndexStatistics(
            thd, *equal_field, companion_set, *equal_field->table, key_no);
      }

      selectivity_cap = std::min(selectivity_cap, key_cap);

      if (key_data.prefix_length > longest_prefix) {
        longest_prefix = key_data.prefix_length;
        selectivity = key_data.selectivity;
      } else if (key_data.prefix_length == longest_prefix) {
        selectivity = std::max(selectivity, key_data.selectivity);
      }
    }
  }

  if (selectivity >= 0.0) {
    selectivity = std::min(selectivity, selectivity_cap);
  } else {
    // Look for histograms if there was no suitable index.
    for (const Field *field : equal_fields) {
      selectivity = std::max(selectivity, HistogramSelectivity(thd, *field));
    }
  }

  return selectivity;
}

}  // Anonymous namespace.

/**
  For the given condition, to try estimate its filtering selectivity,
  on a 0..1 scale (where 1.0 lets all records through).
 */
double EstimateSelectivity(THD *thd, Item *condition,
                           const CompanionSet &companion_set) {
  // If the item is a true constant, we can say immediately whether it passes
  // or filters all rows. (Actually, calling get_filtering_effect() below
  // would crash if used_tables() is zero, which it is for const items.)
  if (condition->const_item()) {
    return (condition->val_int() != 0) ? 1.0 : 0.0;
  }

  // For field = field (e.g. t1.x = t2.y), we try to use index
  // information or histograms to find a better selectivity estimate.
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
        const Field *fields[] = {down_cast<Item_field *>(left)->field,
                                 down_cast<Item_field *>(right)->field};

        double selectivity = EstimateEqualPredicateSelectivity(
            thd, EqualFieldArray(fields, array_elements(fields)),
            companion_set);

        if (selectivity >= 0.0) {
          if (TraceStarted(thd)) {
            Trace(thd) << StringPrintf(
                " - used an index or a histogram for %s, selectivity = %g\n",
                ItemToString(condition).c_str(), selectivity);
          }
          return selectivity;
        }
      } else if (left->type() == Item::FIELD_ITEM) {
        // field = <anything> (except field = field).
        //
        // See if we can derive an upper limit on selectivity from a unique
        // index on this field.
        selectivity_cap = std::min(
            selectivity_cap,
            FindSelectivityCap(thd, *down_cast<Item_field *>(left)->field));
      } else if (right->type() == Item::FIELD_ITEM) {
        // Same, for <anything> = field.
        selectivity_cap = std::min(
            selectivity_cap,
            FindSelectivityCap(thd, *down_cast<Item_field *>(right)->field));
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
    Prealloced_array<const Field *, 4> fields{PSI_NOT_INSTRUMENTED};
    for (const Item_field &item : equal->get_fields()) {
      fields.push_back(item.field);
    }

    double selectivity = EstimateEqualPredicateSelectivity(
        thd, EqualFieldArray(&fields[0], fields.size()), companion_set);

    if (selectivity >= 0.0) {
      if (TraceStarted(thd)) {
        Trace(thd) << StringPrintf(
            " - used an index or a histogram for %s, selectivity = %g\n",
            ItemToString(condition).c_str(), selectivity);
      }
      return selectivity;
    }
  }

  // Neither index information nor histograms could help us, so use
  // Item::get_filtering_effect().
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
  if (TraceStarted(thd)) {
    Trace(thd) << StringPrintf(" - fallback selectivity for %s = %g\n",
                               ItemToString(condition).c_str(), selectivity);
  }
  return selectivity;
}
