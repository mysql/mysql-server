/* Copyright (c) 2000, 2021, Oracle and/or its affiliates.

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

#ifndef SQL_RANGE_OPTIMIZER_RANGE_OPT_PARAM_H_
#define SQL_RANGE_OPTIMIZER_RANGE_OPT_PARAM_H_

#include "sql/range_optimizer/internal.h"
#include "sql/uniques.h"

class RANGE_OPT_PARAM {
 public:
  THD *thd;                 /* Current thread handle */
  TABLE *table;             /* Table being analyzed */
  Query_block *query_block; /* Query block the table is part of */
  Item *cond;               /* Used inside get_mm_tree(). */
  table_map prev_tables;
  table_map read_tables;
  table_map current_table; /* Bit of the table being analyzed */

  /* Array of parts of all keys for which range analysis is performed */
  KEY_PART *key_parts;
  KEY_PART *key_parts_end;
  MEM_ROOT
  *mem_root; /* Memory that will be freed when range analysis completes */
  MEM_ROOT *old_root; /* Memory that will last until the query end */
  /*
    Number of indexes used in range analysis (In SEL_TREE::keys only first
    #keys elements are not empty)
  */
  uint keys;

  /*
    If true, the index descriptions describe real indexes (and it is ok to
    call field->optimize_range(real_keynr[...], ...).
    Otherwise index description describes fake indexes, like a partitioning
    expression.
  */
  bool using_real_indexes;

  /*
    Aggressively remove "scans" that do not have conditions on first
    keyparts. Such scans are usable when doing partition pruning but not
    regular range optimization.
  */
  bool remove_jump_scans;

  /*
    used_key_no -> table_key_no translation table. Only makes sense if
    using_real_indexes==true
  */
  uint real_keynr[MAX_KEY];

  /*
    Used to store 'current key tuples', in both range analysis and
    partitioning (list) analysis
  */
  uchar min_key[MAX_KEY_LENGTH + MAX_FIELD_WIDTH],
      max_key[MAX_KEY_LENGTH + MAX_FIELD_WIDTH];

  bool force_default_mrr;
  /**
    Whether index statistics or index dives should be used when
    estimating the number of rows in an equality range. If true, index
    statistics is used for these indexes.
  */
  bool use_index_statistics;

  /// Error handler for this param.

  Range_optimizer_error_handler error_handler;

  bool has_errors() const { return (error_handler.has_errors()); }

  virtual ~RANGE_OPT_PARAM() = default;
};

class PARAM : public RANGE_OPT_PARAM {
 public:
  KEY_PART *key[MAX_KEY]; /* First key parts of keys used in the query */
  longlong baseflag;
  uint max_key_part;
  /* Number of ranges in the last checked tree->key */
  uint range_count;

  bool quick;  // Don't calulate possible keys

  uint fields_bitmap_size;
  MY_BITMAP needed_fields; /* bitmask of fields needed by the query */
  MY_BITMAP tmp_covered_fields;

  Key_map *needed_reg; /* ptr to needed_reg argument of test_quick_select() */

  // Buffer for index_merge cost estimates.
  Unique::Imerge_cost_buf_type imerge_cost_buff;

  /* true if last checked tree->key can be used for ROR-scan */
  bool is_ror_scan;
  /* true if last checked tree->key can be used for index-merge-scan */
  bool is_imerge_scan;
  /* Number of ranges in the last checked tree->key */
  uint n_ranges;

  /*
     The sort order the range access method must be able
     to provide. Three-value logic: asc/desc/don't care
  */
  enum_order order_direction;

  /// Control whether the various index merge strategies are allowed
  bool index_merge_allowed;
  bool index_merge_union_allowed;
  bool index_merge_sort_union_allowed;
  bool index_merge_intersect_allowed;

  /// Same value as JOIN_TAB::skip_records_in_range().
  bool skip_records_in_range;
};

#endif  // SQL_RANGE_OPTIMIZER_RANGE_OPT_PARAM_H_
