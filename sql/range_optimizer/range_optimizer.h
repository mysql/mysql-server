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

/* classes to use when handling where clause */

#ifndef SQL_RANGE_OPTIMIZER_RANGE_OPTIMIZER_H_
#define SQL_RANGE_OPTIMIZER_RANGE_OPTIMIZER_H_

#include <assert.h>
#include <sys/types.h>
#include <algorithm>

#include "my_base.h"
#include "my_inttypes.h"
#include "my_table_map.h"
#include "prealloced_array.h"  // Prealloced_array
#include "sql/field.h"         // Field
#include "sql/handler.h"
#include "sql/item_func.h"
#include "sql/key_spec.h"
#include "sql/malloc_allocator.h"  // IWYU pragma: keep
#include "sql/sql_bitmap.h"
#include "sql/sql_const.h"
#include "sql_string.h"

class Item;
class Opt_trace_context;
class Query_block;
class THD;
struct MY_BITMAP;
struct TABLE;

struct KEY_PART {
  uint16 key, part;
  /* See KEY_PART_INFO for meaning of the next two: */
  uint16 store_length, length;
  uint8 null_bit;
  /*
    Keypart flags (0 when this structure is used by partition pruning code
    for fake partitioning index description)
  */
  uint16 flag;
  Field *field;
  Field::imagetype image_type;
};

class QUICK_RANGE {
 public:
  uchar *min_key, *max_key;
  uint16 min_length, max_length;

  /// Stores bitwise-or'ed bits defined in enum key_range_flags.
  uint16 flag;

  /**
    Stores one of the HA_READ_MBR_XXX items in enum ha_rkey_function, only
    effective when flag has a GEOM_FLAG bit.
  */
  enum ha_rkey_function rkey_func_flag;
  key_part_map min_keypart_map,  // bitmap of used keyparts in min_key
      max_keypart_map;           // bitmap of used keyparts in max_key

  QUICK_RANGE(); /* Full range */
  QUICK_RANGE(const uchar *min_key_arg, uint min_length_arg,
              key_part_map min_keypart_map_arg, const uchar *max_key_arg,
              uint max_length_arg, key_part_map max_keypart_map_arg,
              uint flag_arg, enum ha_rkey_function rkey_func);

  /**
     Initalizes a key_range object for communication with storage engine.

     This function facilitates communication with the Storage Engine API by
     translating the minimum endpoint of the interval represented by this
     QUICK_RANGE into an index range endpoint specifier for the engine.

     @param kr Pointer to an uninitialized key_range C struct.

     @param prefix_length The length of the search key prefix to be used for
     lookup.

     @param keypart_map A set (bitmap) of keyparts to be used.
  */
  void make_min_endpoint(key_range *kr, uint prefix_length,
                         key_part_map keypart_map) {
    make_min_endpoint(kr);
    kr->length = std::min(kr->length, prefix_length);
    kr->keypart_map &= keypart_map;
  }

  /**
     Initalizes a key_range object for communication with storage engine.

     This function facilitates communication with the Storage Engine API by
     translating the minimum endpoint of the interval represented by this
     QUICK_RANGE into an index range endpoint specifier for the engine.

     @param kr Pointer to an uninitialized key_range C struct.
  */
  void make_min_endpoint(key_range *kr) {
    kr->key = (const uchar *)min_key;
    kr->length = min_length;
    kr->keypart_map = min_keypart_map;
    kr->flag = ((flag & NEAR_MIN) ? HA_READ_AFTER_KEY
                                  : (flag & EQ_RANGE) ? HA_READ_KEY_EXACT
                                                      : HA_READ_KEY_OR_NEXT);
  }

  /**
     Initalizes a key_range object for communication with storage engine.

     This function facilitates communication with the Storage Engine API by
     translating the maximum endpoint of the interval represented by this
     QUICK_RANGE into an index range endpoint specifier for the engine.

     @param kr Pointer to an uninitialized key_range C struct.

     @param prefix_length The length of the search key prefix to be used for
     lookup.

     @param keypart_map A set (bitmap) of keyparts to be used.
  */
  void make_max_endpoint(key_range *kr, uint prefix_length,
                         key_part_map keypart_map) {
    make_max_endpoint(kr);
    kr->length = std::min(kr->length, prefix_length);
    kr->keypart_map &= keypart_map;
  }

  /**
     Initalizes a key_range object for communication with storage engine.

     This function facilitates communication with the Storage Engine API by
     translating the maximum endpoint of the interval represented by this
     QUICK_RANGE into an index range endpoint specifier for the engine.

     @param kr Pointer to an uninitialized key_range C struct.
  */
  void make_max_endpoint(key_range *kr) {
    kr->key = (const uchar *)max_key;
    kr->length = max_length;
    kr->keypart_map = max_keypart_map;
    /*
      We use READ_AFTER_KEY here because if we are reading on a key
      prefix we want to find all keys with this prefix
    */
    kr->flag = (flag & NEAR_MAX ? HA_READ_BEFORE_KEY : HA_READ_AFTER_KEY);
  }
};

enum RangeScanType {
  QS_TYPE_RANGE,
  QS_TYPE_RANGE_DESC,
  QS_TYPE_INDEX_MERGE,
  QS_TYPE_ROR_INTERSECT,
  QS_TYPE_ROR_UNION,
  QS_TYPE_GROUP_MIN_MAX,
  QS_TYPE_SKIP_SCAN
};

/*
  Quick select interface.
  This class is a parent for all QUICK_*_SELECT classes.

  The usage scenario is as follows:
  1. Create quick select
    quick = new (mem_root) QUICK_XXX_SELECT(...);

  2. Perform lightweight initialization. This can be done in 2 ways:
  2.a: Regular initialization
    if (quick->init())
    {
      //the only valid action after failed init() call is destroy
      destroy(quick);
    }
  2.b: Special initialization for quick selects merged by QUICK_ROR_*_SELECT
    if (quick->init_ror_merged_scan())
      destroy(quick);

  3. Perform zero, one, or more scans.
    while (...)
    {
      // initialize quick select for scan. This may allocate
      // buffers and/or prefetch rows.
      if (quick->reset())
      {
        //the only valid action after failed reset() call is destroy
        destroy(quick);
        //abort query
      }

      // perform the scan
      do
      {
        res= quick->get_next();
      } while (res && ...)
    }

  4. Destroy the select:
    destroy(quick);
*/

class QUICK_SELECT_I {
 public:
  ha_rows records;         /* estimate of # of records to be retrieved */
  Cost_estimate cost_est;  ///> cost to perform this retrieval
  TABLE *m_table;
  /*
    Index this quick select uses, or MAX_KEY for quick selects
    that use several indexes
  */
  uint index;

  /*
    Total length of first used_key_parts parts of the key.
    Applicable if index!= MAX_KEY.
  */
  uint max_used_key_length;

  /*
    Max. number of (first) key parts this quick select uses for retrieval.
    eg. for "(key1p1=c1 AND key1p2=c2) OR key1p1=c2" used_key_parts == 2.
    Applicable if index!= MAX_KEY.

    For QUICK_GROUP_MIN_MAX_SELECT it includes MIN/MAX argument keyparts.
  */
  uint used_key_parts;
  /**
    true if creation of the object is forced by the hint.
    The flag is used to skip ref evaluation in find_best_ref() function.
    It also enables using of QUICK_SELECT object in
    Optimize_table_order::best_access_path() regardless of the evaluation cost.
  */
  bool forced_by_hint;

  QUICK_SELECT_I();
  QUICK_SELECT_I(const QUICK_SELECT_I &) = default;
  virtual ~QUICK_SELECT_I() = default;

  /*
    Do post-constructor initialization.
    SYNOPSIS
      init()

    init() performs initializations that should have been in constructor if
    it was possible to return errors from constructors. The join optimizer may
    create and then destroy quick selects without retrieving any rows so init()
    must not contain any IO or CPU intensive code.

    If init() call fails the only valid action is to destroy this quick select,
    reset() and get_next() must not be called.

    RETURN
      0      OK
      other  Error code
  */
  virtual int init() = 0;

  /*
    Initialize quick select for row retrieval.
    SYNOPSIS
      reset()

    reset() should be called when it is certain that row retrieval will be
    necessary. This call may do heavyweight initialization like buffering first
    N records etc. If reset() call fails get_next() must not be called.
    Note that reset() may be called several times if
     * the quick select is executed in a subselect
     * a JOIN buffer is used

    RETURN
      0      OK
      other  Error code
  */
  virtual int reset(void) = 0;

  virtual int get_next() = 0; /* get next record to retrieve */

  /* Range end should be called when we have looped over the whole index */
  virtual void range_end() {}

  /**
    Whether the range access method returns records in reverse order.
  */
  virtual bool reverse_sorted() const = 0;
  /**
    Whether the range access method is capable of returning records
    in reverse order.
  */
  virtual bool reverse_sort_possible() const = 0;
  virtual bool unique_key_range() { return false; }

  /*
    Request that this quick select produces sorted output.
    Not all quick selects can provide sorted output, the caller is responsible
    for calling this function only for those quick selects that can.
    The implementation is also allowed to provide sorted output even if it
    was not requested if benificial, or required by implementation
    internals.
  */
  virtual void need_sorted_output() = 0;

  /* Get type of this quick select */
  virtual RangeScanType get_type() const = 0;
  virtual bool is_loose_index_scan() const = 0;
  virtual bool is_agg_loose_index_scan() const = 0;

  /*
    Initialize this quick select as a merged scan inside a ROR-union or a ROR-
    intersection scan. The caller must not additionally call init() if this
    function is called.
    SYNOPSIS
      init_ror_merged_scan()
        reuse_handler  If true, the quick select may use table->handler,
                       otherwise it must create and use a separate handler
                       object.
    RETURN
      0     Ok
      other Error
  */
  virtual int init_ror_merged_scan(bool reuse_handler [[maybe_unused]]) {
    assert(0);
    return 1;
  }

  /*
    Save ROWID of last retrieved row in file->ref. This used in ROR-merging.
  */
  virtual void save_last_pos() {}

  /*
    Append comma-separated list of keys this quick select uses to key_names;
    append comma-separated list of corresponding used lengths to used_lengths.
    This is used by select_describe.
  */
  virtual void add_keys_and_lengths(String *key_names,
                                    String *used_lengths) = 0;

  /*
    Append text representation of quick select structure (what and how is
    merged) to str. The result is added to "Extra" field in EXPLAIN output.
    This function is implemented only by quick selects that merge other quick
    selects output and/or can produce output suitable for merging.
  */
  virtual void add_info_string(String *str [[maybe_unused]]) {}
  /*
    Return 1 if any index used by this quick select
    uses field which is marked in passed bitmap.
  */
  virtual bool is_keys_used(const MY_BITMAP *fields);

  /*
    rowid of last row retrieved by this quick select. This is used only when
    doing ROR-index_merge selects
  */
  uchar *last_rowid;

  /*
    Table record buffer used by this quick select.
  */
  uchar *record;
#ifndef NDEBUG
  /*
    Print quick select information to DBUG_FILE. Caller is responsible
    for locking DBUG_FILE before this call and unlocking it afterwards.
  */
  virtual void dbug_dump(int indent, bool verbose) = 0;
#endif

  /*
    Returns a QUICK_SELECT with reverse order of to the index.
  */
  virtual QUICK_SELECT_I *make_reverse(uint used_key_parts_arg
                                       [[maybe_unused]]) {
    return nullptr;
  }
  virtual void set_handler(handler *file_arg [[maybe_unused]]) {}

  /**
    Get the fields used by the range access method.

    @param[out] used_fields Bitmap of fields that this range access
                            method uses.
  */
  virtual void get_fields_used(MY_BITMAP *used_fields) = 0;
  void trace_quick_description(Opt_trace_context *trace);
};

using Quick_ranges = Mem_root_array<QUICK_RANGE *>;
using Quick_ranges_array = Mem_root_array<Quick_ranges *>;

int test_quick_select(THD *thd, MEM_ROOT *return_mem_root,
                      MEM_ROOT *temp_mem_root, Key_map keys_to_use,
                      table_map prev_tables, table_map read_tables,
                      ha_rows limit, bool force_quick_range,
                      const enum_order interesting_order, TABLE *table,
                      bool skip_records_in_range, Item *cond,
                      Key_map *needed_reg, QUICK_SELECT_I **quick,
                      bool ignore_table_scan, Query_block *query_block);

void store_key_image_to_rec(Field *field, uchar *ptr, uint len);

extern String null_string;

/// Global initialization of the null_element. Call on server start.
void range_optimizer_init();

/// Global destruction of the null_element. Call on server stop.
void range_optimizer_free();

/**
  Test if 'value' is comparable to 'field' when setting up range
  access for predicate "field OP value". 'field' is a field in the
  table being optimized for while 'value' is whatever 'field' is
  compared to.

  @param cond_func   the predicate item that compares 'field' with 'value'
  @param field       field in the predicate
  @param itype       itMBR if indexed field is spatial, itRAW otherwise
  @param comp_type   comparator for the predicate
  @param value       whatever 'field' is compared to

  @return true if 'field' and 'value' are comparable, false otherwise
*/

bool comparable_in_index(Item *cond_func, const Field *field,
                         const Field::imagetype itype,
                         Item_func::Functype comp_type, const Item *value);

#endif  // SQL_RANGE_OPTIMIZER_RANGE_OPTIMIZER_H_
