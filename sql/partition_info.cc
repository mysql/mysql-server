/* Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.

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

/* Some general useful functions */

#include "sql_priv.h"
// Required to get server definitions for mysql/plugin.h right
#include "sql_plugin.h"
#include "sql_partition.h"                 // partition_info.h: LIST_PART_ENTRY
                                           // NOT_A_PARTITION_ID
#include "partition_info.h"
#include "sql_parse.h"                        // test_if_data_home_dir
#include "sql_acl.h"                          // *_ACL
#include "table.h"                            // TABLE_LIST
#include "my_bitmap.h"                        // bitmap*
#include "sql_base.h"                         // fill_record

#ifdef WITH_PARTITION_STORAGE_ENGINE
#include "ha_partition.h"


partition_info *partition_info::get_clone()
{
  DBUG_ENTER("partition_info::get_clone");
  if (!this)
    DBUG_RETURN(NULL);
  List_iterator<partition_element> part_it(partitions);
  partition_element *part;
  partition_info *clone= new partition_info();
  if (!clone)
  {
    mem_alloc_error(sizeof(partition_info));
    DBUG_RETURN(NULL);
  }
  memcpy(clone, this, sizeof(partition_info));
  memset(&(clone->read_partitions), 0, sizeof(clone->read_partitions));
  memset(&(clone->lock_partitions), 0, sizeof(clone->lock_partitions));
  clone->bitmaps_are_initialized= FALSE;
  clone->partitions.empty();

  while ((part= (part_it++)))
  {
    List_iterator<partition_element> subpart_it(part->subpartitions);
    partition_element *subpart;
    partition_element *part_clone= new partition_element();
    if (!part_clone)
    {
      mem_alloc_error(sizeof(partition_element));
      DBUG_RETURN(NULL);
    }
    memcpy(part_clone, part, sizeof(partition_element));
    part_clone->subpartitions.empty();
    while ((subpart= (subpart_it++)))
    {
      partition_element *subpart_clone= new partition_element();
      if (!subpart_clone)
      {
        mem_alloc_error(sizeof(partition_element));
        DBUG_RETURN(NULL);
      }
      memcpy(subpart_clone, subpart, sizeof(partition_element));
      part_clone->subpartitions.push_back(subpart_clone);
    }
    clone->partitions.push_back(part_clone);
  }
  DBUG_RETURN(clone);
}


/**
  Mark named [sub]partition to be used/locked.

  @param part_name  Partition name to match.
  @param length     Partition name length.

  @return Success if partition found
    @retval true  Partition found
    @retval false Partition not found
*/

bool partition_info::add_named_partition(const char *part_name,
                                         uint length)
{
  HASH *part_name_hash;
  PART_NAME_DEF *part_def;
  Partition_share *part_share;
  DBUG_ENTER("partition_info::add_named_partition");
  DBUG_ASSERT(table && table->s && table->s->ha_share);
  part_share= static_cast<Partition_share*>((table->s->ha_share));
  DBUG_ASSERT(part_share->partition_name_hash_initialized);
  part_name_hash= &part_share->partition_name_hash;
  DBUG_ASSERT(part_name_hash->records);

  part_def= (PART_NAME_DEF*) my_hash_search(part_name_hash,
                                            (const uchar*) part_name,
                                            length);
  if (!part_def)
  {
    my_error(ER_UNKNOWN_PARTITION, MYF(0), part_name, table->alias);
    DBUG_RETURN(true);
  }

  if (part_def->is_subpart)
  {
    bitmap_set_bit(&read_partitions, part_def->part_id);
  }
  else
  {
    if (is_sub_partitioned())
    {
      /* Mark all subpartitions in the partition */
      uint j, start= part_def->part_id;
      uint end= start + num_subparts;
      for (j= start; j < end; j++)
        bitmap_set_bit(&read_partitions, j);
    }
    else
      bitmap_set_bit(&read_partitions, part_def->part_id);
  }
  DBUG_PRINT("info", ("Found partition %u is_subpart %d for name %s",
                      part_def->part_id, part_def->is_subpart,
                      part_name));
  DBUG_RETURN(false);
}


/**
  Mark named [sub]partition to be used/locked.

  @param part_elem  Partition element that matched.
*/

bool partition_info::set_named_partition_bitmap(const char *part_name,
                                                uint length)
{
  DBUG_ENTER("partition_info::set_named_partition_bitmap");
  bitmap_clear_all(&read_partitions);
  if (add_named_partition(part_name, length))
    DBUG_RETURN(true);
  bitmap_copy(&lock_partitions, &read_partitions);
  DBUG_RETURN(false);
}



/**
  Prune away partitions not mentioned in the PARTITION () clause,
  if used.

    @param table_list  Table list pointing to table to prune.

  @return Operation status
    @retval true  Failure
    @retval false Success
*/
bool partition_info::prune_partition_bitmaps(TABLE_LIST *table_list)
{
  List_iterator<String> partition_names_it(*(table_list->partition_names));
  uint num_names= table_list->partition_names->elements;
  uint i= 0;
  DBUG_ENTER("partition_info::prune_partition_bitmaps");

  if (num_names < 1)
    DBUG_RETURN(true);

  /*
    TODO: When adding support for FK in partitioned tables, the referenced
    table must probably lock all partitions for read, and also write depending
    of ON DELETE/UPDATE.
  */
  bitmap_clear_all(&read_partitions);

  /* No check for duplicate names or overlapping partitions/subpartitions. */

  DBUG_PRINT("info", ("Searching through partition_name_hash"));
  do
  {
    String *part_name_str= partition_names_it++;
    if (add_named_partition(part_name_str->c_ptr(), part_name_str->length()))
      DBUG_RETURN(true);
  } while (++i < num_names);
  DBUG_RETURN(false);
}


/**
  Set read/lock_partitions bitmap over non pruned partitions

  @param table_list   Possible TABLE_LIST which can contain
                      list of partition names to query

  @return Operation status
    @retval FALSE  OK
    @retval TRUE   Failed to allocate memory for bitmap or list of partitions
                   did not match

  @note OK to call multiple times without the need for free_bitmaps.
*/

bool partition_info::set_partition_bitmaps(TABLE_LIST *table_list)
{
  DBUG_ENTER("partition_info::set_partition_bitmaps");

  DBUG_ASSERT(bitmaps_are_initialized);
  DBUG_ASSERT(table);
  is_pruning_completed= false;
  if (!bitmaps_are_initialized)
    DBUG_RETURN(TRUE);

  if (table_list &&
      table_list->partition_names &&
      table_list->partition_names->elements)
  {
    if (table->s->db_type()->partition_flags() & HA_USE_AUTO_PARTITION)
    {
        /*
          Don't allow PARTITION () clause on a NDB tables yet.
          TODO: Add partition name handling to NDB/partition_info.
          which is currently ha_partition specific.
        */
        my_error(ER_PARTITION_CLAUSE_ON_NONPARTITIONED, MYF(0));
        DBUG_RETURN(true);
    }
    if (prune_partition_bitmaps(table_list))
      DBUG_RETURN(TRUE);
  }
  else
  {
    bitmap_set_all(&read_partitions);
    DBUG_PRINT("info", ("Set all partitions"));
  }
  bitmap_copy(&lock_partitions, &read_partitions);
  DBUG_ASSERT(bitmap_get_first_set(&lock_partitions) != MY_BIT_NONE);
  DBUG_RETURN(FALSE);
}


/**
  Checks if possible to do prune partitions on insert.

  @param thd           Thread context
  @param duplic        How to handle duplicates
  @param update        In case of ON DUPLICATE UPDATE, default function fields
  @param update_fields In case of ON DUPLICATE UPDATE, which fields to update
  @param fields        Listed fields
  @param empty_values  True if values is empty (only defaults)
  @param[out] prune_needs_default_values  Set on return if copying of default
                                          values is needed
  @param[out] can_prune_partitions        Enum showing if possible to prune
  @param[inout] used_partitions           If possible to prune the bitmap
                                          is initialized and cleared

  @return Operation status
    @retval false  Success
    @retval true   Failure
*/

bool partition_info::can_prune_insert(THD* thd,
                                      enum_duplicates duplic,
                                      COPY_INFO &update,
                                      List<Item> &update_fields,
                                      List<Item> &fields,
                                      bool empty_values,
                                      enum_can_prune *can_prune_partitions,
                                      bool *prune_needs_default_values,
                                      MY_BITMAP *used_partitions)
{
  uint32 *bitmap_buf;
  uint bitmap_bytes;
  uint num_partitions= 0;
  *can_prune_partitions= PRUNE_NO;
  DBUG_ASSERT(bitmaps_are_initialized);
  DBUG_ENTER("partition_info::can_prune_insert");

  if (table->s->db_type()->partition_flags() & HA_USE_AUTO_PARTITION)
    DBUG_RETURN(false); /* Should not insert prune NDB tables */

  /*
    If under LOCK TABLES pruning will skip start_stmt instead of external_lock
    for unused partitions.

    Cannot prune if there are BEFORE INSERT triggers that changes any
    partitioning column, since they may change the row to be in another
    partition.
  */
  if (table->triggers &&
      table->triggers->has_triggers(TRG_EVENT_INSERT, TRG_ACTION_BEFORE) &&
      table->triggers->is_fields_updated_in_trigger(&full_part_field_set,
                                                    TRG_EVENT_INSERT,
                                                    TRG_ACTION_BEFORE))
    DBUG_RETURN(false);

  if (table->found_next_number_field)
  {
    /*
      If the field is used in the partitioning expression, we cannot prune.
      TODO: If all rows have not null values and
      is not 0 (with NO_AUTO_VALUE_ON_ZERO sql_mode), then pruning is possible!
    */
    if (bitmap_is_set(&full_part_field_set,
        table->found_next_number_field->field_index))
      DBUG_RETURN(false);
  }

  /*
    If updating a field in the partitioning expression, we cannot prune.

    Note: TIMESTAMP_AUTO_SET_ON_INSERT is handled by converting Item_null
    to the start time of the statement. Which will be the same as in
    write_row(). So pruning of TIMESTAMP DEFAULT CURRENT_TIME will work.
    But TIMESTAMP_AUTO_SET_ON_UPDATE cannot be pruned if the timestamp
    column is a part of any part/subpart expression.
  */
  if (duplic == DUP_UPDATE)
  {
    /*
      Cannot prune if any field in the partitioning expression can
      be updated by ON DUPLICATE UPDATE.
    */
    if (update.function_defaults_apply_on_columns(&full_part_field_set))
      DBUG_RETURN(false);
 
    /*
      TODO: add check for static update values, which can be pruned.
    */
    if (is_field_in_part_expr(update_fields))
      DBUG_RETURN(false);

    /*
      Cannot prune if there are BEFORE UPDATE triggers that changes any
      partitioning column, since they may change the row to be in another
      partition.
    */
    if (table->triggers &&
        table->triggers->has_triggers(TRG_EVENT_UPDATE,
                                      TRG_ACTION_BEFORE) &&
        table->triggers->is_fields_updated_in_trigger(&full_part_field_set,
                                                      TRG_EVENT_UPDATE,
                                                      TRG_ACTION_BEFORE))
    {
      DBUG_RETURN(false);
    }
  }

  /*
    If not all partitioning fields are given,
    we also must set all non given partitioning fields
    to get correct defaults.
    TODO: If any gain, we could enhance this by only copy the needed default
    fields by
      1) check which fields needs to be set.
      2) only copy those fields from the default record.
  */
  *prune_needs_default_values= false;
  if (fields.elements)
  {
    if (!is_full_part_expr_in_fields(fields))
      *prune_needs_default_values= true;
  }
  else if (empty_values)
  {
    *prune_needs_default_values= true; // like 'INSERT INTO t () VALUES ()'
  }
  else
  {
     /*
       In case of INSERT INTO t VALUES (...) we must get values for
       all fields in table from VALUES (...) part, so no defaults
       are needed.
     */
  }

  /* Pruning possible, have to initialize the used_partitions bitmap. */
  num_partitions= lock_partitions.n_bits;
  bitmap_bytes= bitmap_buffer_size(num_partitions);
  if (!(bitmap_buf= (uint32*) thd->alloc(bitmap_bytes)))
  {
    mem_alloc_error(bitmap_bytes);
    DBUG_RETURN(true);
  }
  /* Also clears all bits. */
  if (bitmap_init(used_partitions, bitmap_buf, num_partitions, false))
  {
    /* purecov: begin deadcode */
    /* Cannot happen, due to pre-alloc. */
    mem_alloc_error(bitmap_bytes);
    DBUG_RETURN(true);
    /* purecov: end */
  }
  /*
    If no partitioning field in set (e.g. defaults) check pruning only once.
  */
  if (fields.elements &&
      !is_field_in_part_expr(fields))
    *can_prune_partitions= PRUNE_DEFAULTS;
  else
    *can_prune_partitions= PRUNE_YES;

  DBUG_RETURN(false);
}


/**
  Mark the partition, the record belongs to, as used.

  @param fields           Fields to set
  @param values           Values to use
  @param info             COPY_INFO used for default values handling
  @param copy_default_values  True if we should copy default values
  @param used_partitions  Bitmap to set

  @returns Operational status
    @retval false  Success
    @retval true   Failure
*/

bool partition_info::set_used_partition(List<Item> &fields,
                                        List<Item> &values,
                                        COPY_INFO &info,
                                        bool copy_default_values,
                                        MY_BITMAP *used_partitions)
{
  THD *thd= table->in_use;
  uint32 part_id;
  longlong func_value;
  Dummy_error_handler error_handler;
  bool ret= true;
  DBUG_ENTER("set_partition");
  DBUG_ASSERT(thd);

  /* Only allow checking of constant values */
  List_iterator_fast<Item> v(values);
  Item *item;
  thd->push_internal_handler(&error_handler);
  while ((item= v++))
  {
    if (!item->const_item())
      goto err;
  }

  if (copy_default_values)
    restore_record(table,s->default_values);

  if (fields.elements || !values.elements)
  {
    if (fill_record(thd, fields, values, false, &full_part_field_set))
      goto err;
  }
  else
  {
    if (fill_record(thd, table->field, values, false, &full_part_field_set))
      goto err;
  }
  DBUG_ASSERT(!table->auto_increment_field_not_null);

  /*
    Evaluate DEFAULT functions like CURRENT_TIMESTAMP.
    TODO: avoid setting non partitioning fields default value, to avoid
    overhead. Not yet done, since mostly only one DEFAULT function per
    table, or at least very few such columns.
  */
  if (info.function_defaults_apply_on_columns(&full_part_field_set))
    info.set_function_defaults(table);

  {
    /*
      This function is used in INSERT; 'values' are supplied by user,
      or are default values, not values read from a table, so read_set is
      irrelevant.
    */
    my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
    const int rc= get_partition_id(this, &part_id, &func_value);
    dbug_tmp_restore_column_map(table->read_set, old_map);
    if (rc)
      goto err;
  }

  DBUG_PRINT("info", ("Insert into partition %u", part_id));
  bitmap_set_bit(used_partitions, part_id);
  ret= false;

err:
  thd->pop_internal_handler();
  DBUG_RETURN(ret);
}


/*
  Create a memory area where default partition names are stored and fill it
  up with the names.

  SYNOPSIS
    create_default_partition_names()
    part_no                         Partition number for subparts
    num_parts                       Number of partitions
    start_no                        Starting partition number
    subpart                         Is it subpartitions

  RETURN VALUE
    A pointer to the memory area of the default partition names

  DESCRIPTION
    A support routine for the partition code where default values are
    generated.
    The external routine needing this code is check_partition_info
*/

#define MAX_PART_NAME_SIZE 8

char *partition_info::create_default_partition_names(uint part_no,
                                                     uint num_parts_arg,
                                                     uint start_no)
{
  char *ptr= (char*) sql_calloc(num_parts_arg*MAX_PART_NAME_SIZE);
  char *move_ptr= ptr;
  uint i= 0;
  DBUG_ENTER("create_default_partition_names");

  if (likely(ptr != 0))
  {
    do
    {
      sprintf(move_ptr, "p%u", (start_no + i));
      move_ptr+= MAX_PART_NAME_SIZE;
    } while (++i < num_parts_arg);
  }
  else
  {
    mem_alloc_error(num_parts_arg*MAX_PART_NAME_SIZE);
  }
  DBUG_RETURN(ptr);
}


/*
  Generate a version string for partition expression
  This function must be updated every time there is a possibility for
  a new function of a higher version number than 5.5.0.

  SYNOPSIS
    set_show_version_string()
  RETURN VALUES
    None
*/
void partition_info::set_show_version_string(String *packet)
{
  int version= 0;
  if (column_list)
    packet->append(STRING_WITH_LEN("\n/*!50500"));
  else
  {
    if (part_expr)
      part_expr->walk(&Item::intro_version, 0, (uchar*)&version);
    if (subpart_expr)
      subpart_expr->walk(&Item::intro_version, 0, (uchar*)&version);
    if (version == 0)
    {
      /* No new functions in partition function */
      packet->append(STRING_WITH_LEN("\n/*!50100"));
    }
    else
    {
      char buf[65];
      char *buf_ptr= longlong10_to_str((longlong)version, buf, 10);
      packet->append(STRING_WITH_LEN("\n/*!"));
      packet->append(buf, (size_t)(buf_ptr - buf));
    }
  }
}

/*
  Create a unique name for the subpartition as part_name'sp''subpart_no'
  SYNOPSIS
    create_default_subpartition_name()
    subpart_no                  Number of subpartition
    part_name                   Name of partition
  RETURN VALUES
    >0                          A reference to the created name string
    0                           Memory allocation error
*/

char *partition_info::create_default_subpartition_name(uint subpart_no,
                                               const char *part_name)
{
  uint size_alloc= strlen(part_name) + MAX_PART_NAME_SIZE;
  char *ptr= (char*) sql_calloc(size_alloc);
  DBUG_ENTER("create_default_subpartition_name");

  if (likely(ptr != NULL))
  {
    my_snprintf(ptr, size_alloc, "%ssp%u", part_name, subpart_no);
  }
  else
  {
    mem_alloc_error(size_alloc);
  }
  DBUG_RETURN(ptr);
}


/*
  Set up all the default partitions not set-up by the user in the SQL
  statement. Also perform a number of checks that the user hasn't tried
  to use default values where no defaults exists.

  SYNOPSIS
    set_up_default_partitions()
    file                A reference to a handler of the table
    info                Create info
    start_no            Starting partition number

  RETURN VALUE
    TRUE                Error, attempted default values not possible
    FALSE               Ok, default partitions set-up

  DESCRIPTION
    The routine uses the underlying handler of the partitioning to define
    the default number of partitions. For some handlers this requires
    knowledge of the maximum number of rows to be stored in the table.
    This routine only accepts HASH and KEY partitioning and thus there is
    no subpartitioning if this routine is successful.
    The external routine needing this code is check_partition_info
*/

bool partition_info::set_up_default_partitions(handler *file,
                                               HA_CREATE_INFO *info,
                                               uint start_no)
{
  uint i;
  char *default_name;
  bool result= TRUE;
  DBUG_ENTER("partition_info::set_up_default_partitions");

  if (part_type != HASH_PARTITION)
  {
    const char *error_string;
    if (part_type == RANGE_PARTITION)
      error_string= partition_keywords[PKW_RANGE].str;
    else
      error_string= partition_keywords[PKW_LIST].str;
    my_error(ER_PARTITIONS_MUST_BE_DEFINED_ERROR, MYF(0), error_string);
    goto end;
  }

  if ((num_parts == 0) &&
      ((num_parts= file->get_default_no_partitions(info)) == 0))
  {
    my_error(ER_PARTITION_NOT_DEFINED_ERROR, MYF(0), "partitions");
    goto end;
  }

  if (unlikely(num_parts > MAX_PARTITIONS))
  {
    my_error(ER_TOO_MANY_PARTITIONS_ERROR, MYF(0));
    goto end;
  }
  if (unlikely((!(default_name= create_default_partition_names(0, num_parts,
                                                               start_no)))))
    goto end;
  i= 0;
  do
  {
    partition_element *part_elem= new partition_element();
    if (likely(part_elem != 0 &&
               (!partitions.push_back(part_elem))))
    {
      part_elem->engine_type= default_engine_type;
      part_elem->partition_name= default_name;
      default_name+=MAX_PART_NAME_SIZE;
    }
    else
    {
      mem_alloc_error(sizeof(partition_element));
      goto end;
    }
  } while (++i < num_parts);
  result= FALSE;
end:
  DBUG_RETURN(result);
}


/*
  Set up all the default subpartitions not set-up by the user in the SQL
  statement. Also perform a number of checks that the default partitioning
  becomes an allowed partitioning scheme.

  SYNOPSIS
    set_up_default_subpartitions()
    file                A reference to a handler of the table
    info                Create info

  RETURN VALUE
    TRUE                Error, attempted default values not possible
    FALSE               Ok, default partitions set-up

  DESCRIPTION
    The routine uses the underlying handler of the partitioning to define
    the default number of partitions. For some handlers this requires
    knowledge of the maximum number of rows to be stored in the table.
    This routine is only called for RANGE or LIST partitioning and those
    need to be specified so only subpartitions are specified.
    The external routine needing this code is check_partition_info
*/

bool partition_info::set_up_default_subpartitions(handler *file, 
                                                  HA_CREATE_INFO *info)
{
  uint i, j;
  bool result= TRUE;
  partition_element *part_elem;
  List_iterator<partition_element> part_it(partitions);
  DBUG_ENTER("partition_info::set_up_default_subpartitions");

  if (num_subparts == 0)
    num_subparts= file->get_default_no_partitions(info);
  if (unlikely((num_parts * num_subparts) > MAX_PARTITIONS))
  {
    my_error(ER_TOO_MANY_PARTITIONS_ERROR, MYF(0));
    goto end;
  }
  i= 0;
  do
  {
    part_elem= part_it++;
    j= 0;
    do
    {
      partition_element *subpart_elem= new partition_element(part_elem);
      if (likely(subpart_elem != 0 &&
          (!part_elem->subpartitions.push_back(subpart_elem))))
      {
        char *ptr= create_default_subpartition_name(j,
                                                    part_elem->partition_name);
        if (!ptr)
          goto end;
        subpart_elem->engine_type= default_engine_type;
        subpart_elem->partition_name= ptr;
      }
      else
      {
        mem_alloc_error(sizeof(partition_element));
        goto end;
      }
    } while (++j < num_subparts);
  } while (++i < num_parts);
  result= FALSE;
end:
  DBUG_RETURN(result);
}


/*
  Support routine for check_partition_info

  SYNOPSIS
    set_up_defaults_for_partitioning()
    file                A reference to a handler of the table
    info                Create info
    start_no            Starting partition number

  RETURN VALUE
    TRUE                Error, attempted default values not possible
    FALSE               Ok, default partitions set-up

  DESCRIPTION
    Set up defaults for partition or subpartition (cannot set-up for both,
    this will return an error.
*/

bool partition_info::set_up_defaults_for_partitioning(handler *file,
                                                      HA_CREATE_INFO *info, 
                                                      uint start_no)
{
  DBUG_ENTER("partition_info::set_up_defaults_for_partitioning");

  if (!default_partitions_setup)
  {
    default_partitions_setup= TRUE;
    if (use_default_partitions)
      DBUG_RETURN(set_up_default_partitions(file, info, start_no));
    if (is_sub_partitioned() && 
        use_default_subpartitions)
      DBUG_RETURN(set_up_default_subpartitions(file, info));
  }
  DBUG_RETURN(FALSE);
}


/*
  Support routine for check_partition_info

  SYNOPSIS
    find_duplicate_field
    no parameters

  RETURN VALUE
    Erroneus field name  Error, there are two fields with same name
    NULL                 Ok, no field defined twice

  DESCRIPTION
    Check that the user haven't defined the same field twice in
    key or column list partitioning.
*/
char* partition_info::find_duplicate_field()
{
  char *field_name_outer, *field_name_inner;
  List_iterator<char> it_outer(part_field_list);
  uint num_fields= part_field_list.elements;
  uint i,j;
  DBUG_ENTER("partition_info::find_duplicate_field");

  for (i= 0; i < num_fields; i++)
  {
    field_name_outer= it_outer++;
    List_iterator<char> it_inner(part_field_list);
    for (j= 0; j < num_fields; j++)
    {
      field_name_inner= it_inner++;
      if (i >= j)
        continue;
      if (!(my_strcasecmp(system_charset_info,
                          field_name_outer,
                          field_name_inner)))
      {
        DBUG_RETURN(field_name_outer);
      }
    }
  }
  DBUG_RETURN(NULL);
}


/**
  @brief Get part_elem and part_id from partition name

  @param partition_name Name of partition to search for.
  @param file_name[out] Partition file name (part after table name,
                        #P#<part>[#SP#<subpart>]), skipped if NULL.
  @param part_id[out]   Id of found partition or NOT_A_PARTITION_ID.

  @retval Pointer to part_elem of [sub]partition, if not found NULL

  @note Since names of partitions AND subpartitions must be unique,
  this function searches both partitions and subpartitions and if name of
  a partition is given for a subpartitioned table, part_elem will be
  the partition, but part_id will be NOT_A_PARTITION_ID and file_name not set.
*/
partition_element *partition_info::get_part_elem(const char *partition_name,
                                                 char *file_name,
                                                 uint32 *part_id)
{
  List_iterator<partition_element> part_it(partitions);
  uint i= 0;
  DBUG_ENTER("partition_info::get_part_elem");
  DBUG_ASSERT(part_id);
  *part_id= NOT_A_PARTITION_ID;
  do
  {
    partition_element *part_elem= part_it++;
    if (is_sub_partitioned())
    {
      List_iterator<partition_element> sub_part_it(part_elem->subpartitions);
      uint j= 0;
      do
      {
        partition_element *sub_part_elem= sub_part_it++;
        if (!my_strcasecmp(system_charset_info,
                           sub_part_elem->partition_name, partition_name))
        {
          if (file_name)
            create_subpartition_name(file_name, "",
                                     part_elem->partition_name,
                                     partition_name,
                                     NORMAL_PART_NAME);
          *part_id= j + (i * num_subparts);
          DBUG_RETURN(sub_part_elem);
        }
      } while (++j < num_subparts);

      /* Naming a partition (first level) on a subpartitioned table. */
      if (!my_strcasecmp(system_charset_info,
                            part_elem->partition_name, partition_name))
        DBUG_RETURN(part_elem);
    }
    else if (!my_strcasecmp(system_charset_info,
                            part_elem->partition_name, partition_name))
    {
      if (file_name)
        create_partition_name(file_name, "", partition_name,
                              NORMAL_PART_NAME, TRUE);
      *part_id= i;
      DBUG_RETURN(part_elem);
    }
  } while (++i < num_parts);
  DBUG_RETURN(NULL);
}


/**
  Helper function to find_duplicate_name.
*/

static const char *get_part_name_from_elem(const char *name, size_t *length,
                                      my_bool not_used __attribute__((unused)))
{
  *length= strlen(name);
  return name;
}

/*
  A support function to check partition names for duplication in a
  partitioned table

  SYNOPSIS
    find_duplicate_name()

  RETURN VALUES
    NULL               Has unique part and subpart names
    !NULL              Pointer to duplicated name

  DESCRIPTION
    Checks that the list of names in the partitions doesn't contain any
    duplicated names.
*/

char *partition_info::find_duplicate_name()
{
  HASH partition_names;
  uint max_names;
  const uchar *curr_name= NULL;
  List_iterator<partition_element> parts_it(partitions);
  partition_element *p_elem;

  DBUG_ENTER("partition_info::find_duplicate_name");

  /*
    TODO: If table->s->ha_part_data->partition_name_hash.elements is > 0,
    then we could just return NULL, but that has not been verified.
    And this only happens when in ALTER TABLE with full table copy.
  */

  max_names= num_parts;
  if (is_sub_partitioned())
    max_names+= num_parts * num_subparts;
  if (my_hash_init(&partition_names, system_charset_info, max_names, 0, 0,
                   (my_hash_get_key) get_part_name_from_elem, 0, HASH_UNIQUE))
  {
    DBUG_ASSERT(0);
    curr_name= (const uchar*) "Internal failure";
    goto error;
  }
  while ((p_elem= (parts_it++)))
  {
    curr_name= (const uchar*) p_elem->partition_name;
    if (my_hash_insert(&partition_names, curr_name))
      goto error;

    if (!p_elem->subpartitions.is_empty())
    {
      List_iterator<partition_element> subparts_it(p_elem->subpartitions);
      partition_element *subp_elem;
      while ((subp_elem= (subparts_it++)))
      {
        curr_name= (const uchar*) subp_elem->partition_name;
        if (my_hash_insert(&partition_names, curr_name))
          goto error;
      }
    }
  }
  my_hash_free(&partition_names);
  DBUG_RETURN(NULL);
error:
  my_hash_free(&partition_names);
  DBUG_RETURN((char*) curr_name);
}


/*
  Check that the partition/subpartition is setup to use the correct
  storage engine
  SYNOPSIS
    check_engine_condition()
    p_elem                   Partition element
    table_engine_set         Have user specified engine on table level
    inout::engine_type       Current engine used
    inout::first             Is it first partition
  RETURN VALUE
    TRUE                     Failed check
    FALSE                    Ok
  DESCRIPTION
    Specified engine for table and partitions p0 and pn
    Must be correct both on CREATE and ALTER commands
    table p0 pn res (0 - OK, 1 - FAIL)
        -  -  - 0
        -  -  x 1
        -  x  - 1
        -  x  x 0
        x  -  - 0
        x  -  x 0
        x  x  - 0
        x  x  x 0
    i.e:
    - All subpartitions must use the same engine
      AND it must be the same as the partition.
    - All partitions must use the same engine
      AND it must be the same as the table.
    - if one does NOT specify an engine on the table level
      then one must either NOT specify any engine on any
      partition/subpartition OR for ALL partitions/subpartitions
    Note:
    When ALTER a table, the engines are already set for all levels
    (table, all partitions and subpartitions). So if one want to
    change the storage engine, one must specify it on the table level

*/

static bool check_engine_condition(partition_element *p_elem,
                                   bool table_engine_set,
                                   handlerton **engine_type,
                                   bool *first)
{
  DBUG_ENTER("check_engine_condition");

  DBUG_PRINT("enter", ("p_eng %s t_eng %s t_eng_set %u first %u state %u",
                       ha_resolve_storage_engine_name(p_elem->engine_type),
                       ha_resolve_storage_engine_name(*engine_type),
                       table_engine_set, *first, p_elem->part_state));
  if (*first && !table_engine_set)
  {
    *engine_type= p_elem->engine_type;
    DBUG_PRINT("info", ("setting table_engine = %s",
                         ha_resolve_storage_engine_name(*engine_type)));
  }
  *first= FALSE;
  if ((table_engine_set &&
      (p_elem->engine_type != (*engine_type) &&
       p_elem->engine_type)) ||
      (!table_engine_set &&
       p_elem->engine_type != (*engine_type)))
  {
    DBUG_RETURN(TRUE);
  }

  DBUG_RETURN(FALSE);
}


/*
  Check engine mix that it is correct
  Current limitation is that all partitions and subpartitions
  must use the same storage engine.
  SYNOPSIS
    check_engine_mix()
    inout::engine_type       Current engine used
    table_engine_set         Have user specified engine on table level
  RETURN VALUE
    TRUE                     Error, mixed engines
    FALSE                    Ok, no mixed engines
  DESCRIPTION
    Current check verifies only that all handlers are the same.
    Later this check will be more sophisticated.
    (specified partition handler ) specified table handler
    (NDB, NDB) NDB           OK
    (MYISAM, MYISAM) -       OK
    (MYISAM, -)      -       NOT OK
    (MYISAM, -)    MYISAM    OK
    (- , MYISAM)   -         NOT OK
    (- , -)        MYISAM    OK
    (-,-)          -         OK
    (NDB, MYISAM) *          NOT OK
*/

bool partition_info::check_engine_mix(handlerton *engine_type,
                                      bool table_engine_set)
{
  handlerton *old_engine_type= engine_type;
  bool first= TRUE;
  uint n_parts= partitions.elements;
  DBUG_ENTER("partition_info::check_engine_mix");
  DBUG_PRINT("info", ("in: engine_type = %s, table_engine_set = %u",
                       ha_resolve_storage_engine_name(engine_type),
                       table_engine_set));
  if (n_parts)
  {
    List_iterator<partition_element> part_it(partitions);
    uint i= 0;
    do
    {
      partition_element *part_elem= part_it++;
      DBUG_PRINT("info", ("part = %d engine = %s table_engine_set %u",
                 i, ha_resolve_storage_engine_name(part_elem->engine_type),
                 table_engine_set));
      if (is_sub_partitioned() &&
          part_elem->subpartitions.elements)
      {
        uint n_subparts= part_elem->subpartitions.elements;
        uint j= 0;
        List_iterator<partition_element> sub_it(part_elem->subpartitions);
        do
        {
          partition_element *sub_elem= sub_it++;
          DBUG_PRINT("info", ("sub = %d engine = %s table_engie_set %u",
                     j, ha_resolve_storage_engine_name(sub_elem->engine_type),
                     table_engine_set));
          if (check_engine_condition(sub_elem, table_engine_set,
                                     &engine_type, &first))
            goto error;
        } while (++j < n_subparts);
        /* ensure that the partition also has correct engine */
        if (check_engine_condition(part_elem, table_engine_set,
                                   &engine_type, &first))
          goto error;
      }
      else if (check_engine_condition(part_elem, table_engine_set,
                                      &engine_type, &first))
        goto error;
    } while (++i < n_parts);
  }
  DBUG_PRINT("info", ("engine_type = %s",
                       ha_resolve_storage_engine_name(engine_type)));
  if (!engine_type)
    engine_type= old_engine_type;
  if (engine_type->flags & HTON_NO_PARTITION)
  {
    my_error(ER_PARTITION_MERGE_ERROR, MYF(0));
    DBUG_RETURN(TRUE);
  }
  DBUG_PRINT("info", ("out: engine_type = %s",
                       ha_resolve_storage_engine_name(engine_type)));
  DBUG_ASSERT(engine_type != partition_hton);
  DBUG_RETURN(FALSE);
error:
  /*
    Mixed engines not yet supported but when supported it will need
    the partition handler
  */
  DBUG_RETURN(TRUE);
}


/*
  This routine allocates an array for all range constants to achieve a fast
  check what partition a certain value belongs to. At the same time it does
  also check that the range constants are defined in increasing order and
  that the expressions are constant integer expressions.

  SYNOPSIS
    check_range_constants()
    thd                          Thread object

  RETURN VALUE
    TRUE                An error occurred during creation of range constants
    FALSE               Successful creation of range constant mapping

  DESCRIPTION
    This routine is called from check_partition_info to get a quick error
    before we came too far into the CREATE TABLE process. It is also called
    from fix_partition_func every time we open the .frm file. It is only
    called for RANGE PARTITIONed tables.
*/

bool partition_info::check_range_constants(THD *thd)
{
  partition_element* part_def;
  bool first= TRUE;
  uint i;
  List_iterator<partition_element> it(partitions);
  int result= TRUE;
  DBUG_ENTER("partition_info::check_range_constants");
  DBUG_PRINT("enter", ("RANGE with %d parts, column_list = %u", num_parts,
                                                         column_list));

  if (column_list)
  {
    part_column_list_val *loc_range_col_array;
    part_column_list_val *UNINIT_VAR(current_largest_col_val);
    uint num_column_values= part_field_list.elements;
    uint size_entries= sizeof(part_column_list_val) * num_column_values;
    range_col_array= (part_column_list_val*)sql_calloc(num_parts *
                                                       size_entries);
    if (unlikely(range_col_array == NULL))
    {
      mem_alloc_error(num_parts * size_entries);
      goto end;
    }
    loc_range_col_array= range_col_array;
    i= 0;
    do
    {
      part_def= it++;
      {
        List_iterator<part_elem_value> list_val_it(part_def->list_val_list);
        part_elem_value *range_val= list_val_it++;
        part_column_list_val *col_val= range_val->col_val_array;
        DBUG_ASSERT(part_def->list_val_list.elements == 1);

        if (fix_column_value_functions(thd, range_val, i))
          goto end;
        memcpy(loc_range_col_array, (const void*)col_val, size_entries);
        loc_range_col_array+= num_column_values;
        if (!first)
        {
          if (compare_column_values((const void*)current_largest_col_val,
                                    (const void*)col_val) >= 0)
            goto range_not_increasing_error;
        }
        current_largest_col_val= col_val;
      }
      first= FALSE;
    } while (++i < num_parts);
  }
  else
  {
    longlong UNINIT_VAR(current_largest);
    longlong part_range_value;
    bool signed_flag= !part_expr->unsigned_flag;

    range_int_array= (longlong*)sql_alloc(num_parts * sizeof(longlong));
    if (unlikely(range_int_array == NULL))
    {
      mem_alloc_error(num_parts * sizeof(longlong));
      goto end;
    }
    i= 0;
    do
    {
      part_def= it++;
      if ((i != (num_parts - 1)) || !defined_max_value)
      {
        part_range_value= part_def->range_value;
        if (!signed_flag)
          part_range_value-= 0x8000000000000000ULL;
      }
      else
        part_range_value= LONGLONG_MAX;

      if (!first)
      {
        if (unlikely(current_largest > part_range_value) ||
            (unlikely(current_largest == part_range_value) &&
            (part_range_value < LONGLONG_MAX ||
             i != (num_parts - 1) ||
             !defined_max_value)))
          goto range_not_increasing_error;
      }
      range_int_array[i]= part_range_value;
      current_largest= part_range_value;
      first= FALSE;
    } while (++i < num_parts);
  }
  result= FALSE;
end:
  DBUG_RETURN(result);

range_not_increasing_error:
  my_error(ER_RANGE_NOT_INCREASING_ERROR, MYF(0));
  goto end;
}


/*
  Support routines for check_list_constants used by qsort to sort the
  constant list expressions. One routine for integers and one for
  column lists.

  SYNOPSIS
    list_part_cmp()
      a                First list constant to compare with
      b                Second list constant to compare with

  RETURN VALUE
    +1                 a > b
    0                  a  == b
    -1                 a < b
*/

extern "C"
int partition_info_list_part_cmp(const void* a, const void* b)
{
  longlong a1= ((LIST_PART_ENTRY*)a)->list_value;
  longlong b1= ((LIST_PART_ENTRY*)b)->list_value;
  if (a1 < b1)
    return -1;
  else if (a1 > b1)
    return +1;
  else
    return 0;
}


int partition_info::list_part_cmp(const void* a, const void* b)
{
  return partition_info_list_part_cmp(a, b);
}


/*
  Compare two lists of column values in RANGE/LIST partitioning
  SYNOPSIS
    compare_column_values()
    first                    First column list argument
    second                   Second column list argument
  RETURN VALUES
    0                        Equal
    -1                       First argument is smaller
    +1                       First argument is larger
*/

extern "C"
int partition_info_compare_column_values(const void *first_arg,
                                         const void *second_arg)
{
  const part_column_list_val *first= (part_column_list_val*)first_arg;
  const part_column_list_val *second= (part_column_list_val*)second_arg;
  partition_info *part_info= first->part_info;
  Field **field;

  for (field= part_info->part_field_array; *field;
       field++, first++, second++)
  {
    if (first->max_value || second->max_value)
    {
      if (first->max_value && second->max_value)
        return 0;
      if (second->max_value)
        return -1;
      else
        return +1;
    }
    if (first->null_value || second->null_value)
    {
      if (first->null_value && second->null_value)
        continue;
      if (second->null_value)
        return +1;
      else
        return -1;
    }
    int res= (*field)->cmp((const uchar*)first->column_value,
                           (const uchar*)second->column_value);
    if (res)
      return res;
  }
  return 0;
}


int partition_info::compare_column_values(const void *first_arg,
                                          const void *second_arg)
{
  return partition_info_compare_column_values(first_arg, second_arg);
}


/*
  This routine allocates an array for all list constants to achieve a fast
  check what partition a certain value belongs to. At the same time it does
  also check that there are no duplicates among the list constants and that
  that the list expressions are constant integer expressions.

  SYNOPSIS
    check_list_constants()
    thd                            Thread object

  RETURN VALUE
    TRUE                  An error occurred during creation of list constants
    FALSE                 Successful creation of list constant mapping

  DESCRIPTION
    This routine is called from check_partition_info to get a quick error
    before we came too far into the CREATE TABLE process. It is also called
    from fix_partition_func every time we open the .frm file. It is only
    called for LIST PARTITIONed tables.
*/

bool partition_info::check_list_constants(THD *thd)
{
  uint i, size_entries, num_column_values;
  uint list_index= 0;
  part_elem_value *list_value;
  bool result= TRUE;
  longlong type_add, calc_value;
  void *curr_value;
  void *UNINIT_VAR(prev_value);
  partition_element* part_def;
  bool found_null= FALSE;
  qsort_cmp compare_func;
  void *ptr;
  List_iterator<partition_element> list_func_it(partitions);
  DBUG_ENTER("partition_info::check_list_constants");

  num_list_values= 0;
  /*
    We begin by calculating the number of list values that have been
    defined in the first step.

    We use this number to allocate a properly sized array of structs
    to keep the partition id and the value to use in that partition.
    In the second traversal we assign them values in the struct array.

    Finally we sort the array of structs in order of values to enable
    a quick binary search for the proper value to discover the
    partition id.
    After sorting the array we check that there are no duplicates in the
    list.
  */

  i= 0;
  do
  {
    part_def= list_func_it++;
    if (part_def->has_null_value)
    {
      if (found_null)
      {
        my_error(ER_MULTIPLE_DEF_CONST_IN_LIST_PART_ERROR, MYF(0));
        goto end;
      }
      has_null_value= TRUE;
      has_null_part_id= i;
      found_null= TRUE;
    }
    List_iterator<part_elem_value> list_val_it1(part_def->list_val_list);
    while (list_val_it1++)
      num_list_values++;
  } while (++i < num_parts);
  list_func_it.rewind();
  num_column_values= part_field_list.elements;
  size_entries= column_list ?
        (num_column_values * sizeof(part_column_list_val)) :
        sizeof(LIST_PART_ENTRY);
  ptr= sql_calloc((num_list_values+1) * size_entries);
  if (unlikely(ptr == NULL))
  {
    mem_alloc_error(num_list_values * size_entries);
    goto end;
  }
  if (column_list)
  {
    part_column_list_val *loc_list_col_array;
    loc_list_col_array= (part_column_list_val*)ptr;
    list_col_array= (part_column_list_val*)ptr;
    compare_func= partition_info_compare_column_values;
    i= 0;
    do
    {
      part_def= list_func_it++;
      List_iterator<part_elem_value> list_val_it2(part_def->list_val_list);
      while ((list_value= list_val_it2++))
      {
        part_column_list_val *col_val= list_value->col_val_array;
        if (unlikely(fix_column_value_functions(thd, list_value, i)))
        {
          DBUG_RETURN(TRUE);
        }
        memcpy(loc_list_col_array, (const void*)col_val, size_entries);
        loc_list_col_array+= num_column_values;
      }
    } while (++i < num_parts);
  }
  else
  {
    compare_func= partition_info_list_part_cmp;
    list_array= (LIST_PART_ENTRY*)ptr;
    i= 0;
    /*
      Fix to be able to reuse signed sort functions also for unsigned
      partition functions.
    */
    type_add= (longlong)(part_expr->unsigned_flag ?
                                       0x8000000000000000ULL :
                                       0ULL);

    do
    {
      part_def= list_func_it++;
      List_iterator<part_elem_value> list_val_it2(part_def->list_val_list);
      while ((list_value= list_val_it2++))
      {
        calc_value= list_value->value - type_add;
        list_array[list_index].list_value= calc_value;
        list_array[list_index++].partition_id= i;
      }
    } while (++i < num_parts);
  }
  DBUG_ASSERT(fixed);
  if (num_list_values)
  {
    bool first= TRUE;
    /*
      list_array and list_col_array are unions, so this works for both
      variants of LIST partitioning.
    */
    my_qsort((void*)list_array, num_list_values, size_entries,
             compare_func);

    i= 0;
    do
    {
      DBUG_ASSERT(i < num_list_values);
      curr_value= column_list ? (void*)&list_col_array[num_column_values * i] :
                                (void*)&list_array[i];
      if (likely(first || compare_func(curr_value, prev_value)))
      {
        prev_value= curr_value;
        first= FALSE;
      }
      else
      {
        my_error(ER_MULTIPLE_DEF_CONST_IN_LIST_PART_ERROR, MYF(0));
        goto end;
      }
    } while (++i < num_list_values);
  }
  result= FALSE;
end:
  DBUG_RETURN(result);
}

/**
  Check if we allow DATA/INDEX DIRECTORY, if not warn and set them to NULL.

  @param thd  THD also containing sql_mode (looks from MODE_NO_DIR_IN_CREATE).
  @param part_elem partition_element to check.
*/
static void warn_if_dir_in_part_elem(THD *thd, partition_element *part_elem)
{
  if (thd->variables.sql_mode & MODE_NO_DIR_IN_CREATE)
  {
    if (part_elem->data_file_name)
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                          WARN_OPTION_IGNORED, ER(WARN_OPTION_IGNORED),
                          "DATA DIRECTORY");
    if (part_elem->index_file_name)
      push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                          WARN_OPTION_IGNORED, ER(WARN_OPTION_IGNORED),
                          "INDEX DIRECTORY");
    part_elem->data_file_name= part_elem->index_file_name= NULL;
  }
}


/*
  This code is used early in the CREATE TABLE and ALTER TABLE process.

  SYNOPSIS
    check_partition_info()
    thd                 Thread object
    eng_type            Return value for used engine in partitions
    file                A reference to a handler of the table
    info                Create info
    add_or_reorg_part   Is it ALTER TABLE ADD/REORGANIZE command

  RETURN VALUE
    TRUE                 Error, something went wrong
    FALSE                Ok, full partition data structures are now generated

  DESCRIPTION
    We will check that the partition info requested is possible to set-up in
    this version. This routine is an extension of the parser one could say.
    If defaults were used we will generate default data structures for all
    partitions.

*/

bool partition_info::check_partition_info(THD *thd, handlerton **eng_type,
                                          handler *file, HA_CREATE_INFO *info,
                                          bool add_or_reorg_part)
{
  handlerton *table_engine= default_engine_type;
  uint i, tot_partitions;
  bool result= TRUE, table_engine_set;
  char *same_name;
  DBUG_ENTER("partition_info::check_partition_info");
  DBUG_ASSERT(default_engine_type != partition_hton);

  DBUG_PRINT("info", ("default table_engine = %s",
                      ha_resolve_storage_engine_name(table_engine)));
  if (!add_or_reorg_part)
  {
    int err= 0;

    if (!list_of_part_fields)
    {
      DBUG_ASSERT(part_expr);
      err= part_expr->walk(&Item::check_partition_func_processor, 0,
                           NULL);
      if (!err && is_sub_partitioned() && !list_of_subpart_fields)
        err= subpart_expr->walk(&Item::check_partition_func_processor, 0,
                                NULL);
    }
    if (err)
    {
      my_error(ER_PARTITION_FUNCTION_IS_NOT_ALLOWED, MYF(0));
      goto end;
    }
    if (thd->lex->sql_command == SQLCOM_CREATE_TABLE &&
        fix_parser_data(thd))
      goto end;
  }
  if (unlikely(!is_sub_partitioned() && 
               !(use_default_subpartitions && use_default_num_subpartitions)))
  {
    my_error(ER_SUBPARTITION_ERROR, MYF(0));
    goto end;
  }
  if (unlikely(is_sub_partitioned() &&
              (!(part_type == RANGE_PARTITION || 
                 part_type == LIST_PARTITION))))
  {
    /* Only RANGE and LIST partitioning can be subpartitioned */
    my_error(ER_SUBPARTITION_ERROR, MYF(0));
    goto end;
  }
  if (unlikely(set_up_defaults_for_partitioning(file, info, (uint)0)))
    goto end;
  if (!(tot_partitions= get_tot_partitions()))
  {
    my_error(ER_PARTITION_NOT_DEFINED_ERROR, MYF(0), "partitions");
    goto end;
  }
  if (unlikely(tot_partitions > MAX_PARTITIONS))
  {
    my_error(ER_TOO_MANY_PARTITIONS_ERROR, MYF(0));
    goto end;
  }
  /*
    if NOT specified ENGINE = <engine>:
      If Create, always use create_info->db_type
      else, use previous tables db_type 
      either ALL or NONE partition should be set to
      default_engine_type when not table_engine_set
      Note: after a table is created its storage engines for
      the table and all partitions/subpartitions are set.
      So when ALTER it is already set on table level
  */
  if (info && info->used_fields & HA_CREATE_USED_ENGINE)
  {
    table_engine_set= TRUE;
    table_engine= info->db_type;
    /* if partition_hton, use thd->lex->create_info */
    if (table_engine == partition_hton)
      table_engine= thd->lex->create_info.db_type;
    DBUG_ASSERT(table_engine != partition_hton);
    DBUG_PRINT("info", ("Using table_engine = %s",
                        ha_resolve_storage_engine_name(table_engine)));
  }
  else
  {
    table_engine_set= FALSE;
    if (thd->lex->sql_command != SQLCOM_CREATE_TABLE)
    {
      table_engine_set= TRUE;
      DBUG_PRINT("info", ("No create, table_engine = %s",
                          ha_resolve_storage_engine_name(table_engine)));
      DBUG_ASSERT(table_engine && table_engine != partition_hton);
    }
  }

  if (part_field_list.elements > 0 &&
      (same_name= find_duplicate_field()))
  {
    my_error(ER_SAME_NAME_PARTITION_FIELD, MYF(0), same_name);
    goto end;
  }
  if ((same_name= find_duplicate_name()))
  {
    my_error(ER_SAME_NAME_PARTITION, MYF(0), same_name);
    goto end;
  }
  i= 0;
  {
    List_iterator<partition_element> part_it(partitions);
    uint num_parts_not_set= 0;
    uint prev_num_subparts_not_set= num_subparts + 1;
    do
    {
      partition_element *part_elem= part_it++;
      warn_if_dir_in_part_elem(thd, part_elem);
      if (!is_sub_partitioned())
      {
        if (part_elem->engine_type == NULL)
        {
          num_parts_not_set++;
          part_elem->engine_type= default_engine_type;
        }
        enum_ident_name_check ident_check_status=
          check_table_name(part_elem->partition_name,
                           strlen(part_elem->partition_name), FALSE);
        if (ident_check_status == IDENT_NAME_WRONG)
        {
          my_error(ER_WRONG_PARTITION_NAME, MYF(0));
          goto end;
        }
        else if (ident_check_status == IDENT_NAME_TOO_LONG)
        {
          my_error(ER_TOO_LONG_IDENT, MYF(0));
          goto end;
        }
        DBUG_PRINT("info", ("part = %d engine = %s",
                   i, ha_resolve_storage_engine_name(part_elem->engine_type)));
      }
      else
      {
        uint j= 0;
        uint num_subparts_not_set= 0;
        List_iterator<partition_element> sub_it(part_elem->subpartitions);
        partition_element *sub_elem;
        do
        {
          sub_elem= sub_it++;
          warn_if_dir_in_part_elem(thd, sub_elem);
          enum_ident_name_check ident_check_status=
            check_table_name(sub_elem->partition_name,
                             strlen(sub_elem->partition_name), FALSE);
          if (ident_check_status == IDENT_NAME_WRONG)
          {
            my_error(ER_WRONG_PARTITION_NAME, MYF(0));
            goto end;
          }
          else if (ident_check_status == IDENT_NAME_TOO_LONG)
          {
            my_error(ER_TOO_LONG_IDENT, MYF(0));
            goto end;
          }
          if (sub_elem->engine_type == NULL)
          {
            if (part_elem->engine_type != NULL)
              sub_elem->engine_type= part_elem->engine_type;
            else
            {
              sub_elem->engine_type= default_engine_type;
              num_subparts_not_set++;
            }
          }
          DBUG_PRINT("info", ("part = %d sub = %d engine = %s", i, j,
                     ha_resolve_storage_engine_name(sub_elem->engine_type)));
        } while (++j < num_subparts);

        if (prev_num_subparts_not_set == (num_subparts + 1) &&
            (num_subparts_not_set == 0 ||
             num_subparts_not_set == num_subparts))
          prev_num_subparts_not_set= num_subparts_not_set;

        if (!table_engine_set &&
            prev_num_subparts_not_set != num_subparts_not_set)
        {
          DBUG_PRINT("info", ("num_subparts_not_set = %u num_subparts = %u",
                     num_subparts_not_set, num_subparts));
          my_error(ER_MIX_HANDLER_ERROR, MYF(0));
          goto end;
        }

        if (part_elem->engine_type == NULL)
        {
          if (num_subparts_not_set == 0)
            part_elem->engine_type= sub_elem->engine_type;
          else
          {
            num_parts_not_set++;
            part_elem->engine_type= default_engine_type;
          }
        }
      }
    } while (++i < num_parts);
    if (!table_engine_set &&
        num_parts_not_set != 0 &&
        num_parts_not_set != num_parts)
    {
      DBUG_PRINT("info", ("num_parts_not_set = %u num_parts = %u",
                 num_parts_not_set, num_subparts));
      my_error(ER_MIX_HANDLER_ERROR, MYF(0));
      goto end;
    }
  }
  if (unlikely(check_engine_mix(table_engine, table_engine_set)))
  {
    my_error(ER_MIX_HANDLER_ERROR, MYF(0));
    goto end;
  }

  DBUG_ASSERT(table_engine != partition_hton &&
              default_engine_type == table_engine);
  if (eng_type)
    *eng_type= table_engine;


  /*
    We need to check all constant expressions that they are of the correct
    type and that they are increasing for ranges and not overlapping for
    list constants.
  */

  if (add_or_reorg_part)
  {
    if (unlikely((part_type == RANGE_PARTITION &&
                  check_range_constants(thd)) ||
                 (part_type == LIST_PARTITION &&
                  check_list_constants(thd))))
      goto end;
  }
  result= FALSE;
end:
  DBUG_RETURN(result);
}


/*
  Print error for no partition found

  SYNOPSIS
    print_no_partition_found()
    table                        Table object

  RETURN VALUES
*/

void partition_info::print_no_partition_found(TABLE *table_arg)
{
  char buf[100];
  char *buf_ptr= (char*)&buf;
  TABLE_LIST table_list;

  memset(&table_list, 0, sizeof(table_list));
  table_list.db= table_arg->s->db.str;
  table_list.table_name= table_arg->s->table_name.str;

  if (check_single_table_access(current_thd,
                                SELECT_ACL, &table_list, TRUE))
  {
    my_message(ER_NO_PARTITION_FOR_GIVEN_VALUE,
               ER(ER_NO_PARTITION_FOR_GIVEN_VALUE_SILENT), MYF(0));
  }
  else
  {
    if (column_list)
      buf_ptr= (char*)"from column_list";
    else
    {
      my_bitmap_map *old_map= dbug_tmp_use_all_columns(table_arg, table_arg->read_set);
      if (part_expr->null_value)
        buf_ptr= (char*)"NULL";
      else
        longlong2str(err_value, buf,
                     part_expr->unsigned_flag ? 10 : -10);
      dbug_tmp_restore_column_map(table_arg->read_set, old_map);
    }
    my_error(ER_NO_PARTITION_FOR_GIVEN_VALUE, MYF(0), buf_ptr);
  }
}


/*
  Set fields related to partition expression
  SYNOPSIS
    set_part_expr()
    start_token               Start of partition function string
    item_ptr                  Pointer to item tree
    end_token                 End of partition function string
    is_subpart                Subpartition indicator
  RETURN VALUES
    TRUE                      Memory allocation error
    FALSE                     Success
*/

bool partition_info::set_part_expr(char *start_token, Item *item_ptr,
                                   char *end_token, bool is_subpart)
{
  uint expr_len= end_token - start_token;
  char *func_string= (char*) sql_memdup(start_token, expr_len);

  if (!func_string)
  {
    mem_alloc_error(expr_len);
    return TRUE;
  }
  if (is_subpart)
  {
    list_of_subpart_fields= FALSE;
    subpart_expr= item_ptr;
    subpart_func_string= func_string;
    subpart_func_len= expr_len;
  }
  else
  {
    list_of_part_fields= FALSE;
    part_expr= item_ptr;
    part_func_string= func_string;
    part_func_len= expr_len;
  }
  return FALSE;
}


/*
  Check that partition fields and subpartition fields are not too long

  SYNOPSIS
    check_partition_field_length()

  RETURN VALUES
    TRUE                             Total length was too big
    FALSE                            Length is ok
*/

bool partition_info::check_partition_field_length()
{
  uint store_length= 0;
  uint i;
  DBUG_ENTER("partition_info::check_partition_field_length");

  for (i= 0; i < num_part_fields; i++)
    store_length+= get_partition_field_store_length(part_field_array[i]);
  if (store_length > MAX_KEY_LENGTH)
    DBUG_RETURN(TRUE);
  store_length= 0;
  for (i= 0; i < num_subpart_fields; i++)
    store_length+= get_partition_field_store_length(subpart_field_array[i]);
  if (store_length > MAX_KEY_LENGTH)
    DBUG_RETURN(TRUE);
  DBUG_RETURN(FALSE);
}


/*
  Set up buffers and arrays for fields requiring preparation
  SYNOPSIS
    set_up_charset_field_preps()

  RETURN VALUES
    TRUE                             Memory Allocation error
    FALSE                            Success

  DESCRIPTION
    Set up arrays and buffers for fields that require special care for
    calculation of partition id. This is used for string fields with
    variable length or string fields with fixed length that isn't using
    the binary collation.
*/

bool partition_info::set_up_charset_field_preps()
{
  Field *field, **ptr;
  uchar **char_ptrs;
  unsigned i;
  size_t size;
  uint tot_fields= 0;
  uint tot_part_fields= 0;
  uint tot_subpart_fields= 0;
  DBUG_ENTER("set_up_charset_field_preps");

  if (!(part_type == HASH_PARTITION &&
        list_of_part_fields) &&
        check_part_func_fields(part_field_array, FALSE))
  {
    ptr= part_field_array;
    /* Set up arrays and buffers for those fields */
    while ((field= *(ptr++)))
    {
      if (field_is_partition_charset(field))
      {
        tot_part_fields++;
        tot_fields++;
      }
    }
    size= tot_part_fields * sizeof(char*);
    if (!(char_ptrs= (uchar**)sql_calloc(size)))
      goto error;
    part_field_buffers= char_ptrs;
    if (!(char_ptrs= (uchar**)sql_calloc(size)))
      goto error;
    restore_part_field_ptrs= char_ptrs;
    size= (tot_part_fields + 1) * sizeof(Field*);
    if (!(char_ptrs= (uchar**)sql_alloc(size)))
      goto error;
    part_charset_field_array= (Field**)char_ptrs;
    ptr= part_field_array;
    i= 0;
    while ((field= *(ptr++)))
    {
      if (field_is_partition_charset(field))
      {
        uchar *field_buf;
        size= field->pack_length();
        if (!(field_buf= (uchar*) sql_calloc(size)))
          goto error;
        part_charset_field_array[i]= field;
        part_field_buffers[i++]= field_buf;
      }
    }
    part_charset_field_array[i]= NULL;
  }
  if (is_sub_partitioned() && !list_of_subpart_fields &&
      check_part_func_fields(subpart_field_array, FALSE))
  {
    /* Set up arrays and buffers for those fields */
    ptr= subpart_field_array;
    while ((field= *(ptr++)))
    {
      if (field_is_partition_charset(field))
      {
        tot_subpart_fields++;
        tot_fields++;
      }
    }
    size= tot_subpart_fields * sizeof(char*);
    if (!(char_ptrs= (uchar**) sql_calloc(size)))
      goto error;
    subpart_field_buffers= char_ptrs;
    if (!(char_ptrs= (uchar**) sql_calloc(size)))
      goto error;
    restore_subpart_field_ptrs= char_ptrs;
    size= (tot_subpart_fields + 1) * sizeof(Field*);
    if (!(char_ptrs= (uchar**) sql_alloc(size)))
      goto error;
    subpart_charset_field_array= (Field**)char_ptrs;
    ptr= subpart_field_array;
    i= 0;
    while ((field= *(ptr++)))
    {
      uchar *field_buf;
      LINT_INIT(field_buf);

      if (!field_is_partition_charset(field))
        continue;
      size= field->pack_length();
      if (!(field_buf= (uchar*) sql_calloc(size)))
        goto error;
      subpart_charset_field_array[i]= field;
      subpart_field_buffers[i++]= field_buf;
    }
    subpart_charset_field_array[i]= NULL;
  }
  DBUG_RETURN(FALSE);
error:
  mem_alloc_error(size);
  DBUG_RETURN(TRUE);
}


/*
  Check if path does not contain mysql data home directory
  for partition elements with data directory and index directory

  SYNOPSIS
    check_partition_dirs()
    part_info               partition_info struct 

  RETURN VALUES
    0	ok
    1	error  
*/

bool check_partition_dirs(partition_info *part_info)
{
  if (!part_info)
    return 0;

  partition_element *part_elem;
  List_iterator<partition_element> part_it(part_info->partitions);
  while ((part_elem= part_it++))
  {
    if (part_elem->subpartitions.elements)
    {
      List_iterator<partition_element> sub_it(part_elem->subpartitions);
      partition_element *subpart_elem;
      while ((subpart_elem= sub_it++))
      {
        if (test_if_data_home_dir(subpart_elem->data_file_name))
          goto dd_err;
        if (test_if_data_home_dir(subpart_elem->index_file_name))
          goto id_err;
      }
    }
    else
    {
      if (test_if_data_home_dir(part_elem->data_file_name))
        goto dd_err;
      if (test_if_data_home_dir(part_elem->index_file_name))
        goto id_err;
    }
  }
  return 0;

dd_err:
  my_error(ER_WRONG_ARGUMENTS,MYF(0),"DATA DIRECTORY");
  return 1;

id_err:
  my_error(ER_WRONG_ARGUMENTS,MYF(0),"INDEX DIRECTORY");
  return 1;
}


/**
  Check what kind of error to report.

  @param use_subpart_expr Use the subpart_expr instead of part_expr
*/

void partition_info::report_part_expr_error(bool use_subpart_expr)
{
  Item *expr= part_expr;
  DBUG_ENTER("partition_info::report_part_expr_error");
  if (use_subpart_expr)
    expr= subpart_expr;

  if (expr->type() == Item::FIELD_ITEM)
  {
    partition_type type= part_type;
    bool list_of_fields= list_of_part_fields;
    Item_field *item_field= (Item_field*) expr;
    /*
      The expression consists of a single field.
      It must be of integer type unless KEY or COLUMNS partitioning.
    */
    if (use_subpart_expr)
    {
      type= subpart_type;
      list_of_fields= list_of_subpart_fields;
    }
    if (!column_list &&
        item_field->field &&
        item_field->field->result_type() != INT_RESULT &&
        !(type == HASH_PARTITION && list_of_fields))
    {
      my_error(ER_FIELD_TYPE_NOT_ALLOWED_AS_PARTITION_FIELD, MYF(0),
               item_field->item_name.ptr());
      DBUG_VOID_RETURN;
    }
  }
  if (use_subpart_expr)
    my_error(ER_PARTITION_FUNC_NOT_ALLOWED_ERROR, MYF(0), "SUBPARTITION");
  else
    my_error(ER_PARTITION_FUNC_NOT_ALLOWED_ERROR, MYF(0), "PARTITION");
  DBUG_VOID_RETURN;
}


/**
  Check if fields are in the partitioning expression.

  @param fields  List of Items (fields)

  @return True if any field in the fields list is used by a partitioning expr.
    @retval true  At least one field in the field list is found.
    @retval false No field is within any partitioning expression.
*/

bool partition_info::is_field_in_part_expr(List<Item> &fields)
{
  List_iterator<Item> it(fields);
  Item *item;
  Item_field *field;
  DBUG_ENTER("is_fields_in_part_expr");
  while ((item= it++))
  {
    field= item->field_for_view_update();
    DBUG_ASSERT(field->field->table == table);
    if (bitmap_is_set(&full_part_field_set, field->field->field_index))
      DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}
 

/**
  Check if all partitioning fields are included.
*/

bool partition_info::is_full_part_expr_in_fields(List<Item> &fields)
{
  Field **part_field= full_part_field_array;
  DBUG_ASSERT(*part_field);
  DBUG_ENTER("is_full_part_expr_in_fields");
  /*
    It is very seldom many fields in full_part_field_array, so it is OK
    to loop over all of them instead of creating a bitmap fields argument
    to compare with.
  */
  do
  {
    List_iterator<Item> it(fields);
    Item *item;
    Item_field *field;
    bool found= false;
  
    while ((item= it++))
    {
      field= item->field_for_view_update();
      DBUG_ASSERT(field->field->table == table);
      if (*part_field == field->field)
      {
        found= true;
        break;
      }
    }
    if (!found)
      DBUG_RETURN(false);
  } while (*(++part_field));
  DBUG_RETURN(true);
}
 

/**
  Create a new column value in current list with maxvalue.

  @return Operation status
    @retval TRUE   Error
    @retval FALSE  Success

  @note Called from parser.
*/

bool partition_info::add_max_value()
{
  DBUG_ENTER("partition_info::add_max_value");

  part_column_list_val *col_val;
  if (!(col_val= add_column_value()))
  {
    DBUG_RETURN(TRUE);
  }
  col_val->max_value= TRUE;
  DBUG_RETURN(FALSE);
}


/**
  Create a new column value in current list.

  @return Pointer to a new part_column_list_val
    @retval  != 0  A part_column_list_val object which have been
                   inserted into its list
    @retval  NULL  Memory allocation failure
    
  @note Called from parser.
*/

part_column_list_val *partition_info::add_column_value()
{
  uint max_val= num_columns ? num_columns : MAX_REF_PARTS;
  DBUG_ENTER("add_column_value");
  DBUG_PRINT("enter", ("num_columns = %u, curr_list_object %u, max_val = %u",
                        num_columns, curr_list_object, max_val));
  if (curr_list_object < max_val)
  {
    curr_list_val->added_items++;
    DBUG_RETURN(&curr_list_val->col_val_array[curr_list_object++]);
  }
  if (!num_columns && part_type == LIST_PARTITION)
  {
    /*
      We're trying to add more than MAX_REF_PARTS, this can happen
      in ALTER TABLE using List partitions where the first partition
      uses VALUES IN (1,2,3...,17) where the number of fields in
      the list is more than MAX_REF_PARTS, in this case we know
      that the number of columns must be 1 and we thus reorganize
      into the structure used for 1 column. After this we call
      ourselves recursively which should always succeed.
    */
    if (!reorganize_into_single_field_col_val() && !init_column_part())
    {
      DBUG_RETURN(add_column_value());
    }
    DBUG_RETURN(NULL);
  }
  if (column_list)
  {
    my_error(ER_PARTITION_COLUMN_LIST_ERROR, MYF(0));
  }
  else
  {
    if (part_type == RANGE_PARTITION)
      my_error(ER_TOO_MANY_VALUES_ERROR, MYF(0), "RANGE");
    else
      my_error(ER_TOO_MANY_VALUES_ERROR, MYF(0), "LIST");
  }
  DBUG_RETURN(NULL);
}


/**
  Initialise part_elem_value object at setting of a new object.

  @param col_val  Column value object to be initialised
  @param item     Item object representing column value

  @return Operation status
    @retval TRUE   Failure
    @retval FALSE  Success

  @note Helper functions to functions called by parser.
*/

void partition_info::init_col_val(part_column_list_val *col_val, Item *item)
{
  DBUG_ENTER("partition_info::init_col_val");

  col_val->item_expression= item;
  col_val->null_value= item->null_value;
  if (item->result_type() == INT_RESULT)
  {
    /*
      This could be both column_list partitioning and function
      partitioning, but it doesn't hurt to set the function
      partitioning flags about unsignedness.
    */
    curr_list_val->value= item->val_int();
    curr_list_val->unsigned_flag= TRUE;
    if (!item->unsigned_flag &&
        curr_list_val->value < 0)
      curr_list_val->unsigned_flag= FALSE;
    if (!curr_list_val->unsigned_flag)
      curr_part_elem->signed_flag= TRUE;
  }
  col_val->part_info= NULL;
  DBUG_VOID_RETURN;
}


/**
  Add a column value in VALUES LESS THAN or VALUES IN.

  @param thd   Thread object
  @param item  Item object representing column value

  @return Operation status
    @retval TRUE   Failure
    @retval FALSE  Success

  @note Called from parser.
*/

bool partition_info::add_column_list_value(THD *thd, Item *item)
{
  part_column_list_val *col_val;
  Name_resolution_context *context= &thd->lex->current_select->context;
  TABLE_LIST *save_list= context->table_list;
  const char *save_where= thd->where;
  DBUG_ENTER("partition_info::add_column_list_value");

  if (part_type == LIST_PARTITION &&
      num_columns == 1U)
  {
    if (init_column_part())
    {
      DBUG_RETURN(TRUE);
    }
  }

  context->table_list= 0;
  if (column_list)
    thd->where= "field list";
  else
    thd->where= "partition function";

  if (item->walk(&Item::check_partition_func_processor, 0,
                 NULL))
  {
    my_error(ER_PARTITION_FUNCTION_IS_NOT_ALLOWED, MYF(0));
    DBUG_RETURN(TRUE);
  }
  if (item->fix_fields(thd, (Item**)0) ||
      ((context->table_list= save_list), FALSE) ||
      (!item->const_item()))
  {
    context->table_list= save_list;
    thd->where= save_where;
    my_error(ER_PARTITION_FUNCTION_IS_NOT_ALLOWED, MYF(0));
    DBUG_RETURN(TRUE);
  }
  thd->where= save_where;

  if (!(col_val= add_column_value()))
  {
    DBUG_RETURN(TRUE);
  }
  init_col_val(col_val, item);
  DBUG_RETURN(FALSE);
}


/**
  Initialize a new column for VALUES {LESS THAN|IN}.

  Initialize part_info object for receiving a set of column values
  for a partition, called when parser reaches VALUES LESS THAN or
  VALUES IN.

  @return Operation status
    @retval TRUE   Failure
    @retval FALSE  Success
*/

bool partition_info::init_column_part()
{
  partition_element *p_elem= curr_part_elem;
  part_column_list_val *col_val_array;
  part_elem_value *list_val;
  uint loc_num_columns;
  DBUG_ENTER("partition_info::init_column_part");

  if (!(list_val=
      (part_elem_value*)sql_calloc(sizeof(part_elem_value))) ||
       p_elem->list_val_list.push_back(list_val))
  {
    mem_alloc_error(sizeof(part_elem_value));
    DBUG_RETURN(TRUE);
  }
  if (num_columns)
    loc_num_columns= num_columns;
  else
    loc_num_columns= MAX_REF_PARTS;
  if (!(col_val_array=
        (part_column_list_val*)sql_calloc(loc_num_columns *
         sizeof(part_column_list_val))))
  {
    mem_alloc_error(loc_num_columns * sizeof(part_elem_value));
    DBUG_RETURN(TRUE);
  }
  list_val->col_val_array= col_val_array;
  list_val->added_items= 0;
  curr_list_val= list_val;
  curr_list_object= 0;
  DBUG_RETURN(FALSE);
}


/**
  Reorganize the preallocated buffer into a single field col list.

  @return Operation status
    @retval  true   Failure
    @retval  false  Success

  @note In the case of ALTER TABLE ADD/REORGANIZE PARTITION for LIST
  partitions we can specify list values as:
  VALUES IN (v1, v2,,,, v17) if we're using the first partitioning
  variant with a function or a column list partitioned table with
  one partition field. In this case the parser knows not the
  number of columns start with and allocates MAX_REF_PARTS in the
  array. If we try to allocate something beyond MAX_REF_PARTS we
  will call this function to reorganize into a structure with
  num_columns = 1. Also when the parser knows that we used LIST
  partitioning and we used a VALUES IN like above where number of
  values was smaller than MAX_REF_PARTS or equal, then we will
  reorganize after discovering this in the parser.
*/

bool partition_info::reorganize_into_single_field_col_val()
{
  part_column_list_val *col_val, *new_col_val;
  part_elem_value *val= curr_list_val;
  uint num_values= num_columns;
  uint i;
  DBUG_ENTER("partition_info::reorganize_into_single_field_col_val");
  DBUG_ASSERT(part_type == LIST_PARTITION);
  DBUG_ASSERT(!num_columns || num_columns == val->added_items);

  if (!num_values)
    num_values= val->added_items;
  num_columns= 1;
  val->added_items= 1U;
  col_val= &val->col_val_array[0];
  init_col_val(col_val, col_val->item_expression);
  for (i= 1; i < num_values; i++)
  {
    col_val= &val->col_val_array[i];
    if (init_column_part())
    {
      DBUG_RETURN(TRUE);
    }
    if (!(new_col_val= add_column_value()))
    {
      DBUG_RETURN(TRUE);
    }
    memcpy(new_col_val, col_val, sizeof(*col_val));
    init_col_val(new_col_val, col_val->item_expression);
  }
  curr_list_val= val;
  DBUG_RETURN(FALSE);
}


/**
  This function handles the case of function-based partitioning.
  
  It fixes some data structures created in the parser and puts
  them in the format required by the rest of the partitioning
  code.

  @param thd        Thread object
  @param col_val    Array of one value
  @param part_elem  The partition instance
  @param part_id    Id of partition instance

  @return Operation status
    @retval TRUE   Failure
    @retval FALSE  Success
*/

bool partition_info::fix_partition_values(THD *thd,
                                          part_elem_value *val,
                                          partition_element *part_elem,
                                          uint part_id)
{
  part_column_list_val *col_val= val->col_val_array;
  DBUG_ENTER("partition_info::fix_partition_values");

  if (col_val->fixed)
  {
    DBUG_RETURN(FALSE);
  }
  if (val->added_items != 1)
  {
    my_error(ER_PARTITION_COLUMN_LIST_ERROR, MYF(0));
    DBUG_RETURN(TRUE);
  }
  if (col_val->max_value)
  {
    /* The parser ensures we're not LIST partitioned here */
    DBUG_ASSERT(part_type == RANGE_PARTITION);
    if (defined_max_value)
    {
      my_error(ER_PARTITION_MAXVALUE_ERROR, MYF(0));
      DBUG_RETURN(TRUE);
    }
    if (part_id == (num_parts - 1))
    {
      defined_max_value= TRUE;
      part_elem->max_value= TRUE;
      part_elem->range_value= LONGLONG_MAX;
    }
    else
    {
      my_error(ER_PARTITION_MAXVALUE_ERROR, MYF(0));
      DBUG_RETURN(TRUE);
    }
  }
  else
  {
    Item *item_expr= col_val->item_expression;
    if ((val->null_value= item_expr->null_value))
    {
      if (part_elem->has_null_value)
      {
         my_error(ER_MULTIPLE_DEF_CONST_IN_LIST_PART_ERROR, MYF(0));
         DBUG_RETURN(TRUE);
      }
      part_elem->has_null_value= TRUE;
    }
    else if (item_expr->result_type() != INT_RESULT)
    {
      my_error(ER_VALUES_IS_NOT_INT_TYPE_ERROR, MYF(0),
               part_elem->partition_name);
      DBUG_RETURN(TRUE);
    }
    if (part_type == RANGE_PARTITION)
    {
      if (part_elem->has_null_value)
      {
        my_error(ER_NULL_IN_VALUES_LESS_THAN, MYF(0));
        DBUG_RETURN(TRUE);
      }
      part_elem->range_value= val->value;
    }
  }
  col_val->fixed= 2;
  DBUG_RETURN(FALSE);
}


/**
  Get column item with a proper character set according to the field.

  @param item   Item object to start with
  @param field  Field for which the item will be compared to

  @return Column item
    @retval NULL  Error
    @retval item  Returned item
*/

Item* partition_info::get_column_item(Item *item, Field *field)
{
  if (field->result_type() == STRING_RESULT &&
      item->collation.collation != field->charset())
  {
    if (!(item= convert_charset_partition_constant(item,
                                                   field->charset())))
    {
      my_error(ER_PARTITION_FUNCTION_IS_NOT_ALLOWED, MYF(0));
      return NULL;
    }
  }
  return item;
}


/**
  Evaluate VALUES functions for column list values.

  @param thd      Thread object
  @param col_val  List of column values
  @param part_id  Partition id we are fixing

  @return Operation status
    @retval TRUE   Error
    @retval FALSE  Success
  
  @note Fix column VALUES and store in memory array adapted to the data type.
*/

bool partition_info::fix_column_value_functions(THD *thd,
                                                part_elem_value *val,
                                                uint part_id)
{
  uint n_columns= part_field_list.elements;
  bool result= FALSE;
  uint i;
  part_column_list_val *col_val= val->col_val_array;
  DBUG_ENTER("partition_info::fix_column_value_functions");

  if (col_val->fixed > 1)
  {
    DBUG_RETURN(FALSE);
  }
  for (i= 0; i < n_columns; col_val++, i++)
  {
    Item *column_item= col_val->item_expression;
    Field *field= part_field_array[i];
    col_val->part_info= this;
    col_val->partition_id= part_id;
    if (col_val->max_value)
      col_val->column_value= NULL;
    else
    {
      col_val->column_value= NULL;
      if (!col_val->null_value)
      {
        uchar *val_ptr;
        uint len= field->pack_length();
        sql_mode_t save_sql_mode;
        bool save_got_warning;

        if (!(column_item= get_column_item(column_item,
                                           field)))
        {
          result= TRUE;
          goto end;
        }
        save_sql_mode= thd->variables.sql_mode;
        thd->variables.sql_mode= 0;
        save_got_warning= thd->got_warning;
        thd->got_warning= 0;
        if (column_item->save_in_field(field, TRUE) ||
            thd->got_warning)
        {
          my_error(ER_WRONG_TYPE_COLUMN_VALUE_ERROR, MYF(0));
          result= TRUE;
          goto end;
        }
        thd->got_warning= save_got_warning;
        thd->variables.sql_mode= save_sql_mode;
        if (!(val_ptr= (uchar*) sql_calloc(len)))
        {
          mem_alloc_error(len);
          result= TRUE;
          goto end;
        }
        col_val->column_value= val_ptr;
        memcpy(val_ptr, field->ptr, len);
      }
    }
    col_val->fixed= 2;
  }
end:
  DBUG_RETURN(result);
}

/**
  Fix partition data from parser.

  @details The parser generates generic data structures, we need to set them
  up as the rest of the code expects to find them. This is in reality part
  of the syntax check of the parser code.

  It is necessary to call this function in the case of a CREATE TABLE
  statement, in this case we do it early in the check_partition_info
  function.

  It is necessary to call this function for ALTER TABLE where we
  assign a completely new partition structure, in this case we do it
  in prep_alter_part_table after discovering that the partition
  structure is entirely redefined.

  It's necessary to call this method also for ALTER TABLE ADD/REORGANIZE
  of partitions, in this we call it in prep_alter_part_table after
  making some initial checks but before going deep to check the partition
  info, we also assign the column_list variable before calling this function
  here.

  Finally we also call it immediately after returning from parsing the
  partitioning text found in the frm file.

  This function mainly fixes the VALUES parts, these are handled differently
  whether or not we use column list partitioning. Since the parser doesn't
  know which we are using we need to set-up the old data structures after
  the parser is complete when we know if what type of partitioning the
  base table is using.

  For column lists we will handle this in the fix_column_value_function.
  For column lists it is sufficient to verify that the number of columns
  and number of elements are in synch with each other. So only partitioning
  using functions need to be set-up to their data structures.

  @param thd  Thread object

  @return Operation status
    @retval TRUE   Failure
    @retval FALSE  Success
*/

bool partition_info::fix_parser_data(THD *thd)
{
  List_iterator<partition_element> it(partitions);
  partition_element *part_elem;
  uint num_elements;
  uint i= 0, j, k;
  DBUG_ENTER("partition_info::fix_parser_data");

  if (!(part_type == RANGE_PARTITION ||
        part_type == LIST_PARTITION))
  {
    /* Nothing to do for HASH/KEY partitioning */
    DBUG_RETURN(FALSE);
  }
  do
  {
    part_elem= it++;
    List_iterator<part_elem_value> list_val_it(part_elem->list_val_list);
    j= 0;
    num_elements= part_elem->list_val_list.elements;
    DBUG_ASSERT(part_type == RANGE_PARTITION ?
                num_elements == 1U : TRUE);
    do
    {
      part_elem_value *val= list_val_it++;
      if (column_list)
      {
        if (val->added_items != num_columns)
        {
          my_error(ER_PARTITION_COLUMN_LIST_ERROR, MYF(0));
          DBUG_RETURN(TRUE);
        }
        for (k= 0; k < num_columns; k++)
        {
          part_column_list_val *col_val= &val->col_val_array[k];
          if (col_val->null_value && part_type == RANGE_PARTITION)
          {
            my_error(ER_NULL_IN_VALUES_LESS_THAN, MYF(0));
            DBUG_RETURN(TRUE);
          }
        }
      }
      else
      {
        if (fix_partition_values(thd, val, part_elem, i))
        {
          DBUG_RETURN(TRUE);
        }
        if (val->null_value)
        {
          /*
            Null values aren't required in the value part, they are kept per
            partition instance, only LIST partitions have NULL values.
          */
          list_val_it.remove();
        }
      }
    } while (++j < num_elements);
  } while (++i < num_parts);
  DBUG_RETURN(FALSE);
}


void partition_info::print_debug(const char *str, uint *value)
{
  DBUG_ENTER("print_debug");
  if (value)
    DBUG_PRINT("info", ("parser: %s, val = %u", str, *value));
  else
    DBUG_PRINT("info", ("parser: %s", str));
  DBUG_VOID_RETURN;
}
#else /* WITH_PARTITION_STORAGE_ENGINE */
 /*
   For builds without partitioning we need to define these functions
   since we they are called from the parser. The parser cannot
   remove code parts using ifdef, but the code parts cannot be called
   so we simply need to add empty functions to make the linker happy.
 */
part_column_list_val *partition_info::add_column_value()
{
  return NULL;
}

bool partition_info::set_part_expr(char *start_token, Item *item_ptr,
                                   char *end_token, bool is_subpart)
{
  (void)start_token;
  (void)item_ptr;
  (void)end_token;
  (void)is_subpart;
  return FALSE;
}

bool partition_info::reorganize_into_single_field_col_val()
{
  return 0;
}

bool partition_info::init_column_part()
{
  return FALSE;
}

bool partition_info::add_column_list_value(THD *thd, Item *item)
{
  return FALSE;
}

bool partition_info::add_max_value()
{
  return false;
}

void partition_info::print_debug(const char *str, uint *value)
{
}

#endif /* WITH_PARTITION_STORAGE_ENGINE */
