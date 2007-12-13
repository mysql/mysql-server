/* Copyright (C) 2006 MySQL AB

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifdef __GNUC__
#pragma interface				/* gcc class implementation */
#endif

/* Flags for partition handlers */
#define HA_CAN_PARTITION       (1 << 0) /* Partition support */
#define HA_CAN_UPDATE_PARTITION_KEY (1 << 1)
#define HA_CAN_PARTITION_UNIQUE (1 << 2)
#define HA_USE_AUTO_PARTITION (1 << 3)

/*typedef struct {
  ulonglong data_file_length;
  ulonglong max_data_file_length;
  ulonglong index_file_length;
  ulonglong delete_length;
  ha_rows records;
  ulong mean_rec_length;
  time_t create_time;
  time_t check_time;
  time_t update_time;
  ulonglong check_sum;
} PARTITION_INFO;
*/
typedef struct {
  longlong list_value;
  uint32 partition_id;
} LIST_PART_ENTRY;

typedef struct {
  uint32 start_part;
  uint32 end_part;
} part_id_range;

struct st_partition_iter;
#define NOT_A_PARTITION_ID ((uint32)-1)

bool is_partition_in_list(char *part_name, List<char> list_part_names);
char *are_partitions_in_table(partition_info *new_part_info,
                              partition_info *old_part_info);
bool check_reorganise_list(partition_info *new_part_info,
                           partition_info *old_part_info,
                           List<char> list_part_names);
handler *get_ha_partition(partition_info *part_info);
int get_parts_for_update(const uchar *old_data, uchar *new_data,
                         const uchar *rec0, partition_info *part_info,
                         uint32 *old_part_id, uint32 *new_part_id,
                         longlong *func_value);
int get_part_for_delete(const uchar *buf, const uchar *rec0,
                        partition_info *part_info, uint32 *part_id);
void prune_partition_set(const TABLE *table, part_id_range *part_spec);
bool check_partition_info(partition_info *part_info,handlerton **eng_type,
                          TABLE *table, handler *file, HA_CREATE_INFO *info);
void set_linear_hash_mask(partition_info *part_info, uint no_parts);
bool fix_partition_func(THD *thd, TABLE *table, bool create_table_ind);
char *generate_partition_syntax(partition_info *part_info,
                                uint *buf_length, bool use_sql_alloc,
                                bool show_partition_options);
bool partition_key_modified(TABLE *table, const MY_BITMAP *fields);
void get_partition_set(const TABLE *table, uchar *buf, const uint index,
                       const key_range *key_spec,
                       part_id_range *part_spec);
void get_full_part_id_from_key(const TABLE *table, uchar *buf,
                               KEY *key_info,
                               const key_range *key_spec,
                               part_id_range *part_spec);
bool mysql_unpack_partition(THD *thd, const char *part_buf,
                            uint part_info_len,
                            const char *part_state, uint part_state_len,
                            TABLE *table, bool is_create_table_ind,
                            handlerton *default_db_type,
                            bool *work_part_info_used);
void make_used_partitions_str(partition_info *part_info, String *parts_str);
uint32 get_list_array_idx_for_endpoint(partition_info *part_info,
                                       bool left_endpoint,
                                       bool include_endpoint);
uint32 get_partition_id_range_for_endpoint(partition_info *part_info,
                                           bool left_endpoint,
                                           bool include_endpoint);
bool fix_fields_part_func(THD *thd, Item* func_expr, TABLE *table,
                          bool is_sub_part, bool is_field_to_be_setup);

bool check_part_func_fields(Field **ptr, bool ok_with_charsets);
bool field_is_partition_charset(Field *field);

/*
  A "Get next" function for partition iterator.

  SYNOPSIS
    partition_iter_func()
      part_iter  Partition iterator, you call only "iter.get_next(&iter)"

  DESCRIPTION
    Depending on whether partitions or sub-partitions are iterated, the
    function returns next subpartition id/partition number. The sequence of
    returned numbers is not ordered and may contain duplicates.

    When the end of sequence is reached, NOT_A_PARTITION_ID is returned, and 
    the iterator resets itself (so next get_next() call will start to 
    enumerate the set all over again).

  RETURN 
    NOT_A_PARTITION_ID if there are no more partitions.
    [sub]partition_id  of the next partition
*/

typedef uint32 (*partition_iter_func)(st_partition_iter* part_iter);


/*
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


/*
  Get an iterator for set of partitions that match given field-space interval

  SYNOPSIS
    get_partitions_in_range_iter()
      part_info   Partitioning info
      is_subpart  
      min_val     Left edge,  field value in opt_range_key format.
      max_val     Right edge, field value in opt_range_key format. 
      flags       Some combination of NEAR_MIN, NEAR_MAX, NO_MIN_RANGE,
                  NO_MAX_RANGE.
      part_iter   Iterator structure to be initialized

  DESCRIPTION
    Functions with this signature are used to perform "Partitioning Interval
    Analysis". This analysis is applicable for any type of [sub]partitioning 
    by some function of a single fieldX. The idea is as follows:
    Given an interval "const1 <=? fieldX <=? const2", find a set of partitions
    that may contain records with value of fieldX within the given interval.

    The min_val, max_val and flags parameters specify the interval.
    The set of partitions is returned by initializing an iterator in *part_iter

  NOTES
    There are currently two functions of this type:
     - get_part_iter_for_interval_via_walking
     - get_part_iter_for_interval_via_mapping

  RETURN 
    0 - No matching partitions, iterator not initialized
    1 - Some partitions would match, iterator intialized for traversing them
   -1 - All partitions would match, iterator not initialized
*/

typedef int (*get_partitions_in_range_iter)(partition_info *part_info,
                                            bool is_subpart,
                                            uchar *min_val, uchar *max_val,
                                            uint flags,
                                            PARTITION_ITERATOR *part_iter);

#include "partition_info.h"

