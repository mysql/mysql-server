#ifndef PARTITION_INFO_INCLUDED
#define PARTITION_INFO_INCLUDED

/* Copyright (c) 2006, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "lock.h"                             // Tablespace_hash_set
#include "partition_element.h"
#include "mysqld.h"                           // key_map
#include "sql_bitmap.h"                       // Bitmap
#include "sql_data_change.h"                  // enum_duplicates

#define NOT_A_PARTITION_ID UINT_MAX32

class partition_info;
class Partition_share;
class COPY_INFO;
class Create_field;
struct st_partition_iter;
struct TABLE_LIST;

/**
  A "Get next" function for partition iterator.


  Depending on whether partitions or sub-partitions are iterated, the
  function returns next subpartition id/partition number. The sequence of
  returned numbers is not ordered and may contain duplicates.

  When the end of sequence is reached, NOT_A_PARTITION_ID is returned, and
  the iterator resets itself (so next get_next() call will start to
  enumerate the set all over again).

  @param[in,out] part_iter Partition iterator, you call only
                           "iter.get_next(&iter)"

  @return Partition id
    @retval NOT_A_PARTITION_ID if there are no more partitions.
    @retval [sub]partition_id  of the next partition
*/
typedef uint32 (*partition_iter_func)(st_partition_iter* part_iter);

/**
  Partition set iterator. Used to enumerate a set of [sub]partitions
  obtained in partition interval analysis (see get_partitions_in_range_iter).

  For the user, the only meaningful field is get_next, which may be used as
  follows:
             part_iterator.get_next(&part_iterator);

  Initialization is done by any of the following calls:
    - get_partitions_in_range_iter-type function call
    - init_single_partition_iterator()
    - init_all_partitions_iterator()
  Cleanup is not needed.
*/

typedef struct st_partition_iter
{
  partition_iter_func get_next;
  /*
    Valid for "Interval mapping" in LIST partitioning: if true, let the
    iterator also produce id of the partition that contains NULL value.
  */
  bool ret_null_part, ret_null_part_orig;
  struct st_part_num_range
  {
    uint32 start;
    uint32 cur;
    uint32 end;
  };

  struct st_field_value_range
  {
    longlong start;
    longlong cur;
    longlong end;
  };

  union
  {
    struct st_part_num_range     part_nums;
    struct st_field_value_range  field_vals;
  };
  partition_info *part_info;
} PARTITION_ITERATOR;


struct st_ddl_log_memory_entry;

typedef struct {
  longlong list_value;
  uint32 partition_id;
} LIST_PART_ENTRY;

/* Some function typedefs */
typedef int (*get_part_id_func)(partition_info *part_info,
                                 uint32 *part_id,
                                 longlong *func_value);
typedef int (*get_subpart_id_func)(partition_info *part_info,
                                   uint32 *part_id);


/**
  Get an iterator for set of partitions that match given field-space interval.

  Functions with this signature are used to perform "Partitioning Interval
  Analysis". This analysis is applicable for any type of [sub]partitioning
  by some function of a single fieldX. The idea is as follows:
  Given an interval "const1 <=? fieldX <=? const2", find a set of partitions
  that may contain records with value of fieldX within the given interval.

  The min_val, max_val and flags parameters specify the interval.
  The set of partitions is returned by initializing an iterator in *part_iter

  @note
    There are currently three functions of this type:
     - get_part_iter_for_interval_via_walking
     - get_part_iter_for_interval_cols_via_map
     - get_part_iter_for_interval_via_mapping

  @param part_info           Partitioning info
  @param is_subpart
  @param store_length_array  Length of fields packed in opt_range_key format
  @param min_val             Left edge,  field value in opt_range_key format
  @param max_val             Right edge, field value in opt_range_key format
  @param min_len             Length of minimum value
  @param max_len             Length of maximum value
  @param flags               Some combination of NEAR_MIN, NEAR_MAX,
                             NO_MIN_RANGE, NO_MAX_RANGE
  @param part_iter           Iterator structure to be initialized

  @return Operation status
    @retval 0   No matching partitions, iterator not initialized
    @retval 1   Some partitions would match, iterator intialized for traversing them
    @retval -1  All partitions would match, iterator not initialized
*/

typedef int (*get_partitions_in_range_iter)(partition_info *part_info,
                                            bool is_subpart,
                                            uint32 *store_length_array,
                                            uchar *min_val, uchar *max_val,
                                            uint min_len, uint max_len,
                                            uint flags,
                                            PARTITION_ITERATOR *part_iter);
class partition_info : public Sql_alloc
{
public:
  /*
   * Here comes a set of definitions needed for partitioned table handlers.
   */
  List<partition_element> partitions;
  List<partition_element> temp_partitions;

  List<char> part_field_list;
  List<char> subpart_field_list;

  /*
    If there is no subpartitioning, use only this func to get partition ids.
    If there is subpartitioning, use the this func to get partition id when
    you have both partition and subpartition fields.
  */
  get_part_id_func get_partition_id;

  /* Get partition id when we don't have subpartition fields */
  get_part_id_func get_part_partition_id;

  /*
    Get subpartition id when we have don't have partition fields by we do
    have subpartition ids.
    Mikael said that for given constant tuple
    {subpart_field1, ..., subpart_fieldN} the subpartition id will be the
    same in all subpartitions
  */
  get_subpart_id_func get_subpartition_id;

  /*
    When we have various string fields we might need some preparation
    before and clean-up after calling the get_part_id_func's. We need
    one such method for get_part_partition_id and one for
    get_subpartition_id.
  */
  get_part_id_func get_part_partition_id_charset;
  get_subpart_id_func get_subpartition_id_charset;

  /* NULL-terminated array of fields used in partitioned expression */
  Field **part_field_array;
  Field **subpart_field_array;
  Field **part_charset_field_array;
  Field **subpart_charset_field_array;
  /*
    Array of all fields used in partition and subpartition expression,
    without duplicates, NULL-terminated.
  */
  Field **full_part_field_array;
  /*
    Set of all fields used in partition and subpartition expression.
    Required for testing of partition fields in write_set when
    updating. We need to set all bits in read_set because the row may
    need to be inserted in a different [sub]partition.
  */
  MY_BITMAP full_part_field_set;

  /*
    When we have a field that requires transformation before calling the
    partition functions we must allocate field buffers for the field of
    the fields in the partition function.
  */
  uchar **part_field_buffers;
  uchar **subpart_field_buffers;
  uchar **restore_part_field_ptrs;
  uchar **restore_subpart_field_ptrs;

  Item *part_expr;
  Item *subpart_expr;

  Item *item_free_list;

  struct st_ddl_log_memory_entry *first_log_entry;
  struct st_ddl_log_memory_entry *exec_log_entry;
  struct st_ddl_log_memory_entry *frm_log_entry;

  /*
    Bitmaps of partitions used by the current query.
    * read_partitions  - partitions to be used for reading.
    * lock_partitions  - partitions that must be locked (read or write).
    Usually read_partitions is the same set as lock_partitions, but
    in case of UPDATE the WHERE clause can limit the read_partitions set,
    but not neccesarily the lock_partitions set.
    Usage pattern:
    * Initialized in ha_partition::open().
    * read+lock_partitions is set  according to explicit PARTITION,
      WL#5217, in open_and_lock_tables().
    * Bits in read_partitions can be cleared in prune_partitions()
      in the optimizing step.
      (WL#4443 is about allowing prune_partitions() to affect lock_partitions
      and be done before locking too).
    * When the partition enabled handler get an external_lock call it locks
      all partitions in lock_partitions (and remembers which partitions it
      locked, so that it can unlock them later). In case of LOCK TABLES it will
      lock all partitions, and keep them locked while lock_partitions can
      change for each statement under LOCK TABLES.
    * Freed at the same time item_free_list is freed.
  */
  MY_BITMAP read_partitions;
  MY_BITMAP lock_partitions;
  bool bitmaps_are_initialized;
  // TODO: Add first_read_partition and num_read_partitions?

  union {
    longlong *range_int_array;
    LIST_PART_ENTRY *list_array;
    part_column_list_val *range_col_array;
    part_column_list_val *list_col_array;
  };

  /********************************************
   * INTERVAL ANALYSIS
   ********************************************/
  /*
    Partitioning interval analysis function for partitioning, or NULL if
    interval analysis is not supported for this kind of partitioning.
  */
  get_partitions_in_range_iter get_part_iter_for_interval;
  /*
    Partitioning interval analysis function for subpartitioning, or NULL if
    interval analysis is not supported for this kind of partitioning.
  */
  get_partitions_in_range_iter get_subpart_iter_for_interval;

  /********************************************
   * INTERVAL ANALYSIS ENDS
   ********************************************/

  longlong err_value;
  char* part_info_string;

  char *part_func_string;
  char *subpart_func_string;

  partition_element *curr_part_elem;     // part or sub part
  partition_element *current_partition;  // partition
  part_elem_value *curr_list_val;
  uint curr_list_object;
  uint num_columns;

  TABLE *table;
  /*
    These key_map's are used for Partitioning to enable quick decisions
    on whether we can derive more information about which partition to
    scan just by looking at what index is used.
  */
  key_map all_fields_in_PF, all_fields_in_PPF, all_fields_in_SPF;
  key_map some_fields_in_PF;

  handlerton *default_engine_type;
  partition_type part_type;
  partition_type subpart_type;

  size_t part_info_len;
  size_t part_func_len;
  size_t subpart_func_len;

  uint num_parts;
  uint num_subparts;
  uint count_curr_subparts;                  // used during parsing

  uint num_list_values;

  uint num_part_fields;
  uint num_subpart_fields;
  uint num_full_part_fields;

  uint has_null_part_id;
  /*
    This variable is used to calculate the partition id when using
    LINEAR KEY/HASH. This functionality is kept in the MySQL Server
    but mainly of use to handlers supporting partitioning.
  */
  uint16 linear_hash_mask;
  /*
    PARTITION BY KEY ALGORITHM=N
    Which algorithm to use for hashing the fields.
    N = 1 - Use 5.1 hashing (numeric fields are hashed as binary)
    N = 2 - Use 5.5 hashing (numeric fields are hashed like latin1 bytes)
  */
  enum enum_key_algorithm
    {
      KEY_ALGORITHM_NONE= 0,
      KEY_ALGORITHM_51= 1,
      KEY_ALGORITHM_55= 2
    };
  enum_key_algorithm key_algorithm;

  /* Only the number of partitions defined (uses default names and options). */
  bool use_default_partitions;
  bool use_default_num_partitions;
  /* Only the number of subpartitions defined (uses default names etc.). */
  bool use_default_subpartitions;
  bool use_default_num_subpartitions;
  bool default_partitions_setup;
  bool defined_max_value;
  bool list_of_part_fields;                  // KEY or COLUMNS PARTITIONING
  bool list_of_subpart_fields;               // KEY SUBPARTITIONING
  bool linear_hash_ind;                      // LINEAR HASH/KEY
  bool fixed;
  bool is_auto_partitioned;
  bool has_null_value;
  bool column_list;                          // COLUMNS PARTITIONING, 5.5+
  /**
    True if pruning has been completed and can not be pruned any further,
    even if there are subqueries or stored programs in the condition.

    Some times it is needed to run prune_partitions() a second time to prune
    read partitions after tables are locked, when subquery and
    stored functions might have been evaluated.
  */
  bool is_pruning_completed;

  partition_info()
  : get_partition_id(NULL), get_part_partition_id(NULL),
    get_subpartition_id(NULL),
    part_field_array(NULL), subpart_field_array(NULL),
    part_charset_field_array(NULL),
    subpart_charset_field_array(NULL),
    full_part_field_array(NULL),
    part_field_buffers(NULL), subpart_field_buffers(NULL),
    restore_part_field_ptrs(NULL), restore_subpart_field_ptrs(NULL),
    part_expr(NULL), subpart_expr(NULL), item_free_list(NULL),
    first_log_entry(NULL), exec_log_entry(NULL), frm_log_entry(NULL),
    bitmaps_are_initialized(FALSE),
    list_array(NULL), err_value(0),
    part_info_string(NULL),
    part_func_string(NULL), subpart_func_string(NULL),
    curr_part_elem(NULL), current_partition(NULL),
    curr_list_object(0), num_columns(0), table(NULL),
    default_engine_type(NULL),
    part_type(NOT_A_PARTITION), subpart_type(NOT_A_PARTITION),
    part_info_len(0),
    part_func_len(0), subpart_func_len(0),
    num_parts(0), num_subparts(0),
    count_curr_subparts(0),
    num_list_values(0), num_part_fields(0), num_subpart_fields(0),
    num_full_part_fields(0), has_null_part_id(0), linear_hash_mask(0),
    key_algorithm(KEY_ALGORITHM_NONE),
    use_default_partitions(TRUE), use_default_num_partitions(TRUE),
    use_default_subpartitions(TRUE), use_default_num_subpartitions(TRUE),
    default_partitions_setup(FALSE), defined_max_value(FALSE),
    list_of_part_fields(FALSE), list_of_subpart_fields(FALSE),
    linear_hash_ind(FALSE), fixed(FALSE),
    is_auto_partitioned(FALSE),
    has_null_value(FALSE), column_list(FALSE), is_pruning_completed(false)
  {
    partitions.empty();
    temp_partitions.empty();
    part_field_list.empty();
    subpart_field_list.empty();
  }
  ~partition_info() {}

  partition_info *get_clone(bool reset = false);
  partition_info *get_full_clone();
  bool set_named_partition_bitmap(const char *part_name, size_t length);
  bool set_partition_bitmaps(TABLE_LIST *table_list);
  bool set_read_partitions(List<String> *partition_names);
  /* Answers the question if subpartitioning is used for a certain table */
  inline bool is_sub_partitioned() const
  {
    return (subpart_type == NOT_A_PARTITION ?  FALSE : TRUE);
  }

  /* Returns the total number of partitions on the leaf level */
  inline uint get_tot_partitions() const
  {
    return num_parts * (is_sub_partitioned() ? num_subparts : 1);
  }

  bool set_up_defaults_for_partitioning(Partition_handler *part_handler,
                                        HA_CREATE_INFO *info,
                                        uint start_no);
  char *find_duplicate_field();
  char *find_duplicate_name();
  bool check_engine_mix(handlerton *engine_type, bool default_engine);
  bool check_range_constants(THD *thd);
  bool check_list_constants(THD *thd);
  bool check_partition_info(THD *thd, handlerton **eng_type,
                            handler *file, HA_CREATE_INFO *info,
                            bool check_partition_function);
  void print_no_partition_found(TABLE *table);
  void print_debug(const char *str, uint*);
  Item* get_column_item(Item *item, Field *field);
  bool fix_partition_values(THD *thd,
                            part_elem_value *val,
                            partition_element *part_elem,
                            uint part_id);
  bool fix_column_value_functions(THD *thd,
                                  part_elem_value *val,
                                  uint part_id);
  bool fix_parser_data(THD *thd);
  bool add_max_value();
  void init_col_val(part_column_list_val *col_val, Item *item);
  bool reorganize_into_single_field_col_val();
  part_column_list_val *add_column_value();
  bool set_part_expr(char *start_token, Item *item_ptr,
                     char *end_token, bool is_subpart);
  static int compare_column_values(const void *a, const void *b);
  bool set_up_charset_field_preps();
  bool check_partition_field_length();
  bool init_column_part();
  bool add_column_list_value(THD *thd, Item *item);
  void set_show_version_string(String *packet);
  partition_element *get_part_elem(const char *partition_name,
                                   char *file_name,
                                   uint32 *part_id);
  void report_part_expr_error(bool use_subpart_expr);
  bool set_used_partition(List<Item> &fields,
                          List<Item> &values,
                          COPY_INFO &info,
                          bool copy_default_values,
                          MY_BITMAP *used_partitions);
  /**
    PRUNE_NO - Unable to prune.
    PRUNE_DEFAULTS - Partitioning field is only set to
                     DEFAULT values, only need to check
                     pruning for one row where the DEFAULTS
                     values are set.
    PRUNE_YES - Pruning is possible, calculate the used partition set
                by evaluate the partition_id on row by row basis.
  */
  enum enum_can_prune {PRUNE_NO=0, PRUNE_DEFAULTS, PRUNE_YES};
  bool can_prune_insert(THD *thd,
                        enum_duplicates duplic,
                        COPY_INFO &update,
                        List<Item> &update_fields,
                        List<Item> &fields,
                        bool empty_values,
                        enum_can_prune *can_prune_partitions,
                        bool *prune_needs_default_values,
                        MY_BITMAP *used_partitions);
  bool has_same_partitioning(partition_info *new_part_info);
  inline bool is_partition_used(uint part_id) const
  {
    return bitmap_is_set(&read_partitions, part_id);
  }
  inline bool is_partition_locked(uint part_id) const
  {
    return bitmap_is_set(&lock_partitions, part_id);
  }
  inline uint num_partitions_used()
  {
    return bitmap_bits_set(&read_partitions);
  }
  inline uint get_first_used_partition() const
  {
    return bitmap_get_first_set(&read_partitions);
  }
  inline uint get_next_used_partition(uint part_id) const
  {
    return bitmap_get_next_set(&read_partitions, part_id);
  }
  bool same_key_column_order(List<Create_field> *create_list);

private:
  static int list_part_cmp(const void* a, const void* b);
  bool set_up_default_partitions(Partition_handler *part_handler,
                                 HA_CREATE_INFO *info,
                                 uint start_no);
  bool set_up_default_subpartitions(Partition_handler *part_handler,
                                    HA_CREATE_INFO *info);
  char *create_default_partition_names(uint part_no, uint num_parts,
                                       uint start_no);
  char *create_default_subpartition_name(uint subpart_no,
                                         const char *part_name);
  bool add_named_partition(const char *part_name, size_t length);
  bool is_fields_in_part_expr(List<Item> &fields);
  bool is_full_part_expr_in_fields(List<Item> &fields);
};

uint32 get_next_partition_id_range(struct st_partition_iter* part_iter);
bool check_partition_dirs(partition_info *part_info);

/* Initialize the iterator to return a single partition with given part_id */

static inline void init_single_partition_iterator(uint32 part_id,
                                           PARTITION_ITERATOR *part_iter)
{
  part_iter->part_nums.start= part_iter->part_nums.cur= part_id;
  part_iter->part_nums.end= part_id+1;
  part_iter->ret_null_part= part_iter->ret_null_part_orig= FALSE;
  part_iter->get_next= get_next_partition_id_range;
}

/* Initialize the iterator to enumerate all partitions */
static inline
void init_all_partitions_iterator(partition_info *part_info,
                                  PARTITION_ITERATOR *part_iter)
{
  part_iter->part_nums.start= part_iter->part_nums.cur= 0;
  part_iter->part_nums.end= part_info->num_parts;
  part_iter->ret_null_part= part_iter->ret_null_part_orig= FALSE;
  part_iter->get_next= get_next_partition_id_range;
}

bool fill_partition_tablespace_names(
       partition_info *part_info,
       Tablespace_hash_set *tablespace_set);

bool check_partition_tablespace_names(partition_info *part_info);

/**
  Predicate which returns true if any partition or subpartition uses
  an external data directory or external index directory.

  @param pi partitioning information
  @retval true if any partition or subpartition has an external
  data directory or external index directory.
  @retval false otherwise
 */
bool has_external_data_or_index_dir(partition_info &pi);

#endif /* PARTITION_INFO_INCLUDED */
