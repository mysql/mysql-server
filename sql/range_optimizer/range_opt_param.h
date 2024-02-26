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

#ifndef SQL_RANGE_OPTIMIZER_RANGE_OPT_PARAM_H_
#define SQL_RANGE_OPTIMIZER_RANGE_OPT_PARAM_H_

#include "sql/range_optimizer/internal.h"

class RANGE_OPT_PARAM {
 public:
  TABLE *table;             /* Table being analyzed */
  Query_block *query_block; /* Query block the table is part of */

  /* Array of parts of all keys for which range analysis is performed */
  KEY_PART *key_parts;
  KEY_PART *key_parts_end;
  // Memory used for allocating AccessPaths and similar
  // objects that are required for a later call to make_quick(), as well as
  // RowIterator objects and allocations they need to do themselves.
  // Typically points to thd->mem_root, but DynamicRangeIterator uses its
  // own MEM_ROOT here, as it needs to delete all the old data and allocate
  // new objects. Note that not all data allocated here will indeed be used;
  // e.g., we may allocate five AccessPaths here but only choose to use one
  // of them.
  MEM_ROOT *return_mem_root;
  // Memory that will be freed when range analysis completes.
  // In particular, this contains the tree built up to analyze
  // the input expressions (SEL_TREE), but not the actual scan ranges
  // decided on and given to the AccessPath (Quick_range).
  MEM_ROOT *temp_mem_root;
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
    used_key_no -> table_key_no translation table. Only makes sense if
    using_real_indexes==true
  */
  uint *real_keynr = nullptr;

  /**
    Whether index statistics or index dives should be used when
    estimating the number of rows in an equality range. If true, index
    statistics is used for these indexes.
  */
  bool use_index_statistics;

  /// Error handler for this param.

  Range_optimizer_error_handler error_handler;

  bool has_errors() const { return (error_handler.has_errors()); }

  KEY_PART **key = nullptr; /* First key parts of keys used in the query */
};

#endif  // SQL_RANGE_OPTIMIZER_RANGE_OPT_PARAM_H_
