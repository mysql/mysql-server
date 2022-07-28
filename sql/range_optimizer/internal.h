/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.

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

#ifndef SQL_RANGE_OPTIMIZER_INTERNAL_H_
#define SQL_RANGE_OPTIMIZER_INTERNAL_H_

#include "sql/range_optimizer/range_optimizer.h"

#include "mysys_err.h"          // EE_CAPACITY_EXCEEDED
#include "sql/derror.h"         // ER_THD
#include "sql/error_handler.h"  // Internal_error_handler
#include "sql/sql_class.h"      // THD

class RANGE_OPT_PARAM;
struct ROR_SCAN_INFO;
class SEL_ARG;
class SEL_IMERGE;
class SEL_TREE;
class SEL_ROOT;

[[maybe_unused]] void print_tree(String *out, const char *tree_name,
                                 SEL_TREE *tree, const RANGE_OPT_PARAM *param,
                                 const bool print_full);

void append_range(String *out, const KEY_PART_INFO *key_parts,
                  const uchar *min_key, const uchar *max_key, const uint flag);
class Opt_trace_array;
void append_range_all_keyparts(Opt_trace_array *range_trace,
                               String *range_string, String *range_so_far,
                               SEL_ROOT *keypart,
                               const KEY_PART_INFO *key_parts,
                               const bool print_full);

// Simplified version of the logic in append_range_all_keyparts(),
// supporting only append to string and using QUICK_RANGE instead of SEL_ROOT.
void append_range_to_string(const QUICK_RANGE *range,
                            const KEY_PART_INFO *first_key_part, String *out);

/**
  Shared sentinel node for all trees. Initialized by range_optimizer_init(),
  destroyed by range_optimizer_free();
  Put it in a namespace, to avoid possible conflicts with the global namespace.
*/
namespace opt_range {
extern SEL_ARG *null_element;
}

/**
  Error handling class for range optimizer. We handle only out of memory
  error here. This is to give a hint to the user to
  raise range_optimizer_max_mem_size if required.
  Warning for the memory error is pushed only once. The consequent errors
  will be ignored.
*/
class Range_optimizer_error_handler : public Internal_error_handler {
 public:
  Range_optimizer_error_handler()
      : m_has_errors(false), m_is_mem_error(false) {}

  bool handle_condition(THD *thd, uint sql_errno, const char *,
                        Sql_condition::enum_severity_level *level,
                        const char *) override {
    if (*level == Sql_condition::SL_ERROR) {
      m_has_errors = true;
      /* Out of memory error is reported only once. Return as handled */
      if (m_is_mem_error && sql_errno == EE_CAPACITY_EXCEEDED) return true;
      if (sql_errno == EE_CAPACITY_EXCEEDED) {
        m_is_mem_error = true;
        /* Convert the error into a warning. */
        *level = Sql_condition::SL_WARNING;
        push_warning_printf(
            thd, Sql_condition::SL_WARNING, ER_CAPACITY_EXCEEDED,
            ER_THD(thd, ER_CAPACITY_EXCEEDED),
            (ulonglong)thd->variables.range_optimizer_max_mem_size,
            "range_optimizer_max_mem_size",
            ER_THD(thd, ER_CAPACITY_EXCEEDED_IN_RANGE_OPTIMIZER));
        return true;
      }
    }
    return false;
  }

  bool has_errors() const { return m_has_errors; }

 private:
  bool m_has_errors;
  bool m_is_mem_error;
};

int index_next_different(bool is_index_scan, handler *file,
                         KEY_PART_INFO *key_part, uchar *record,
                         const uchar *group_prefix, uint group_prefix_len,
                         uint group_key_parts);

/*
  SEL_IMERGE is a list of possible ways to do index merge, i.e. it is
  a condition in the following form:
   (t_1||t_2||...||t_N) && (next)

  where all t_i are SEL_TREEs, next is another SEL_IMERGE and no pair
  (t_i,t_j) contains SEL_ARGS for the same index.

  SEL_TREE contained in SEL_IMERGE always has merges=NULL.

  This class relies on memory manager to do the cleanup.
*/

class SEL_IMERGE {
 public:
  Mem_root_array<SEL_TREE *> trees;

  SEL_IMERGE(MEM_ROOT *mem_root) : trees(mem_root) {}
  SEL_IMERGE(SEL_IMERGE *arg, RANGE_OPT_PARAM *param);
  bool or_sel_tree(SEL_TREE *tree);
  int or_sel_tree_with_checks(RANGE_OPT_PARAM *param, bool remove_jump_scans,
                              SEL_TREE *new_tree);
  int or_sel_imerge_with_checks(RANGE_OPT_PARAM *param, bool remove_jump_scans,
                                SEL_IMERGE *imerge);
};

/*
  Convert double value to #rows. Currently this does floor(), and we
  might consider using round() instead.
*/
#define double2rows(x) ((ha_rows)(x))

void print_key_value(String *out, const KEY_PART_INFO *key_part,
                     const uchar *key);

#endif  // SQL_RANGE_OPTIMIZER_INTERNAL_H_
