/* Copyright (C) 2000,2006 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#include "partition_element.h"

class partition_info;

/* Some function typedefs */
typedef int (*get_part_id_func)(partition_info *part_info,
                                 uint32 *part_id,
                                 longlong *func_value);
typedef uint32 (*get_subpart_id_func)(partition_info *part_info);

struct st_ddl_log_memory_entry;

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
    one such method for get_partition_id and one for
    get_part_partition_id and one for get_subpartition_id.
  */
  get_part_id_func get_partition_id_charset;
  get_part_id_func get_part_partition_id_charset;
  get_subpart_id_func get_subpartition_id_charset;

  /* NULL-terminated array of fields used in partitioned expression */
  Field **part_field_array;
  /* NULL-terminated array of fields used in subpartitioned expression */
  Field **subpart_field_array;

  /* 
    Array of all fields used in partition and subpartition expression,
    without duplicates, NULL-terminated.
  */
  Field **full_part_field_array;

  /*
    When we have a field that requires transformation before calling the
    partition functions we must allocate field buffers for the field of
    the fields in the partition function.
  */
  char **part_field_buffers;
  char **subpart_field_buffers;
  char **restore_part_field_ptrs;
  char **restore_subpart_field_ptrs;

  Item *part_expr;
  Item *subpart_expr;

  Item *item_free_list;

  struct st_ddl_log_memory_entry *first_log_entry;
  struct st_ddl_log_memory_entry *exec_log_entry;
  struct st_ddl_log_memory_entry *frm_log_entry;

  /* 
    A bitmap of partitions used by the current query. 
    Usage pattern:
    * The handler->extra(HA_EXTRA_RESET) call at query start/end sets all
      partitions to be unused.
    * Before index/rnd_init(), partition pruning code sets the bits for used
      partitions.
  */
  MY_BITMAP used_partitions;

  union {
    longlong *range_int_array;
    LIST_PART_ENTRY *list_array;
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
  
  /*
    Valid iff
    get_part_iter_for_interval=get_part_iter_for_interval_via_walking:
      controls how we'll process "field < C" and "field > C" intervals.
      If the partitioning function F is strictly increasing, then for any x, y
      "x < y" => "F(x) < F(y)" (*), i.e. when we get interval "field < C" 
      we can perform partition pruning on the equivalent "F(field) < F(C)".

      If the partitioning function not strictly increasing (it is simply
      increasing), then instead of (*) we get "x < y" => "F(x) <= F(y)"
      i.e. for interval "field < C" we can perform partition pruning for
      "F(field) <= F(C)".
  */
  bool range_analysis_include_bounds;
  /********************************************
   * INTERVAL ANALYSIS ENDS 
   ********************************************/
  
  char* part_info_string;

  char *part_func_string;
  char *subpart_func_string;

  uchar *part_state;

  partition_element *curr_part_elem;
  partition_element *current_partition;
  /*
    These key_map's are used for Partitioning to enable quick decisions
    on whether we can derive more information about which partition to
    scan just by looking at what index is used.
  */
  key_map all_fields_in_PF, all_fields_in_PPF, all_fields_in_SPF;
  key_map some_fields_in_PF;

  handlerton *default_engine_type;
  Item_result part_result_type;
  partition_type part_type;
  partition_type subpart_type;

  uint part_info_len;
  uint part_state_len;
  uint part_func_len;
  uint subpart_func_len;

  uint no_parts;
  uint no_subparts;
  uint count_curr_subparts;

  uint part_error_code;

  uint no_list_values;

  uint no_part_fields;
  uint no_subpart_fields;
  uint no_full_part_fields;

  uint has_null_part_id;
  /*
    This variable is used to calculate the partition id when using
    LINEAR KEY/HASH. This functionality is kept in the MySQL Server
    but mainly of use to handlers supporting partitioning.
  */
  uint16 linear_hash_mask;

  bool use_default_partitions;
  bool use_default_no_partitions;
  bool use_default_subpartitions;
  bool use_default_no_subpartitions;
  bool default_partitions_setup;
  bool defined_max_value;
  bool list_of_part_fields;
  bool list_of_subpart_fields;
  bool linear_hash_ind;
  bool fixed;
  bool is_auto_partitioned;
  bool from_openfrm;
  bool has_null_value;
  bool includes_charset_field_part;
  bool includes_charset_field_subpart;


  partition_info()
  : get_partition_id(NULL), get_part_partition_id(NULL),
    get_subpartition_id(NULL),
    part_field_array(NULL), subpart_field_array(NULL),
    full_part_field_array(NULL),
    part_field_buffers(NULL), subpart_field_buffers(NULL),
    restore_part_field_ptrs(NULL), restore_subpart_field_ptrs(NULL),
    part_expr(NULL), subpart_expr(NULL), item_free_list(NULL),
    first_log_entry(NULL), exec_log_entry(NULL), frm_log_entry(NULL),
    list_array(NULL),
    part_info_string(NULL),
    part_func_string(NULL), subpart_func_string(NULL),
    part_state(NULL),
    curr_part_elem(NULL), current_partition(NULL),
    default_engine_type(NULL),
    part_result_type(INT_RESULT),
    part_type(NOT_A_PARTITION), subpart_type(NOT_A_PARTITION),
    part_info_len(0), part_state_len(0),
    part_func_len(0), subpart_func_len(0),
    no_parts(0), no_subparts(0),
    count_curr_subparts(0), part_error_code(0),
    no_list_values(0), no_part_fields(0), no_subpart_fields(0),
    no_full_part_fields(0), has_null_part_id(0), linear_hash_mask(0),
    use_default_partitions(TRUE), use_default_no_partitions(TRUE),
    use_default_subpartitions(TRUE), use_default_no_subpartitions(TRUE),
    default_partitions_setup(FALSE), defined_max_value(FALSE),
    list_of_part_fields(FALSE), list_of_subpart_fields(FALSE),
    linear_hash_ind(FALSE), fixed(FALSE),
    is_auto_partitioned(FALSE), from_openfrm(FALSE),
    has_null_value(FALSE), includes_charset_field_part(FALSE),
    includes_charset_field_subpart(FALSE)
  {
    all_fields_in_PF.clear_all();
    all_fields_in_PPF.clear_all();
    all_fields_in_SPF.clear_all();
    some_fields_in_PF.clear_all();
    partitions.empty();
    temp_partitions.empty();
    part_field_list.empty();
    subpart_field_list.empty();
  }
  ~partition_info() {}

  partition_info *get_clone();
  /* Answers the question if subpartitioning is used for a certain table */
  bool is_sub_partitioned()
  {
    return (subpart_type == NOT_A_PARTITION ?  FALSE : TRUE);
  }

  /* Returns the total number of partitions on the leaf level */
  uint get_tot_partitions()
  {
    return no_parts * (is_sub_partitioned() ? no_subparts : 1);
  }

  bool set_up_defaults_for_partitioning(handler *file, HA_CREATE_INFO *info,
                                        uint start_no);
  char *has_unique_names();
  static bool check_engine_mix(handlerton **engine_array, uint no_parts);
  bool check_range_constants();
  bool check_list_constants();
  bool check_partition_info(THD *thd, handlerton **eng_type,
                            handler *file, HA_CREATE_INFO *info,
                            bool check_partition_function);
  void print_no_partition_found(TABLE *table);
private:
  static int list_part_cmp(const void* a, const void* b);
  static int list_part_cmp_unsigned(const void* a, const void* b);
  bool set_up_default_partitions(handler *file, HA_CREATE_INFO *info,
                                 uint start_no);
  bool set_up_default_subpartitions(handler *file, HA_CREATE_INFO *info);
  char *create_default_partition_names(uint part_no, uint no_parts,
                                       uint start_no);
  char *create_subpartition_name(uint subpart_no, const char *part_name);
  bool has_unique_name(partition_element *element);
};

uint32 get_next_partition_id_range(struct st_partition_iter* part_iter);

/* Initialize the iterator to return a single partition with given part_id */

static inline void init_single_partition_iterator(uint32 part_id,
                                           PARTITION_ITERATOR *part_iter)
{
  part_iter->part_nums.start= part_iter->part_nums.cur= part_id;
  part_iter->part_nums.end= part_id+1;
  part_iter->get_next= get_next_partition_id_range;
}

/* Initialize the iterator to enumerate all partitions */
static inline
void init_all_partitions_iterator(partition_info *part_info,
                                  PARTITION_ITERATOR *part_iter)
{
  part_iter->part_nums.start= part_iter->part_nums.cur= 0;
  part_iter->part_nums.end= part_info->no_parts;
  part_iter->get_next= get_next_partition_id_range;
}
