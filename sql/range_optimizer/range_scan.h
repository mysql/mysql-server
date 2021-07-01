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

#ifndef SQL_RANGE_OPTIMIZER_RANGE_SCAN_H_
#define SQL_RANGE_OPTIMIZER_RANGE_SCAN_H_

#include <sys/types.h>
#include <memory>

#include "my_bitmap.h"
#include "my_inttypes.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/range_optimizer/range_optimizer.h"

class KEY_PART_INFO;
class RANGE_OPT_PARAM;
class SEL_ARG;
class SEL_ROOT;
class String;
class THD;
struct KEY_MULTI_RANGE;
struct MEM_ROOT;
struct TABLE;

/*
  MRR range sequence, array<QUICK_RANGE> implementation: sequence traversal
  context.
*/
struct QUICK_RANGE_SEQ_CTX {
  Quick_ranges::const_iterator first;
  Quick_ranges::const_iterator cur;
  Quick_ranges::const_iterator last;
};

/*
  Quick select that does a range scan on a single key. The records are
  returned in key order if ::need_sorted_output() has been called.
*/
class QUICK_RANGE_SELECT : public QUICK_SELECT_I {
 protected:
  handler *file;
  /* Members to deal with case when this quick select is a ROR-merged scan */
  bool in_ror_merged_scan;

  // TODO: pre-allocate space to avoid malloc/free for small number of columns.
  MY_BITMAP column_bitmap;

  friend class TRP_ROR_INTERSECT;
  friend uint quick_range_seq_next(range_seq_t rseq, KEY_MULTI_RANGE *range);
  friend range_seq_t quick_range_seq_init(void *init_param, uint n_ranges,
                                          uint flags);
  friend class QUICK_SELECT_DESC;
  friend class QUICK_INDEX_MERGE_SELECT;
  friend class QUICK_ROR_INTERSECT_SELECT;
  friend class QUICK_GROUP_MIN_MAX_SELECT;

  Bounds_checked_array<QUICK_RANGE *> ranges; /* ordered array of range ptrs */
  bool free_file; /* TRUE <=> this->file is "owned" by this quick select */

  /* Range pointers to be used when not using MRR interface */
  QUICK_RANGE **cur_range; /* current element in ranges  */
  QUICK_RANGE *last_range;

  /* Members needed to use the MRR interface */
  QUICK_RANGE_SEQ_CTX qr_traversal_ctx;

  uint mrr_flags;    /* Flags to be used with MRR interface */
  uint mrr_buf_size; /* copy from thd->variables.read_rnd_buff_size */
  HANDLER_BUFFER *mrr_buf_desc; /* the handler buffer */

  /* Info about index we're scanning */
  const KEY_PART *key_parts;
  KEY_PART_INFO *key_part_info;

  bool dont_free; /* Used by QUICK_SELECT_DESC */

 private:
  MEM_ROOT *mem_root;

  int cmp_next(QUICK_RANGE *range);
  int cmp_prev(QUICK_RANGE *range);
  bool row_in_ranges();

 public:
  QUICK_RANGE_SELECT(TABLE *table, uint index_arg, MEM_ROOT *return_mem_root,
                     uint mrr_flags, uint mrr_buf_size, const KEY_PART *key,
                     Bounds_checked_array<QUICK_RANGE *> ranges,
                     uint used_key_parts_arg);
  ~QUICK_RANGE_SELECT() override;

  QUICK_RANGE_SELECT(const QUICK_RANGE_SELECT &) = delete;

  /* Default move ctor used by QUICK_SELECT_DESC */
  QUICK_RANGE_SELECT(QUICK_RANGE_SELECT &&) = default;

  void need_sorted_output() override;
  int init() override;
  int reset(void) override;
  int get_next() override;
  void range_end() override;
  int get_next_prefix(uint prefix_length, uint group_key_parts,
                      uchar *cur_prefix);
  bool reverse_sorted() const override { return false; }
  bool reverse_sort_possible() const override { return true; }
  bool unique_key_range() override;
  int init_ror_merged_scan(bool reuse_handler) override;
  void save_last_pos() override { file->position(record); }
  RangeScanType get_type() const override { return QS_TYPE_RANGE; }
  bool is_loose_index_scan() const override { return false; }
  bool is_agg_loose_index_scan() const override { return false; }
  void add_keys_and_lengths(String *key_names, String *used_lengths) override;
  void add_info_string(String *str) override;
#ifndef NDEBUG
  void dbug_dump(int indent, bool verbose) override;
#endif
  QUICK_SELECT_I *make_reverse(uint used_key_parts_arg) override;
  void set_handler(handler *file_arg) override { file = file_arg; }

  void get_fields_used(MY_BITMAP *used_fields) override {
    for (uint i = 0; i < used_key_parts; i++)
      bitmap_set_bit(used_fields, key_parts[i].field->field_index());
  }
  uint get_mrr_flags() const { return mrr_flags; }
};

#endif  // SQL_RANGE_OPTIMIZER_RANGE_SCAN_H_
