#ifndef SQL_PARTITION_INCLUDED
#define SQL_PARTITION_INCLUDED

/* Copyright (c) 2006, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"

#include "partition_element.h"       // partition_state
#include "lock.h"                    // Tablespace_hash_set

class Alter_info;
class Alter_table_ctx;
class Field;
class Item;
class String;
class handler;
class partition_info;
class THD;
struct handlerton;
struct TABLE;
struct TABLE_SHARE;
struct TABLE_LIST;
typedef struct charset_info_st CHARSET_INFO;
typedef struct st_bitmap MY_BITMAP;
typedef struct st_ha_create_information HA_CREATE_INFO;
typedef struct st_key KEY;
typedef struct st_key_range key_range;
typedef struct st_mysql_lex_string LEX_STRING;
template <class T> class List;

/* Flags for partition handlers */
/*
  Removed HA_CAN_PARTITION (1 << 0) since if handlerton::partition_flags
  is set, then it implies that it have partitioning support.
*/
#define HA_CAN_UPDATE_PARTITION_KEY (1 << 1)
#define HA_CAN_PARTITION_UNIQUE (1 << 2)
/* If the engine will use auto partitioning even when not defined. */
#define HA_USE_AUTO_PARTITION (1 << 3)
/**
  The handler can exchange a partition with a non-partitioned table
  of the same handlerton/engine.
*/
#define HA_CAN_EXCHANGE_PARTITION (1 << 4)
/**
  The handler can not use FOREIGN KEYS with partitioning
*/
#define HA_CANNOT_PARTITION_FK (1 << 5)

#define NORMAL_PART_NAME 0
#define TEMP_PART_NAME 1
#define RENAMED_PART_NAME 2

typedef struct st_lock_param_type
{
  TABLE_LIST *table_list;
  ulonglong copied;
  ulonglong deleted;
  THD *thd;
  HA_CREATE_INFO *create_info;
  Alter_info *alter_info;
  TABLE *table;
  KEY *key_info_buffer;
  const char *db;
  const char *table_name;
  uchar *pack_frm_data;
  uint key_count;
  uint db_options;
  size_t pack_frm_len;
  partition_info *part_info;
} ALTER_PARTITION_PARAM_TYPE;

typedef struct {
  uint32 start_part;
  uint32 end_part;
} part_id_range;

bool is_partition_in_list(char *part_name, List<char> list_part_names);
char *are_partitions_in_table(partition_info *new_part_info,
                              partition_info *old_part_info);
int get_parts_for_update(const uchar *old_data, uchar *new_data,
                         const uchar *rec0, partition_info *part_info,
                         uint32 *old_part_id, uint32 *new_part_id,
                         longlong *func_value);
int get_part_for_delete(const uchar *buf, const uchar *rec0,
                        partition_info *part_info, uint32 *part_id);
void prune_partition_set(const TABLE *table, part_id_range *part_spec);
bool check_partition_info(partition_info *part_info,handlerton **eng_type,
                          TABLE *table, handler *file, HA_CREATE_INFO *info);
void set_linear_hash_mask(partition_info *part_info, uint num_parts);
bool fix_partition_func(THD *thd, TABLE *table, bool create_table_ind);
bool partition_key_modified(TABLE *table, const MY_BITMAP *fields);
void get_partition_set(const TABLE *table, uchar *buf, const uint index,
                       const key_range *key_spec,
                       part_id_range *part_spec);
uint get_partition_field_store_length(Field *field);
int get_cs_converted_part_value_from_string(THD *thd,
                                            Item *item,
                                            String *input_str,
                                            String *output_str,
                                            const CHARSET_INFO *cs,
                                            bool use_hex);
void get_full_part_id_from_key(const TABLE *table, uchar *buf,
                               KEY *key_info,
                               const key_range *key_spec,
                               part_id_range *part_spec);
bool get_partition_tablespace_names(
       THD *thd,
       const char *partition_info_str,
       uint partition_info_len,
       Tablespace_hash_set *tablespace_set);
bool mysql_unpack_partition(THD *thd, char *part_buf,
                            uint part_info_len,
                            TABLE *table, bool is_create_table_ind,
                            handlerton *default_db_type,
                            bool *work_part_info_used);
bool make_used_partitions_str(partition_info *part_info,
                              List<const char> *parts);
uint32 get_list_array_idx_for_endpoint(partition_info *part_info,
                                       bool left_endpoint,
                                       bool include_endpoint);
uint32 get_partition_id_range_for_endpoint(partition_info *part_info,
                                           bool left_endpoint,
                                           bool include_endpoint);
bool check_part_func_fields(Field **ptr, bool ok_with_charsets);
bool field_is_partition_charset(Field *field);
Item* convert_charset_partition_constant(Item *item, const CHARSET_INFO *cs);
/**
  Append all fields in read_set to string

  @param[in,out] str   String to append to.
  @param[in]     row   Row to append.
  @param[in]     table Table containing read_set and fields for the row.
*/
void append_row_to_str(String &str, const uchar *row, TABLE *table);
void mem_alloc_error(size_t size);
void truncate_partition_filename(MEM_ROOT* root, const char **path);

bool fast_alter_partition_table(THD *thd,
                                TABLE *table,
                                Alter_info *alter_info,
                                HA_CREATE_INFO *create_info,
                                TABLE_LIST *table_list,
                                char *db,
                                const char *table_name,
                                partition_info *new_part_info);
bool set_part_state(Alter_info *alter_info,
                    partition_info *tab_part_info,
                    enum partition_state part_state,
                    bool include_subpartitions);
void set_all_part_state(partition_info *tab_part_info,
                        enum partition_state part_state);
uint prep_alter_part_table(THD *thd, TABLE *table, Alter_info *alter_info,
                           HA_CREATE_INFO *create_info,
                           Alter_table_ctx *alter_ctx,
                           bool *partition_changed,
                           partition_info **new_part_info);
char *generate_partition_syntax(partition_info *part_info,
                                uint *buf_length, bool use_sql_alloc,
                                bool show_partition_options,
                                HA_CREATE_INFO *create_info,
                                Alter_info *alter_info,
                                const char *current_comment_start);
bool verify_data_with_partition(TABLE *table, TABLE *part_table,
                                uint32 part_id);
bool compare_partition_options(HA_CREATE_INFO *table_create_info,
                               partition_element *part_elem);

void create_partition_name(char *out, const char *in1,
                           const char *in2, uint name_variant,
                           bool translate);
void create_subpartition_name(char *out, const char *in1,
                              const char *in2, const char *in3,
                              uint name_variant);
void set_field_ptr(Field **ptr, const uchar *new_buf, const uchar *old_buf);
void set_key_field_ptr(KEY *key_info, const uchar *new_buf,
                       const uchar *old_buf);
/** Set up table for creating a partition.
Copy info from partition to the table share so the created partition
has the correct info.
  @param thd               THD object
  @param share             Table share to be updated.
  @param info              Create info to be updated.
  @param part_elem         partition_element containing the info.

  @return    status
    @retval  TRUE  Error
    @retval  FALSE Success

  @details
    Set up
    1) Comment on partition
    2) MAX_ROWS, MIN_ROWS on partition
    3) Index file name on partition
    4) Data file name on partition
*/
bool set_up_table_before_create(THD *thd,
                                TABLE_SHARE *share,
                                const char *partition_name_with_path,
                                HA_CREATE_INFO *info,
                                partition_element *part_elem);

enum enum_partition_keywords
{
  PKW_HASH= 0, PKW_RANGE, PKW_LIST, PKW_KEY, PKW_MAXVALUE, PKW_LINEAR,
  PKW_COLUMNS, PKW_ALGORITHM
};

extern const LEX_STRING partition_keywords[];

#endif /* SQL_PARTITION_INCLUDED */
