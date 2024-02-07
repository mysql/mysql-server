/* Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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

#ifndef SQL_RANGE_OPTIMIZER_RANGE_ANALYSIS_H_
#define SQL_RANGE_OPTIMIZER_RANGE_ANALYSIS_H_

#include "my_table_map.h"

class SEL_TREE;
class RANGE_OPT_PARAM;
class Item;
class THD;

/*
  RangeAnalysisModule
    A module that accepts a condition, index (or partitioning) description,
    and builds lists of intervals (in index/partitioning space), such that
    all possible records that match the condition are contained within the
    intervals.
    The entry point for the range analysis module is get_mm_tree()
    (mm=min_max) function.

    The lists are returned in form of complicated structure of interlinked
    SEL_TREE/SEL_IMERGE/SEL_ROOT/SEL_ARG objects.
    See quick_range_seq_next, find_used_partitions for examples of how to walk
    this structure.
    All direct "users" of this module are located within this file, too.

 */
SEL_TREE *get_mm_tree(THD *thd, RANGE_OPT_PARAM *param, table_map prev_tables,
                      table_map read_tables, table_map current_table,
                      bool remove_jump_scans, Item *cond);

#endif  // SQL_RANGE_OPTIMIZER_RANGE_ANALYSIS_H_
