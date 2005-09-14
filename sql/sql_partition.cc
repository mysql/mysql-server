/* Copyright (C) 2005 MySQL AB

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

/*
  This file was introduced as a container for general functionality related
  to partitioning introduced in MySQL version 5.1. It contains functionality
  used by all handlers that support partitioning, which in the first version
  is the partitioning handler itself and the NDB handler.

  The first version was written by Mikael Ronstr�m.

  This version supports RANGE partitioning, LIST partitioning, HASH
  partitioning and composite partitioning (hereafter called subpartitioning)
  where each RANGE/LIST partitioning is HASH partitioned. The hash function
  can either be supplied by the user or by only a list of fields (also
  called KEY partitioning, where the MySQL server will use an internal
  hash function.
  There are quite a few defaults that can be used as well.
*/

/* Some general useful functions */

#include "mysql_priv.h"
#include <errno.h>
#include <m_ctype.h>
#include "md5.h"

#ifdef HAVE_PARTITION_DB
/*
  Partition related functions declarations and some static constants;
*/
static char *hash_str= "HASH";
static char *range_str= "RANGE";
static char *list_str= "LIST";
static char *part_str= "PARTITION";
static char *sub_str= "SUB";
static char *by_str= "BY";
static char *key_str= "KEY";
static char *space_str= " ";
static char *equal_str= "=";
static char *end_paren_str= ")";
static char *begin_paren_str= "(";
static char *comma_str= ",";
static char buff[22];

bool get_partition_id_list(partition_info *part_info,
                           uint32 *part_id);
bool get_partition_id_range(partition_info *part_info,
                            uint32 *part_id);
bool get_partition_id_hash_nosub(partition_info *part_info,
                                 uint32 *part_id);
bool get_partition_id_key_nosub(partition_info *part_info,
                                uint32 *part_id);
bool get_partition_id_linear_hash_nosub(partition_info *part_info,
                                        uint32 *part_id);
bool get_partition_id_linear_key_nosub(partition_info *part_info,
                                       uint32 *part_id);
bool get_partition_id_range_sub_hash(partition_info *part_info,
                                     uint32 *part_id);
bool get_partition_id_range_sub_key(partition_info *part_info,
                                    uint32 *part_id);
bool get_partition_id_range_sub_linear_hash(partition_info *part_info,
                                            uint32 *part_id);
bool get_partition_id_range_sub_linear_key(partition_info *part_info,
                                           uint32 *part_id);
bool get_partition_id_list_sub_hash(partition_info *part_info,
                                    uint32 *part_id);
bool get_partition_id_list_sub_key(partition_info *part_info,
                                   uint32 *part_id);
bool get_partition_id_list_sub_linear_hash(partition_info *part_info,
                                           uint32 *part_id);
bool get_partition_id_list_sub_linear_key(partition_info *part_info,
                                          uint32 *part_id);
uint32 get_partition_id_hash_sub(partition_info *part_info); 
uint32 get_partition_id_key_sub(partition_info *part_info); 
uint32 get_partition_id_linear_hash_sub(partition_info *part_info); 
uint32 get_partition_id_linear_key_sub(partition_info *part_info); 
#endif


/*
  A routine used by the parser to decide whether we are specifying a full
  partitioning or if only partitions to add or to split.
  SYNOPSIS
    is_partition_management()
    lex                    Reference to the lex object
  RETURN VALUE
    TRUE                   Yes, it is part of a management partition command
    FALSE                  No, not a management partition command
  DESCRIPTION
    This needs to be outside of HAVE_PARTITION_DB since it is used from the
    sql parser that doesn't have any #ifdef's
*/

my_bool is_partition_management(LEX *lex)
{
  return (lex->sql_command == SQLCOM_ALTER_TABLE &&
          (lex->alter_info.flags == ALTER_ADD_PARTITION ||
           lex->alter_info.flags == ALTER_REORGANISE_PARTITION));
}

#ifdef HAVE_PARTITION_DB
/*
  A support function to check if a partition name is in a list of strings
  SYNOPSIS
    is_partition_in_list()
    part_name          String searched for
    list_part_names    A list of names searched in
  RETURN VALUES
    TRUE               String found
    FALSE              String not found
*/

bool is_partition_in_list(char *part_name,
                          List<char> list_part_names)
{
  List_iterator<char> part_names_it(list_part_names);
  uint no_names= list_part_names.elements;
  uint i= 0;
  do
  {
    char *list_name= part_names_it++;
    if (!(my_strcasecmp(system_charset_info, part_name, list_name)))
      return TRUE;
  } while (++i < no_names);
  return FALSE;
}


/*
  A support function to check partition names for duplication in a
  partitioned table
  SYNOPSIS
    is_partitions_in_table()
    new_part_info      New partition info
    old_part_info      Old partition info
  RETURN VALUES
    TRUE               Duplicate names found
    FALSE              Duplicate names not found
  DESCRIPTION
    Can handle that the new and old parts are the same in which case it
    checks that the list of names in the partitions doesn't contain any
    duplicated names.
*/

bool is_partitions_in_table(partition_info *new_part_info,
                            partition_info *old_part_info)
{
  uint no_new_parts= new_part_info->partitions.elements, new_count;
  uint no_old_parts= old_part_info->partitions.elements, old_count;
  List_iterator<partition_element> new_parts_it(new_part_info->partitions);
  bool same_part_info= (new_part_info == old_part_info);
  DBUG_ENTER("is_partitions_in_table");

  new_count= 0;
  do
  {
    List_iterator<partition_element> old_parts_it(old_part_info->partitions);
    char *new_name= (new_parts_it++)->partition_name;
    new_count++;
    old_count= 0;
    do
    {
      char *old_name= (old_parts_it++)->partition_name;
      old_count++;
      if (same_part_info && old_count == new_count)
        break;
      if (!(my_strcasecmp(system_charset_info, old_name, new_name)))
      {
        DBUG_RETURN(TRUE);
      }
    } while (old_count < no_old_parts);
  } while (new_count < no_new_parts);
  DBUG_RETURN(FALSE);
}


/*
  A useful routine used by update_row for partition handlers to calculate
  the partition ids of the old and the new record.
  SYNOPSIS
    get_part_for_update()
    old_data                Buffer of old record
    new_data                Buffer of new record
    rec0                    Reference to table->record[0]
    part_info               Reference to partition information
    part_field_array        A NULL-terminated array of fields for partition
                            function
    old_part_id             The returned partition id of old record 
    new_part_id             The returned partition id of new record 
  RETURN VALUE
    0                       Success
    > 0                     Error code
  DESCRIPTION
    Dependent on whether buf is not record[0] we need to prepare the
    fields. Then we call the function pointer get_partition_id to
    calculate the partition ids.
*/

int get_parts_for_update(const byte *old_data, byte *new_data,
                         const byte *rec0, partition_info *part_info,
                         uint32 *old_part_id, uint32 *new_part_id)
{
  Field **part_field_array= part_info->full_part_field_array;
  int error;
  DBUG_ENTER("get_parts_for_update");
  DBUG_ASSERT(new_data == rec0);

  set_field_ptr(part_field_array, old_data, rec0);
  error= part_info->get_partition_id(part_info, old_part_id);
  set_field_ptr(part_field_array, rec0, old_data);
  if (unlikely(error))                             // Should never happen
  {
    DBUG_ASSERT(0);
    DBUG_RETURN(error);
  }
#ifdef NOT_NEEDED
  if (new_data == rec0)
#endif
  {
    if (unlikely(error= part_info->get_partition_id(part_info,new_part_id)))
    {
      DBUG_RETURN(error);
    }
  }
#ifdef NOT_NEEDED
  else
  {
    /*
      This branch should never execute but it is written anyways for
      future use. It will be tested by ensuring that the above
      condition is false in one test situation before pushing the code.
    */
    set_field_ptr(part_field_array, new_data, rec0);
    error= part_info->get_partition_id(part_info, new_part_id);
    set_field_ptr(part_field_array, rec0, new_data);
    if (unlikely(error))
    {
      DBUG_RETURN(error);
    }
  }
#endif
  DBUG_RETURN(0);
}


/*
  A useful routine used by delete_row for partition handlers to calculate
  the partition id.
  SYNOPSIS
    get_part_for_delete()
    buf                     Buffer of old record
    rec0                    Reference to table->record[0]
    part_info               Reference to partition information
    part_field_array        A NULL-terminated array of fields for partition
                            function
    part_id                 The returned partition id to delete from
  RETURN VALUE
    0                       Success
    > 0                     Error code
  DESCRIPTION
    Dependent on whether buf is not record[0] we need to prepare the
    fields. Then we call the function pointer get_partition_id to
    calculate the partition id.
*/

int get_part_for_delete(const byte *buf, const byte *rec0,
                        partition_info *part_info, uint32 *part_id)
{
  int error;
  DBUG_ENTER("get_part_for_delete");

  if (likely(buf == rec0))
  {
    if (unlikely((error= part_info->get_partition_id(part_info, part_id))))
    {
      DBUG_RETURN(error);
    }
    DBUG_PRINT("info", ("Delete from partition %d", *part_id));
  }
  else
  {
    Field **part_field_array= part_info->full_part_field_array;
    set_field_ptr(part_field_array, buf, rec0);
    error= part_info->get_partition_id(part_info, part_id);
    set_field_ptr(part_field_array, rec0, buf);
    if (unlikely(error))
    {
      DBUG_RETURN(error);
    }
    DBUG_PRINT("info", ("Delete from partition %d (path2)", *part_id));
  }
  DBUG_RETURN(0);
}


/*
  This routine allocates an array for all range constants to achieve a fast
  check what partition a certain value belongs to. At the same time it does
  also check that the range constants are defined in increasing order and
  that the expressions are constant integer expressions.
  SYNOPSIS
    check_range_constants()
      part_info
  RETURN VALUE
    TRUE                An error occurred during creation of range constants
    FALSE               Successful creation of range constant mapping
  DESCRIPTION
    This routine is called from check_partition_info to get a quick error
    before we came too far into the CREATE TABLE process. It is also called
    from fix_partition_func every time we open the .frm file. It is only
    called for RANGE PARTITIONed tables.
*/

static bool check_range_constants(partition_info *part_info)
{
  partition_element* part_def;
  longlong current_largest_int= LONGLONG_MIN, part_range_value_int;
  uint no_parts= part_info->no_parts, i;
  List_iterator<partition_element> it(part_info->partitions);
  bool result= TRUE;
  DBUG_ENTER("check_range_constants");
  DBUG_PRINT("enter", ("INT_RESULT with %d parts", no_parts));

  part_info->part_result_type= INT_RESULT;
  part_info->range_int_array= 
                      (longlong*)sql_alloc(no_parts * sizeof(longlong));
  if (unlikely(part_info->range_int_array == NULL))
  {
    my_error(ER_OUTOFMEMORY, MYF(0), no_parts*sizeof(longlong));
    goto end;
  }
  i= 0;
  do
  {
    part_def= it++;
    if ((i != (no_parts - 1)) || !part_info->defined_max_value)
      part_range_value_int= part_def->range_value; 
    else
      part_range_value_int= LONGLONG_MAX;
    if (likely(current_largest_int < part_range_value_int))
    {
      current_largest_int= part_range_value_int;
      part_info->range_int_array[i]= part_range_value_int;
    }
    else
    {
      my_error(ER_RANGE_NOT_INCREASING_ERROR, MYF(0));
      goto end;
    }
  } while (++i < no_parts);
  result= FALSE;
end:
  DBUG_RETURN(result);
}


/*
  A support routine for check_list_constants used by qsort to sort the
  constant list expressions.
  SYNOPSIS
    list_part_cmp()
      a                First list constant to compare with
      b                Second list constant to compare with
  RETURN VALUE
    +1                 a > b
    0                  a  == b
    -1                 a < b
*/

static int list_part_cmp(const void* a, const void* b)
{
  longlong a1, b1;
  a1= ((LIST_PART_ENTRY*)a)->list_value;
  b1= ((LIST_PART_ENTRY*)b)->list_value;
  if (a1 < b1)
    return -1;
  else if (a1 > b1)
    return +1;
  else
    return 0;
}


/*
  This routine allocates an array for all list constants to achieve a fast
  check what partition a certain value belongs to. At the same time it does
  also check that there are no duplicates among the list constants and that
  that the list expressions are constant integer expressions.
  SYNOPSIS
    check_list_constants()
      part_info
  RETURN VALUE
    TRUE                  An error occurred during creation of list constants
    FALSE                 Successful creation of list constant mapping
  DESCRIPTION
    This routine is called from check_partition_info to get a quick error
    before we came too far into the CREATE TABLE process. It is also called
    from fix_partition_func every time we open the .frm file. It is only
    called for LIST PARTITIONed tables.
*/

static bool check_list_constants(partition_info *part_info)
{
  uint i, no_list_values= 0, no_parts, list_index= 0;
  longlong *list_value;
  bool not_first, result= TRUE;
  longlong curr_value, prev_value;
  partition_element* part_def;
  List_iterator<partition_element> list_func_it(part_info->partitions);
  DBUG_ENTER("check_list_constants");

  part_info->part_result_type= INT_RESULT;

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

  no_parts= part_info->no_parts;
  i= 0;
  do
  {
    part_def= list_func_it++;
    List_iterator<longlong> list_val_it1(part_def->list_val_list);
    while (list_val_it1++)
      no_list_values++;
  } while (++i < no_parts);
  list_func_it.rewind();
  part_info->no_list_values= no_list_values;
  part_info->list_array=
      (LIST_PART_ENTRY*)sql_alloc(no_list_values*sizeof(LIST_PART_ENTRY));
  if (unlikely(part_info->list_array == NULL))
  {
    my_error(ER_OUTOFMEMORY, MYF(0), no_list_values*sizeof(LIST_PART_ENTRY));
    goto end;
  }

  i= 0;
  do
  {
    part_def= list_func_it++;
    List_iterator<longlong> list_val_it2(part_def->list_val_list);
    while ((list_value= list_val_it2++))
    {
      part_info->list_array[list_index].list_value= *list_value;
      part_info->list_array[list_index++].partition_id= i;
    }
  } while (++i < no_parts);

  qsort((void*)part_info->list_array, no_list_values,
        sizeof(LIST_PART_ENTRY), &list_part_cmp);

  not_first= FALSE;
  i= prev_value= 0; //prev_value initialised to quiet compiler
  do
  {
    curr_value= part_info->list_array[i].list_value;
    if (likely(!not_first || prev_value != curr_value))
    {
      prev_value= curr_value;
      not_first= TRUE;
    }
    else
    {
      my_error(ER_MULTIPLE_DEF_CONST_IN_LIST_PART_ERROR, MYF(0));
      goto end;
    }
  } while (++i < no_list_values);
  result= FALSE;
end:
  DBUG_RETURN(result);
}


/*
  Create a memory area where default partition names are stored and fill it
  up with the names.
  SYNOPSIS
    create_default_partition_names()
    no_parts                        Number of partitions
    subpart                         Is it subpartitions
  RETURN VALUE
    A pointer to the memory area of the default partition names
  DESCRIPTION
    A support routine for the partition code where default values are
    generated.
    The external routine needing this code is check_partition_info
*/

#define MAX_PART_NAME_SIZE 8

static char *create_default_partition_names(uint no_parts, uint start_no,
                                            bool subpart)
{
  char *ptr= sql_calloc(no_parts*MAX_PART_NAME_SIZE);
  char *move_ptr= ptr;
  uint i= 0;
  DBUG_ENTER("create_default_partition_names");
  if (likely(ptr != 0))
  {
    do
    {
      if (subpart)
        my_sprintf(move_ptr, (move_ptr,"sp%u", (start_no + i)));
      else
        my_sprintf(move_ptr, (move_ptr,"p%u", (start_no + i)));
      move_ptr+=MAX_PART_NAME_SIZE;
    } while (++i < no_parts);
  }
  else
  {
    my_error(ER_OUTOFMEMORY, MYF(0), no_parts*MAX_PART_NAME_SIZE);
  }
  DBUG_RETURN(ptr);
}


/*
  Set up all the default partitions not set-up by the user in the SQL
  statement. Also perform a number of checks that the user hasn't tried
  to use default values where no defaults exists.
  SYNOPSIS
    set_up_default_partitions()
    part_info           The reference to all partition information
    file                A reference to a handler of the table
    max_rows            Maximum number of rows stored in the table
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

static bool set_up_default_partitions(partition_info *part_info,
                                      handler *file, ulonglong max_rows,
                                      uint start_no)
{
  uint no_parts, i;
  char *default_name;
  bool result= TRUE;
  DBUG_ENTER("set_up_default_partitions");

  if (part_info->part_type != HASH_PARTITION)
  {
    char *error_string;
    if (part_info->part_type == RANGE_PARTITION)
      error_string= range_str;
    else
      error_string= list_str;
    my_error(ER_PARTITIONS_MUST_BE_DEFINED_ERROR, MYF(0), error_string);
    goto end;
  }
  if (part_info->no_parts == 0)
    part_info->no_parts= file->get_default_no_partitions(max_rows);
  no_parts= part_info->no_parts;
  part_info->use_default_partitions= FALSE;
  if (unlikely(no_parts > MAX_PARTITIONS))
  {
    my_error(ER_TOO_MANY_PARTITIONS_ERROR, MYF(0));
    goto end;
  }
  if (unlikely((!(default_name= create_default_partition_names(no_parts,
                                                               start_no,
                                                               FALSE)))))
    goto end;
  i= 0;
  do
  {
    partition_element *part_elem= new partition_element();
    if (likely(part_elem != 0))
    {
      part_elem->engine_type= DB_TYPE_UNKNOWN;
      part_elem->partition_name= default_name;
      default_name+=MAX_PART_NAME_SIZE;
      part_info->partitions.push_back(part_elem);
    }
    else
    {
      my_error(ER_OUTOFMEMORY, MYF(0), sizeof(partition_element));
      goto end;
    }
  } while (++i < no_parts);
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
    part_info           The reference to all partition information
    file                A reference to a handler of the table
    max_rows            Maximum number of rows stored in the table
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

static bool set_up_default_subpartitions(partition_info *part_info,
                                         handler *file, ulonglong max_rows)
{
  uint i, j, no_parts, no_subparts;
  char *default_name, *name_ptr;
  bool result= TRUE;
  partition_element *part_elem;
  List_iterator<partition_element> part_it(part_info->partitions);
  DBUG_ENTER("set_up_default_subpartitions");

  if (part_info->no_subparts == 0)
    part_info->no_subparts= file->get_default_no_partitions(max_rows);
  no_parts= part_info->no_parts;
  no_subparts= part_info->no_subparts;
  part_info->use_default_subpartitions= FALSE;
  if (unlikely((no_parts * no_subparts) > MAX_PARTITIONS))
  {
    my_error(ER_TOO_MANY_PARTITIONS_ERROR, MYF(0));
    goto end;
  }
  if (unlikely((!(default_name=
             create_default_partition_names(no_subparts, (uint)0, TRUE)))))
    goto end;
  i= 0;
  do
  {
    part_elem= part_it++;
    j= 0;
    name_ptr= default_name;
    do
    {
      partition_element *subpart_elem= new partition_element();
      if (likely(subpart_elem != 0))
      {
        subpart_elem->engine_type= DB_TYPE_UNKNOWN;
        subpart_elem->partition_name= name_ptr;
        name_ptr+= MAX_PART_NAME_SIZE;
        part_elem->subpartitions.push_back(subpart_elem);
      }
      else
      {
        my_error(ER_OUTOFMEMORY, MYF(0), sizeof(partition_element));
        goto end;
      }
    } while (++j < no_subparts);
  } while (++i < no_parts);
  result= FALSE;
end:
  DBUG_RETURN(result);
}


/*
  Set up defaults for partition or subpartition (cannot set-up for both,
  this will return an error.
  SYNOPSIS
    set_up_defaults_for_partitioning()
    part_info           The reference to all partition information
    file                A reference to a handler of the table
    max_rows            Maximum number of rows stored in the table
  RETURN VALUE
    TRUE                Error, attempted default values not possible
    FALSE               Ok, default partitions set-up
  DESCRIPTION
    Support routine for check_partition_info
*/

bool set_up_defaults_for_partitioning(partition_info *part_info,
                                      handler *file,
                                      ulonglong max_rows, uint start_no)
{
  DBUG_ENTER("set_up_defaults_for_partitioning");

  if (part_info->use_default_partitions)
    DBUG_RETURN(set_up_default_partitions(part_info, file, max_rows,
                                          start_no));
  if (is_sub_partitioned(part_info) && part_info->use_default_subpartitions)
    DBUG_RETURN(set_up_default_subpartitions(part_info, file, max_rows));
  DBUG_RETURN(FALSE);
}


/*
  Check that all partitions use the same storage engine.
  This is currently a limitation in this version.
  SYNOPSIS
    check_engine_mix()
    engine_array           An array of engine identifiers
    no_parts               Total number of partitions
  RETURN VALUE
    TRUE                   Error, mixed engines
    FALSE                  Ok, no mixed engines
*/

static bool check_engine_mix(u_char *engine_array, uint no_parts)
{
  /*
    Current check verifies only that all handlers are the same.
    Later this check will be more sophisticated.
  */
  uint i= 0;
  bool result= FALSE;
  DBUG_ENTER("check_engine_mix");

  do
  {
    if (engine_array[i] != engine_array[0])
    {
      result= TRUE;
      break;
    }
  } while (++i < no_parts);
  DBUG_RETURN(result);
}


/*
  We will check that the partition info requested is possible to set-up in
  this version. This routine is an extension of the parser one could say.
  If defaults were used we will generate default data structures for all
  partitions.
  SYNOPSIS
    check_partition_info()
    part_info           The reference to all partition information
    db_type             Default storage engine if no engine specified per
                        partition.
    file                A reference to a handler of the table
    max_rows            Maximum number of rows stored in the table
  RETURN VALUE
    TRUE                 Error, something went wrong
    FALSE                Ok, full partition data structures are now generated
  DESCRIPTION
    This code is used early in the CREATE TABLE and ALTER TABLE process.
*/

bool check_partition_info(partition_info *part_info,enum db_type eng_type,
                          handler *file, ulonglong max_rows)
{
  u_char *engine_array= NULL;
  uint part_count= 0, i, no_parts, tot_partitions;
  bool result= TRUE;
  List_iterator<partition_element> part_it(part_info->partitions);
  DBUG_ENTER("check_partition_info");

  if (unlikely(is_sub_partitioned(part_info) &&
              (!(part_info->part_type == RANGE_PARTITION ||
                 part_info->part_type == LIST_PARTITION))))
  {
    /* Only RANGE and LIST partitioning can be subpartitioned */
    my_error(ER_SUBPARTITION_ERROR, MYF(0));
    goto end;
  }
  if (unlikely(set_up_defaults_for_partitioning(part_info, file,
                                                max_rows, (uint)0)))
    goto end;
  tot_partitions= get_tot_partitions(part_info);
  if (unlikely(tot_partitions > MAX_PARTITIONS))
  {
    my_error(ER_TOO_MANY_PARTITIONS_ERROR, MYF(0));
    goto end;
  }
  if (unlikely(is_partitions_in_table(part_info, part_info)))
  {
    my_error(ER_SAME_NAME_PARTITION, MYF(0));
    goto end;
  }
  engine_array= (u_char*)my_malloc(tot_partitions, MYF(MY_WME));
  if (unlikely(!engine_array))
    goto end;
  i= 0;
  no_parts= part_info->no_parts;
  do
  {
    partition_element *part_elem= part_it++;
    if (!is_sub_partitioned(part_info))
    {
      if (part_elem->engine_type == DB_TYPE_UNKNOWN)
        part_elem->engine_type= eng_type;
      DBUG_PRINT("info", ("engine = %u",(uint)part_elem->engine_type));
      engine_array[part_count++]= (u_char)part_elem->engine_type;
    }
    else
    {
      uint j= 0, no_subparts= part_info->no_subparts;;
      List_iterator<partition_element> sub_it(part_elem->subpartitions);
      do
      {
        part_elem= sub_it++;
        if (part_elem->engine_type == DB_TYPE_UNKNOWN)
          part_elem->engine_type= eng_type;
        DBUG_PRINT("info", ("engine = %u",(uint)part_elem->engine_type));
        engine_array[part_count++]= (u_char)part_elem->engine_type;
      } while (++j < no_subparts);
    }
  } while (++i < part_info->no_parts);
  if (unlikely(check_engine_mix(engine_array, part_count)))
  {
    my_error(ER_MIX_HANDLER_ERROR, MYF(0));
    goto end;
  }

  /*
    We need to check all constant expressions that they are of the correct
    type and that they are increasing for ranges and not overlapping for
    list constants.
  */

  if (unlikely((part_info->part_type == RANGE_PARTITION &&
                check_range_constants(part_info)) ||
               (part_info->part_type == LIST_PARTITION &&
                check_list_constants(part_info))))
    goto end;
  result= FALSE;
end:
  my_free((char*)engine_array,MYF(MY_ALLOW_ZERO_PTR));
  DBUG_RETURN(result);
}


/*
  A great number of functions below here is part of the fix_partition_func
  method. It is used to set up the partition structures for execution from
  openfrm. It is called at the end of the openfrm when the table struct has
  been set-up apart from the partition information.
  It involves:
  1) Setting arrays of fields for the partition functions.
  2) Setting up binary search array for LIST partitioning
  3) Setting up array for binary search for RANGE partitioning
  4) Setting up key_map's to assist in quick evaluation whether one
     can deduce anything from a given index of what partition to use
  5) Checking whether a set of partitions can be derived from a range on
     a field in the partition function.
  As part of doing this there is also a great number of error controls.
  This is actually the place where most of the things are checked for
  partition information when creating a table.
  Things that are checked includes
  1) No NULLable fields in partition function
  2) All fields of partition function in Primary keys and unique indexes
     (if not supported)
  3) No fields in partition function that are BLOB's or VARCHAR with a
     collation other than the binary collation.



  Create an array of partition fields (NULL terminated). Before this method
  is called fix_fields or find_table_in_sef has been called to set
  GET_FIXED_FIELDS_FLAG on all fields that are part of the partition
  function.
  SYNOPSIS
    set_up_field_array()
    table                TABLE object for which partition fields are set-up
    sub_part             Is the table subpartitioned as well
  RETURN VALUE
    TRUE                 Error, some field didn't meet requirements
    FALSE                Ok, partition field array set-up
  DESCRIPTION
    This method is used to set-up both partition and subpartitioning
    field array and used for all types of partitioning.
    It is part of the logic around fix_partition_func.
*/
static bool set_up_field_array(TABLE *table,
                              bool sub_part)
{
  Field **ptr, *field, **field_array;
  uint no_fields= 0, size_field_array, i= 0;
  partition_info *part_info= table->s->part_info;
  int result= FALSE;
  DBUG_ENTER("set_up_field_array");

  ptr= table->field;
  while ((field= *(ptr++))) 
  {
    if (field->flags & GET_FIXED_FIELDS_FLAG)
      no_fields++;
  }
  size_field_array= (no_fields+1)*sizeof(Field*);
  field_array= (Field**)sql_alloc(size_field_array);
  if (unlikely(!field_array))
  {
    my_error(ER_OUTOFMEMORY, MYF(0), size_field_array);
    result= TRUE;
  }
  ptr= table->field;
  while ((field= *(ptr++))) 
  {
    if (field->flags & GET_FIXED_FIELDS_FLAG)
    {
      field->flags&= ~GET_FIXED_FIELDS_FLAG;
      field->flags|= FIELD_IN_PART_FUNC_FLAG;
      if (likely(!result))
      {
        field_array[i++]= field;

        /*
          We check that the fields are proper. It is required for each
          field in a partition function to:
          1) Not be a BLOB of any type
            A BLOB takes too long time to evaluate so we don't want it for
            performance reasons.
          2) Not be a VARCHAR other than VARCHAR with a binary collation
            A VARCHAR with character sets can have several values being
            equal with different number of spaces or NULL's. This is not a
            good ground for a safe and exact partition function. Thus it is
            not allowed in partition functions.
        */

        if (unlikely(field->flags & BLOB_FLAG))
        {
          my_error(ER_BLOB_FIELD_IN_PART_FUNC_ERROR, MYF(0));
          result= TRUE;
        }
        else if (unlikely((!field->flags & BINARY_FLAG) &&
                          field->real_type() == MYSQL_TYPE_VARCHAR))
        {
          my_error(ER_CHAR_SET_IN_PART_FIELD_ERROR, MYF(0));
          result= TRUE;
        }
      }
    }
  }
  field_array[no_fields]= 0;
  if (!sub_part)
  {
    part_info->part_field_array= field_array;
    part_info->no_part_fields= no_fields;
  }
  else
  {
    part_info->subpart_field_array= field_array;
    part_info->no_subpart_fields= no_fields;
  }
  DBUG_RETURN(result);
}


/*
  Create a field array including all fields of both the partitioning and the
  subpartitioning functions.
  SYNOPSIS
    create_full_part_field_array()
    table                TABLE object for which partition fields are set-up
    part_info            Reference to partitioning data structure
  RETURN VALUE
    TRUE                 Memory allocation of field array failed
    FALSE                Ok
  DESCRIPTION
    If there is no subpartitioning then the same array is used as for the
    partitioning. Otherwise a new array is built up using the flag
    FIELD_IN_PART_FUNC in the field object.
    This function is called from fix_partition_func
*/

static bool create_full_part_field_array(TABLE *table,
                                         partition_info *part_info)
{
  bool result= FALSE;
  DBUG_ENTER("create_full_part_field_array");

  if (!is_sub_partitioned(part_info))
  {
    part_info->full_part_field_array= part_info->part_field_array;
    part_info->no_full_part_fields= part_info->no_part_fields;
  }
  else
  {
    Field **ptr, *field, **field_array;
    uint no_part_fields=0, size_field_array;
    ptr= table->field;
    while ((field= *(ptr++)))
    {
      if (field->flags & FIELD_IN_PART_FUNC_FLAG)
        no_part_fields++;
    }
    size_field_array= (no_part_fields+1)*sizeof(Field*);
    field_array= (Field**)sql_alloc(size_field_array);
    if (unlikely(!field_array))
    {
      my_error(ER_OUTOFMEMORY, MYF(0), size_field_array);
      result= TRUE;
      goto end;
    }
    no_part_fields= 0;
    ptr= table->field;
    while ((field= *(ptr++)))
    {
      if (field->flags & FIELD_IN_PART_FUNC_FLAG)
        field_array[no_part_fields++]= field;
    }
    field_array[no_part_fields]=0;
    part_info->full_part_field_array= field_array;
    part_info->no_full_part_fields= no_part_fields;
  }
end:
  DBUG_RETURN(result);
}


/*
  These support routines is used to set/reset an indicator of all fields
  in a certain key. It is used in conjunction with another support routine
  that traverse all fields in the PF to find if all or some fields in the
  PF is part of the key. This is used to check primary keys and unique
  keys involve all fields in PF (unless supported) and to derive the
  key_map's used to quickly decide whether the index can be used to
  derive which partitions are needed to scan.



  Clear flag GET_FIXED_FIELDS_FLAG in all fields of a key previously set by
  set_indicator_in_key_fields (always used in pairs).
  SYNOPSIS
    clear_indicator_in_key_fields()
    key_info                  Reference to find the key fields
*/

static void clear_indicator_in_key_fields(KEY *key_info)
{
  KEY_PART_INFO *key_part;
  uint key_parts= key_info->key_parts, i;
  for (i= 0, key_part=key_info->key_part; i < key_parts; i++, key_part++)
    key_part->field->flags&= (~GET_FIXED_FIELDS_FLAG);
}


/*
  Set flag GET_FIXED_FIELDS_FLAG in all fields of a key.
  SYNOPSIS
    set_indicator_in_key_fields
    key_info                  Reference to find the key fields
*/

static void set_indicator_in_key_fields(KEY *key_info)
{
  KEY_PART_INFO *key_part;
  uint key_parts= key_info->key_parts, i;
  for (i= 0, key_part=key_info->key_part; i < key_parts; i++, key_part++)
    key_part->field->flags|= GET_FIXED_FIELDS_FLAG;
}


/*
  Check if all or some fields in partition field array is part of a key
  previously used to tag key fields.
  SYNOPSIS
    check_fields_in_PF()
    ptr                  Partition field array
    all_fields           Is all fields of partition field array used in key
    some_fields          Is some fields of partition field array used in key
  RETURN VALUE
    all_fields, some_fields
*/

static void check_fields_in_PF(Field **ptr, bool *all_fields,
                               bool *some_fields)
{
  DBUG_ENTER("check_fields_in_PF");
  *all_fields= TRUE;
  *some_fields= FALSE;
  do
  {
  /* Check if the field of the PF is part of the current key investigated */
    if ((*ptr)->flags & GET_FIXED_FIELDS_FLAG)
      *some_fields= TRUE; 
    else
      *all_fields= FALSE;
  } while (*(++ptr));
  DBUG_VOID_RETURN;
}


/*
  Clear flag GET_FIXED_FIELDS_FLAG in all fields of the table.
  This routine is used for error handling purposes.
  SYNOPSIS
    clear_field_flag()
    table                TABLE object for which partition fields are set-up
*/

static void clear_field_flag(TABLE *table)
{
  Field **ptr;
  DBUG_ENTER("clear_field_flag");

  for (ptr= table->field; *ptr; ptr++)
    (*ptr)->flags&= (~GET_FIXED_FIELDS_FLAG);
  DBUG_VOID_RETURN;
}


/*
  This routine sets-up the partition field array for KEY partitioning, it
  also verifies that all fields in the list of fields is actually a part of
  the table.
  SYNOPSIS
    handle_list_of_fields()
    it                   A list of field names for the partition function
    table                TABLE object for which partition fields are set-up
    part_info            Reference to partitioning data structure
    sub_part             Is the table subpartitioned as well
  RETURN VALUE
    TRUE                 Fields in list of fields not part of table
    FALSE                All fields ok and array created
  DESCRIPTION
    find_field_in_table_sef finds the field given its name. All fields get
    GET_FIXED_FIELDS_FLAG set.
*/

static bool handle_list_of_fields(List_iterator<char> it,
                                  TABLE *table,
                                  partition_info *part_info,
                                  bool sub_part)
{
  Field *field;
  bool result;
  char *field_name;
  DBUG_ENTER("handle_list_of_fields");

  while ((field_name= it++))
  {
    field= find_field_in_table_sef(table, field_name);
    if (likely(field != 0))
      field->flags|= GET_FIXED_FIELDS_FLAG;
    else
    {
      my_error(ER_FIELD_NOT_FOUND_PART_ERROR, MYF(0));
      clear_field_flag(table);
      result= TRUE;
      goto end;
    }
  }
  result= set_up_field_array(table, sub_part);
end:
  DBUG_RETURN(result);
}


/*
  This function is used to build an array of partition fields for the
  partitioning function and subpartitioning function. The partitioning
  function is an item tree that must reference at least one field in the
  table. This is checked first in the parser that the function doesn't
  contain non-cacheable parts (like a random function) and by checking
  here that the function isn't a constant function.
  SYNOPSIS
    fix_fields_part_func()
    thd                  The thread object
    tables               A list of one table, the partitioned table
    func_expr            The item tree reference of the partition function
    part_info            Reference to partitioning data structure
    sub_part             Is the table subpartitioned as well
  RETURN VALUE
    TRUE                 An error occurred, something was wrong with the
                         partition function.
    FALSE                Ok, a partition field array was created
  DESCRIPTION
    The function uses a new feature in fix_fields where the flag 
    GET_FIXED_FIELDS_FLAG is set for all fields in the item tree.
    This field must always be reset before returning from the function
    since it is used for other purposes as well.
*/

static bool fix_fields_part_func(THD *thd, TABLE_LIST *tables,
                                 Item* func_expr, partition_info *part_info,
                                 bool sub_part)
{
  /*
    Calculate the number of fields in the partition function.
    Use it allocate memory for array of Field pointers.
    Initialise array of field pointers. Use information set when
    calling fix_fields and reset it immediately after.
    The get_fields_in_item_tree activates setting of bit in flags
    on the field object.
  */

  bool result= TRUE;
  TABLE *table= tables->table;
  TABLE_LIST *save_table_list, *save_first_table, *save_last_table;
  int error;
  Name_resolution_context *context;
  DBUG_ENTER("fix_fields_part_func");

  context= thd->lex->current_context();
  table->map= 1; //To ensure correct calculation of const item
  table->get_fields_in_item_tree= TRUE;
  save_table_list= context->table_list;
  save_first_table= context->first_name_resolution_table;
  save_last_table= context->last_name_resolution_table;
  context->table_list= tables;
  context->first_name_resolution_table= tables;
  context->last_name_resolution_table= NULL;
  func_expr->walk(&Item::change_context_processor, (byte*) context);
  thd->where= "partition function";
  error= func_expr->fix_fields(thd, (Item**)0);
  context->table_list= save_table_list;
  context->first_name_resolution_table= save_first_table;
  context->last_name_resolution_table= save_last_table;
  if (unlikely(error))
  {
    DBUG_PRINT("info", ("Field in partition function not part of table"));
    clear_field_flag(table);
    goto end;
  }
  if (unlikely(func_expr->const_item()))
  {
    my_error(ER_CONST_EXPR_IN_PARTITION_FUNC_ERROR, MYF(0));
    clear_field_flag(table);
    goto end;
  }
  result= set_up_field_array(table, sub_part);
end:
  table->get_fields_in_item_tree= FALSE;
  table->map= 0; //Restore old value
  DBUG_RETURN(result);
}


/*
  This function verifies that if there is a primary key that it contains
  all the fields of the partition function.
  This is a temporary limitation that will hopefully be removed after a
  while.
  SYNOPSIS
    check_primary_key()
    table                TABLE object for which partition fields are set-up
  RETURN VALUES
    TRUE                 Not all fields in partitioning function was part
                         of primary key
    FALSE                Ok, all fields of partitioning function were part
                         of primary key
*/

static bool check_primary_key(TABLE *table)
{
  uint primary_key= table->s->primary_key;
  bool all_fields, some_fields, result= FALSE;
  DBUG_ENTER("check_primary_key");

  if (primary_key < MAX_KEY)
  {
    set_indicator_in_key_fields(table->key_info+primary_key);
    check_fields_in_PF(table->s->part_info->full_part_field_array,
                        &all_fields, &some_fields);
    clear_indicator_in_key_fields(table->key_info+primary_key);
    if (unlikely(!all_fields))
    {
      my_error(ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF,MYF(0),"PRIMARY KEY");
      result= TRUE;
    }
  }
  DBUG_RETURN(result);
}


/*
  This function verifies that if there is a unique index that it contains
  all the fields of the partition function.
  This is a temporary limitation that will hopefully be removed after a
  while.
  SYNOPSIS
    check_unique_keys()
    table                TABLE object for which partition fields are set-up
  RETURN VALUES
    TRUE                 Not all fields in partitioning function was part
                         of all unique keys
    FALSE                Ok, all fields of partitioning function were part
                         of unique keys
*/

static bool check_unique_keys(TABLE *table)
{
  bool all_fields, some_fields, result= FALSE;
  uint keys= table->s->keys, i;
  DBUG_ENTER("check_unique_keys");
  for (i= 0; i < keys; i++)
  {
    if (table->key_info[i].flags & HA_NOSAME) //Unique index
    {
      set_indicator_in_key_fields(table->key_info+i);
      check_fields_in_PF(table->s->part_info->full_part_field_array,
                         &all_fields, &some_fields);
      clear_indicator_in_key_fields(table->key_info+i);
      if (unlikely(!all_fields))
      {
        my_error(ER_UNIQUE_KEY_NEED_ALL_FIELDS_IN_PF,MYF(0),"UNIQUE INDEX");
        result= TRUE;
        break;
      }
    }
  }
  DBUG_RETURN(result);
}


/*
  An important optimisation is whether a range on a field can select a subset
  of the partitions.
  A prerequisite for this to happen is that the PF is a growing function OR
  a shrinking function.
  This can never happen for a multi-dimensional PF. Thus this can only happen
  with PF with at most one field involved in the PF.
  The idea is that if the function is a growing function and you know that
  the field of the PF is 4 <= A <= 6 then we can convert this to a range
  in the PF instead by setting the range to PF(4) <= PF(A) <= PF(6). In the
  case of RANGE PARTITIONING and LIST PARTITIONING this can be used to
  calculate a set of partitions rather than scanning all of them.
  Thus the following prerequisites are there to check if sets of partitions
  can be found.
  1) Only possible for RANGE and LIST partitioning (not for subpartitioning)
  2) Only possible if PF only contains 1 field
  3) Possible if PF is a growing function of the field
  4) Possible if PF is a shrinking function of the field
  OBSERVATION:
  1) IF f1(A) is a growing function AND f2(A) is a growing function THEN
     f1(A) + f2(A) is a growing function
     f1(A) * f2(A) is a growing function if f1(A) >= 0 and f2(A) >= 0
  2) IF f1(A) is a growing function and f2(A) is a shrinking function THEN
     f1(A) / f2(A) is a growing function if f1(A) >= 0 and f2(A) > 0
  3) IF A is a growing function then a function f(A) that removes the
     least significant portion of A is a growing function
     E.g. DATE(datetime) is a growing function
     MONTH(datetime) is not a growing/shrinking function
  4) IF f1(A) is a growing function and f2(A) is a growing function THEN
     f1(f2(A)) and f2(f1(A)) are also growing functions
  5) IF f1(A) is a shrinking function and f2(A) is a growing function THEN
     f1(f2(A)) is a shrinking function and f2(f1(A)) is a shrinking function
  6) f1(A) = A is a growing function
  7) f1(A) = A*a + b (where a and b are constants) is a growing function

  By analysing the item tree of the PF we can use these deducements and
  derive whether the PF is a growing function or a shrinking function or
  neither of it.

  If the PF is range capable then a flag is set on the table object
  indicating this to notify that we can use also ranges on the field
  of the PF to deduce a set of partitions if the fields of the PF were
  not all fully bound.
  SYNOPSIS
    check_range_capable_PF()
    table                TABLE object for which partition fields are set-up
  DESCRIPTION
    Support for this is not implemented yet.
*/

void check_range_capable_PF(TABLE *table)
{
  DBUG_ENTER("check_range_capable_PF");
  DBUG_VOID_RETURN;
}


/*
  Set up partition key maps
  SYNOPSIS
    set_up_partition_key_maps()
    table                TABLE object for which partition fields are set-up
    part_info            Reference to partitioning data structure
  RETURN VALUES
    None
  DESCRIPTION
  This function sets up a couple of key maps to be able to quickly check
  if an index ever can be used to deduce the partition fields or even
  a part of the fields of the  partition function.
  We set up the following key_map's.
  PF = Partition Function
  1) All fields of the PF is set even by equal on the first fields in the
     key
  2) All fields of the PF is set if all fields of the key is set
  3) At least one field in the PF is set if all fields is set
  4) At least one field in the PF is part of the key
*/

static void set_up_partition_key_maps(TABLE *table,
                                      partition_info *part_info)
{
  uint keys= table->s->keys, i;
  bool all_fields, some_fields;
  DBUG_ENTER("set_up_partition_key_maps");

  part_info->all_fields_in_PF.clear_all();
  part_info->all_fields_in_PPF.clear_all();
  part_info->all_fields_in_SPF.clear_all();
  part_info->some_fields_in_PF.clear_all();
  for (i= 0; i < keys; i++)
  {
    set_indicator_in_key_fields(table->key_info+i);
    check_fields_in_PF(part_info->full_part_field_array,
                       &all_fields, &some_fields);
    if (all_fields)
      part_info->all_fields_in_PF.set_bit(i);
    if (some_fields)
      part_info->some_fields_in_PF.set_bit(i);
    if (is_sub_partitioned(part_info))
    {
      check_fields_in_PF(part_info->part_field_array,
                         &all_fields, &some_fields);
      if (all_fields)
        part_info->all_fields_in_PPF.set_bit(i);
      check_fields_in_PF(part_info->subpart_field_array,
                         &all_fields, &some_fields);
      if (all_fields)
        part_info->all_fields_in_SPF.set_bit(i);
    }
    clear_indicator_in_key_fields(table->key_info+i);
  }
  DBUG_VOID_RETURN;
}


/*
  Set-up all function pointers for calculation of partition id,
  subpartition id and the upper part in subpartitioning. This is to speed up
  execution of get_partition_id which is executed once every record to be
  written and deleted and twice for updates.
  SYNOPSIS
    set_up_partition_function_pointers()
    part_info            Reference to partitioning data structure
*/

static void set_up_partition_func_pointers(partition_info *part_info)
{
  if (is_sub_partitioned(part_info))
  {
    if (part_info->part_type == RANGE_PARTITION)
    {
      part_info->get_part_partition_id= get_partition_id_range;
      if (part_info->list_of_subpart_fields)
      {
        if (part_info->linear_hash_ind)
        {
          part_info->get_partition_id= get_partition_id_range_sub_linear_key;
          part_info->get_subpartition_id= get_partition_id_linear_key_sub;
        }
        else
        {
          part_info->get_partition_id= get_partition_id_range_sub_key;
          part_info->get_subpartition_id= get_partition_id_key_sub;
        }
      }
      else
      {
        if (part_info->linear_hash_ind)
        {
          part_info->get_partition_id= get_partition_id_range_sub_linear_hash;
          part_info->get_subpartition_id= get_partition_id_linear_hash_sub;
        }
        else
        {
          part_info->get_partition_id= get_partition_id_range_sub_hash;
          part_info->get_subpartition_id= get_partition_id_hash_sub;
        }
      }
    }
    else //LIST Partitioning
    {
      part_info->get_part_partition_id= get_partition_id_list;
      if (part_info->list_of_subpart_fields)
      {
        if (part_info->linear_hash_ind)
        {
          part_info->get_partition_id= get_partition_id_list_sub_linear_key;
          part_info->get_subpartition_id= get_partition_id_linear_key_sub;
        }
        else
        {
          part_info->get_partition_id= get_partition_id_list_sub_key;
          part_info->get_subpartition_id= get_partition_id_key_sub;
        }
      }
      else
      {
        if (part_info->linear_hash_ind)
        {
          part_info->get_partition_id= get_partition_id_list_sub_linear_hash;
          part_info->get_subpartition_id= get_partition_id_linear_hash_sub;
        }
        else
        {
          part_info->get_partition_id= get_partition_id_list_sub_hash;
          part_info->get_subpartition_id= get_partition_id_hash_sub;
        }
      }
    }
  }
  else //No subpartitioning
  {
    part_info->get_part_partition_id= NULL;
    part_info->get_subpartition_id= NULL;
    if (part_info->part_type == RANGE_PARTITION)
      part_info->get_partition_id= get_partition_id_range;
    else if (part_info->part_type == LIST_PARTITION)
      part_info->get_partition_id= get_partition_id_list;
    else //HASH partitioning
    {
      if (part_info->list_of_part_fields)
      {
        if (part_info->linear_hash_ind)
          part_info->get_partition_id= get_partition_id_linear_key_nosub;
        else
          part_info->get_partition_id= get_partition_id_key_nosub;
      }
      else
      {
        if (part_info->linear_hash_ind)
          part_info->get_partition_id= get_partition_id_linear_hash_nosub;
        else
          part_info->get_partition_id= get_partition_id_hash_nosub;
      }
    }
  }
}
          
        
/*
  For linear hashing we need a mask which is on the form 2**n - 1 where
  2**n >= no_parts. Thus if no_parts is 6 then mask is 2**3 - 1 = 8 - 1 = 7.
  SYNOPSIS
    set_linear_hash_mask()
    part_info            Reference to partitioning data structure
    no_parts             Number of parts in linear hash partitioning
*/

static void set_linear_hash_mask(partition_info *part_info, uint no_parts)
{
  uint mask;
  for (mask= 1; mask < no_parts; mask<<=1)
    ;
  part_info->linear_hash_mask= mask - 1;
}


/*
  This function calculates the partition id provided the result of the hash
  function using linear hashing parameters, mask and number of partitions.
  SYNOPSIS
    get_part_id_from_linear_hash()
    hash_value          Hash value calculated by HASH function or KEY function
    mask                Mask calculated previously by set_linear_hash_mask
    no_parts            Number of partitions in HASH partitioned part
  RETURN VALUE
    part_id             The calculated partition identity (starting at 0)
  DESCRIPTION
    The partition is calculated according to the theory of linear hashing.
    See e.g. Linear hashing: a new tool for file and table addressing,
    Reprinted from VLDB-80 in Readings Database Systems, 2nd ed, M. Stonebraker
    (ed.), Morgan Kaufmann 1994.
*/

static uint32 get_part_id_from_linear_hash(longlong hash_value, uint mask,
                                           uint no_parts)
{
  uint32 part_id= (uint32)(hash_value & mask);
  if (part_id >= no_parts)
  {
    uint new_mask= ((mask + 1) >> 1) - 1;
    part_id= hash_value & new_mask;
  }
  return part_id;
}

/*
  This function is called as part of opening the table by opening the .frm
  file. It is a part of CREATE TABLE to do this so it is quite permissible
  that errors due to erroneus syntax isn't found until we come here.
  If the user has used a non-existing field in the table is one such example
  of an error that is not discovered until here.
  SYNOPSIS
    fix_partition_func()
    thd                  The thread object
    name                 The name of the partitioned table
    table                TABLE object for which partition fields are set-up
  RETURN VALUE
    TRUE
    FALSE
  DESCRIPTION
    The name parameter contains the full table name and is used to get the
    database name of the table which is used to set-up a correct
    TABLE_LIST object for use in fix_fields.
*/

bool fix_partition_func(THD *thd, const char* name, TABLE *table)
{
  bool result= TRUE;
  uint dir_length, home_dir_length;
  TABLE_LIST tables;
  TABLE_SHARE *share= table->s;
  char db_name_string[FN_REFLEN];
  char* db_name;
  partition_info *part_info= share->part_info;
  ulong save_set_query_id= thd->set_query_id;
  DBUG_ENTER("fix_partition_func");

  thd->set_query_id= 0;
  /*
  Set-up the TABLE_LIST object to be a list with a single table
  Set the object to zero to create NULL pointers and set alias
  and real name to table name and get database name from file name.
  */

  bzero((void*)&tables, sizeof(TABLE_LIST));
  tables.alias= tables.table_name= (char*)share->table_name;
  tables.table= table;
  tables.next_local= 0;
  tables.next_name_resolution_table= 0;
  strmov(db_name_string, name);
  dir_length= dirname_length(db_name_string);
  db_name_string[dir_length - 1]= 0;
  home_dir_length= dirname_length(db_name_string);
  db_name= &db_name_string[home_dir_length];
  tables.db= db_name;

  if (is_sub_partitioned(part_info))
  {
    DBUG_ASSERT(part_info->subpart_type == HASH_PARTITION);
    /*
     Subpartition is defined. We need to verify that subpartitioning
     function is correct.
    */
    if (part_info->linear_hash_ind)
      set_linear_hash_mask(part_info, part_info->no_subparts);
    if (part_info->list_of_subpart_fields)
    {
      List_iterator<char> it(part_info->subpart_field_list);
      if (unlikely(handle_list_of_fields(it, table, part_info, TRUE)))
        goto end;
    }
    else
    {
      if (unlikely(fix_fields_part_func(thd, &tables,
                   part_info->subpart_expr, part_info, TRUE)))
        goto end;
      if (unlikely(part_info->subpart_expr->result_type() != INT_RESULT))
      {
        my_error(ER_PARTITION_FUNC_NOT_ALLOWED_ERROR, MYF(0),
                 "SUBPARTITION");
        goto end;
      }
    }
  }
  DBUG_ASSERT(part_info->part_type != NOT_A_PARTITION);
  /*
   Partition is defined. We need to verify that partitioning
   function is correct.
  */
  if (part_info->part_type == HASH_PARTITION)
  {
    if (part_info->linear_hash_ind)
      set_linear_hash_mask(part_info, part_info->no_parts);
    if (part_info->list_of_part_fields)
    {
      List_iterator<char> it(part_info->part_field_list);
      if (unlikely(handle_list_of_fields(it, table, part_info, FALSE)))
        goto end;
    }
    else
    {
      if (unlikely(fix_fields_part_func(thd, &tables, part_info->part_expr,
                                        part_info, FALSE)))
        goto end;
      if (unlikely(part_info->part_expr->result_type() != INT_RESULT))
      {
        my_error(ER_PARTITION_FUNC_NOT_ALLOWED_ERROR, MYF(0), part_str);
        goto end;
      }
      part_info->part_result_type= INT_RESULT;
    }
  }
  else
  {
    char *error_str;
    if (part_info->part_type == RANGE_PARTITION)
    {
      error_str= range_str; 
      if (unlikely(check_range_constants(part_info)))
        goto end;
    }
    else if (part_info->part_type == LIST_PARTITION)
    {
      error_str= list_str; 
      if (unlikely(check_list_constants(part_info)))
        goto end;
    }
    else
    {
      DBUG_ASSERT(0);
      my_error(ER_INCONSISTENT_PARTITION_INFO_ERROR, MYF(0));
      goto end;
    }
    if (unlikely(part_info->no_parts < 1))
    {
      my_error(ER_PARTITIONS_MUST_BE_DEFINED_ERROR, MYF(0), error_str);
      goto end;
    }
    if (unlikely(fix_fields_part_func(thd, &tables, part_info->part_expr,
                                      part_info, FALSE)))
      goto end;
    if (unlikely(part_info->part_expr->result_type() != INT_RESULT))
    {
      my_error(ER_PARTITION_FUNC_NOT_ALLOWED_ERROR, MYF(0), part_str);
      goto end;
    }
  }
  if (unlikely(create_full_part_field_array(table, part_info)))
    goto end;
  if (unlikely(check_primary_key(table)))
    goto end;
  if (unlikely((!table->file->partition_flags() & HA_CAN_PARTITION_UNIQUE) &&
               check_unique_keys(table)))
    goto end;
  check_range_capable_PF(table);
  set_up_partition_key_maps(table, part_info);
  set_up_partition_func_pointers(part_info);
  result= FALSE;
end:
  thd->set_query_id= save_set_query_id;
  DBUG_RETURN(result);
}


/*
  The code below is support routines for the reverse parsing of the 
  partitioning syntax. This feature is very useful to generate syntax for
  all default values to avoid all default checking when opening the frm
  file. It is also used when altering the partitioning by use of various
  ALTER TABLE commands. Finally it is used for SHOW CREATE TABLES.
*/

static int add_write(File fptr, const char *buf, uint len)
{
  uint len_written= my_write(fptr, buf, len, MYF(0));
  if (likely(len == len_written))
    return 0;
  else
    return 1;
}

static int add_string(File fptr, const char *string)
{
  return add_write(fptr, string, strlen(string));
}

static int add_string_len(File fptr, const char *string, uint len)
{
  return add_write(fptr, string, len);
}

static int add_space(File fptr)
{
  return add_string(fptr, space_str);
}

static int add_comma(File fptr)
{
  return add_string(fptr, comma_str);
}

static int add_equal(File fptr)
{
  return add_string(fptr, equal_str);
}

static int add_end_parenthesis(File fptr)
{
  return add_string(fptr, end_paren_str);
}

static int add_begin_parenthesis(File fptr)
{
  return add_string(fptr, begin_paren_str);
}

static int add_part_key_word(File fptr, const char *key_string)
{
  int err= add_string(fptr, key_string);
  err+= add_space(fptr);
  return err + add_begin_parenthesis(fptr);
}

static int add_hash(File fptr)
{
  return add_part_key_word(fptr, hash_str);
}

static int add_partition(File fptr)
{
  strxmov(buff, part_str, space_str, NullS);
  return add_string(fptr, buff);
}

static int add_subpartition(File fptr)
{
  int err= add_string(fptr, sub_str);
  return err + add_partition(fptr);
}

static int add_partition_by(File fptr)
{
  strxmov(buff, part_str, space_str, by_str, space_str, NullS);
  return add_string(fptr, buff);
}

static int add_subpartition_by(File fptr)
{
  int err= add_string(fptr, sub_str);
  return err + add_partition_by(fptr);
}

static int add_key_partition(File fptr, List<char> field_list)
{
  uint i, no_fields;
  int err;
  List_iterator<char> part_it(field_list);
  err= add_part_key_word(fptr, key_str);
  no_fields= field_list.elements;
  i= 0;
  do
  {
    const char *field_str= part_it++;
    err+= add_string(fptr, field_str);
    if (i != (no_fields-1))
      err+= add_comma(fptr);
  } while (++i < no_fields);
  return err;
}

static int add_int(File fptr, longlong number)
{
  llstr(number, buff);
  return add_string(fptr, buff);
}

static int add_keyword_string(File fptr, const char *keyword,
                              const char *keystr)
{
  int err= add_string(fptr, keyword);
  err+= add_space(fptr);
  err+= add_equal(fptr);
  err+= add_space(fptr);
  err+= add_string(fptr, keystr);
  return err + add_space(fptr);
}

static int add_keyword_int(File fptr, const char *keyword, longlong num)
{
  int err= add_string(fptr, keyword);
  err+= add_space(fptr);
  err+= add_equal(fptr);
  err+= add_space(fptr);
  err+= add_int(fptr, num);
  return err + add_space(fptr);
}

static int add_engine(File fptr, enum db_type engine_type)
{
  const char *engine_str= ha_get_storage_engine(engine_type);
  int err= add_string(fptr, "ENGINE = ");
  return err + add_string(fptr, engine_str);
  return err;
}

static int add_partition_options(File fptr, partition_element *p_elem)
{
  int err= 0;
  if (p_elem->tablespace_name)
    err+= add_keyword_string(fptr,"TABLESPACE",p_elem->tablespace_name);
  if (p_elem->nodegroup_id != UNDEF_NODEGROUP)
    err+= add_keyword_int(fptr,"NODEGROUP",(longlong)p_elem->nodegroup_id);
  if (p_elem->part_max_rows)
    err+= add_keyword_int(fptr,"MAX_ROWS",(longlong)p_elem->part_max_rows);
  if (p_elem->part_min_rows)
    err+= add_keyword_int(fptr,"MIN_ROWS",(longlong)p_elem->part_min_rows);
  if (p_elem->data_file_name)
    err+= add_keyword_string(fptr,"DATA DIRECTORY",p_elem->data_file_name);
  if (p_elem->index_file_name)
    err+= add_keyword_string(fptr,"INDEX DIRECTORY",p_elem->index_file_name);
  if (p_elem->part_comment)
    err+= add_keyword_string(fptr, "COMMENT",p_elem->part_comment);
  return err + add_engine(fptr,p_elem->engine_type);
}

static int add_partition_values(File fptr, partition_info *part_info,
                         partition_element *p_elem)
{
  int err= 0;
  if (part_info->part_type == RANGE_PARTITION)
  {
    err+= add_string(fptr, "VALUES LESS THAN ");
    if (p_elem->range_value != LONGLONG_MAX)
    {
      err+= add_begin_parenthesis(fptr);
      err+= add_int(fptr, p_elem->range_value);
      err+= add_end_parenthesis(fptr);
    }
    else
      err+= add_string(fptr, "MAXVALUE");
  }
  else if (part_info->part_type == LIST_PARTITION)
  {
    uint i;
    List_iterator<longlong> list_val_it(p_elem->list_val_list);
    err+= add_string(fptr, "VALUES IN ");
    uint no_items= p_elem->list_val_list.elements;
    err+= add_begin_parenthesis(fptr);
    i= 0;
    do
    {
      longlong *list_value= list_val_it++;
      err+= add_int(fptr, *list_value);
      if (i != (no_items-1))
        err+= add_comma(fptr);
    } while (++i < no_items);
    err+= add_end_parenthesis(fptr);
  }
  return err + add_space(fptr);
}

/*
  Generate the partition syntax from the partition data structure.
  Useful for support of generating defaults, SHOW CREATE TABLES
  and easy partition management.
  SYNOPSIS
    generate_partition_syntax()
    part_info                  The partitioning data structure
    buf_length                 A pointer to the returned buffer length
    use_sql_alloc              Allocate buffer from sql_alloc if true
                               otherwise use my_malloc
  RETURN VALUES
    NULL error
    buf, buf_length            Buffer and its length
  DESCRIPTION
  Here we will generate the full syntax for the given command where all
  defaults have been expanded. By so doing the it is also possible to
  make lots of checks of correctness while at it.
  This could will also be reused for SHOW CREATE TABLES and also for all
  type ALTER TABLE commands focusing on changing the PARTITION structure
  in any fashion.

  The implementation writes the syntax to a temporary file (essentially
  an abstraction of a dynamic array) and if all writes goes well it
  allocates a buffer and writes the syntax into this one and returns it.

  As a security precaution the file is deleted before writing into it. This
  means that no other processes on the machine can open and read the file
  while this processing is ongoing.

  The code is optimised for minimal code size since it is not used in any
  common queries.
*/

char *generate_partition_syntax(partition_info *part_info,
                                uint *buf_length,
                                bool use_sql_alloc)
{
  uint i,j, no_parts, no_subparts;
  partition_element *part_elem;
  ulonglong buffer_length;
  char path[FN_REFLEN];
  int err= 0;
  DBUG_ENTER("generate_partition_syntax");
  File fptr;
  char *buf= NULL; //Return buffer
  const char *file_name;
  sprintf(path, "%s_%lx_%lx", "part_syntax", current_pid,
          current_thd->thread_id);
  fn_format(path,path,mysql_tmpdir,".psy", MY_REPLACE_EXT);
  file_name= &path[0];
  DBUG_PRINT("info", ("File name = %s", file_name));
  if (unlikely(((fptr= my_open(file_name,O_CREAT|O_RDWR, MYF(MY_WME))) == -1)))
    DBUG_RETURN(NULL);
#if defined(MSDOS) || defined(__WIN__) || defined(__EMX__) || defined(OS2)
#else
  my_delete(file_name, MYF(0));
#endif
  err+= add_space(fptr);
  err+= add_partition_by(fptr);
  switch (part_info->part_type)
  {
    case RANGE_PARTITION:
      err+= add_part_key_word(fptr, range_str);
      break;
    case LIST_PARTITION:
      err+= add_part_key_word(fptr, list_str);
      break;
    case HASH_PARTITION:
      if (part_info->linear_hash_ind)
        err+= add_string(fptr, "LINEAR ");
      if (part_info->list_of_part_fields)
        err+= add_key_partition(fptr, part_info->part_field_list);
      else
        err+= add_hash(fptr);
      break;
    default:
      DBUG_ASSERT(0);
      /* We really shouldn't get here, no use in continuing from here */
      current_thd->fatal_error();
      DBUG_RETURN(NULL);
  }
  if (part_info->part_expr)
    err+= add_string_len(fptr, part_info->part_func_string,
                         part_info->part_func_len);
  err+= add_end_parenthesis(fptr);
  err+= add_space(fptr);
  if (is_sub_partitioned(part_info))
  {
    err+= add_subpartition_by(fptr);
    /* Must be hash partitioning for subpartitioning */
    if (part_info->list_of_subpart_fields)
      err+= add_key_partition(fptr, part_info->subpart_field_list);
    else
      err+= add_hash(fptr);
    if (part_info->subpart_expr)
      err+= add_string_len(fptr, part_info->subpart_func_string,
                           part_info->subpart_func_len);
    err+= add_end_parenthesis(fptr);
    err+= add_space(fptr);
  }
  err+= add_begin_parenthesis(fptr);
  List_iterator<partition_element> part_it(part_info->partitions);
  no_parts= part_info->no_parts;
  no_subparts= part_info->no_subparts;
  i= 0;
  do
  {
    part_elem= part_it++;
    err+= add_partition(fptr);
    err+= add_string(fptr, part_elem->partition_name);
    err+= add_space(fptr);
    err+= add_partition_values(fptr, part_info, part_elem);
    if (!is_sub_partitioned(part_info))
      err+= add_partition_options(fptr, part_elem);
    if (is_sub_partitioned(part_info))
    {
      err+= add_space(fptr);
      err+= add_begin_parenthesis(fptr);
      List_iterator<partition_element> sub_it(part_elem->subpartitions);
      j= 0;
      do
      {
        part_elem= sub_it++;
        err+= add_subpartition(fptr);
        err+= add_string(fptr, part_elem->partition_name);
        err+= add_space(fptr);
        err+= add_partition_options(fptr, part_elem);
        if (j != (no_subparts-1))
        {
          err+= add_comma(fptr);
          err+= add_space(fptr);
        }
        else
          err+= add_end_parenthesis(fptr);
      } while (++j < no_subparts);
    }
    if (i != (no_parts-1))
    {
      err+= add_comma(fptr);
      err+= add_space(fptr);
    }
    else
      err+= add_end_parenthesis(fptr);
  } while (++i < no_parts);
  if (err)
    goto close_file;
  buffer_length= my_seek(fptr, 0L,MY_SEEK_END,MYF(0));
  if (unlikely(buffer_length == MY_FILEPOS_ERROR))
    goto close_file;
  if (unlikely(my_seek(fptr, 0L, MY_SEEK_SET, MYF(0)) == MY_FILEPOS_ERROR))
    goto close_file;
  *buf_length= (uint)buffer_length;
  if (use_sql_alloc)
    buf= sql_alloc(*buf_length+1);
  else
    buf= my_malloc(*buf_length+1, MYF(MY_WME));
  if (!buf)
    goto close_file;

  if (unlikely(my_read(fptr, buf, *buf_length, MYF(MY_FNABP))))
  {
    if (!use_sql_alloc)
      my_free(buf, MYF(0));
    else
      buf= NULL;
  }
  else
    buf[*buf_length]= 0;

close_file:
  /*
    Delete the file before closing to ensure the file doesn't get synched
    to disk unnecessary. We only used the file system as a dynamic array
    implementation so we are not really interested in getting the file
    present on disk.
    This is not possible on Windows so here it has to be done after closing
    the file. Also on Unix we delete immediately after opening to ensure no
    other process can read the information written into the file.
  */
  my_close(fptr, MYF(0));
#if defined(MSDOS) || defined(__WIN__) || defined(__EMX__) || defined(OS2)
  my_delete(file_name, MYF(0));
#endif
  DBUG_RETURN(buf);
}


/*
  Check if partition key fields are modified and if it can be handled by the
  underlying storage engine.
  SYNOPSIS
    partition_key_modified
    table                TABLE object for which partition fields are set-up
    fields               A list of the to be modifed
  RETURN VALUES
    TRUE                 Need special handling of UPDATE
    FALSE                Normal UPDATE handling is ok
*/

bool partition_key_modified(TABLE *table, List<Item> &fields)
{
  List_iterator_fast<Item> f(fields);
  partition_info *part_info= table->s->part_info;
  Item_field *item_field;
  DBUG_ENTER("partition_key_modified");
  if (!part_info)
    DBUG_RETURN(FALSE);
  if (table->file->partition_flags() & HA_CAN_UPDATE_PARTITION_KEY)
    DBUG_RETURN(FALSE);
  f.rewind();
  while ((item_field=(Item_field*) f++))
    if (item_field->field->flags & FIELD_IN_PART_FUNC_FLAG)
      DBUG_RETURN(TRUE);
  DBUG_RETURN(FALSE);
}


/*
  The next set of functions are used to calculate the partition identity.
  A handler sets up a variable that corresponds to one of these functions
  to be able to quickly call it whenever the partition id needs to calculated
  based on the record in table->record[0] (or set up to fake that).
  There are 4 functions for hash partitioning and 2 for RANGE/LIST partitions.
  In addition there are 4 variants for RANGE subpartitioning and 4 variants
  for LIST subpartitioning thus in total there are 14 variants of this
  function.

  We have a set of support functions for these 14 variants. There are 4
  variants of hash functions and there is a function for each. The KEY
  partitioning uses the function calculate_key_value to calculate the hash
  value based on an array of fields. The linear hash variants uses the
  method get_part_id_from_linear_hash to get the partition id using the
  hash value and some parameters calculated from the number of partitions.
*/

/*
  Calculate hash value for KEY partitioning using an array of fields.
  SYNOPSIS
    calculate_key_value()
    field_array             An array of the fields in KEY partitioning
  RETURN VALUE
    hash_value calculated
  DESCRIPTION
    Uses the hash function on the character set of the field. Integer and
    floating point fields use the binary character set by default.
*/

static uint32 calculate_key_value(Field **field_array)
{
  uint32 hashnr= 0;
  ulong nr2= 4;
  do
  {
    Field *field= *field_array;
    if (field->is_null())
    {
      hashnr^= (hashnr << 1) | 1;
    }
    else
    {
      uint len= field->pack_length();
      ulong nr1= 1;
      CHARSET_INFO *cs= field->charset();
      cs->coll->hash_sort(cs, (uchar*)field->ptr, len, &nr1, &nr2);
      hashnr^= (uint32)nr1;
    }
  } while (*(++field_array));
  return hashnr;
}


/*
  A simple support function to calculate part_id given local part and
  sub part.
  SYNOPSIS
    get_part_id_for_sub()
    loc_part_id             Local partition id
    sub_part_id             Subpartition id
    no_subparts             Number of subparts
*/

inline
static uint32 get_part_id_for_sub(uint32 loc_part_id, uint32 sub_part_id,
                                  uint no_subparts)
{
  return (uint32)((loc_part_id * no_subparts) + sub_part_id);
}


/*
  Calculate part_id for (SUB)PARTITION BY HASH
  SYNOPSIS
    get_part_id_hash()
    no_parts                 Number of hash partitions
    part_expr                Item tree of hash function
  RETURN VALUE
    Calculated partition id
*/

inline
static uint32 get_part_id_hash(uint no_parts,
                               Item *part_expr)
{
  DBUG_ENTER("get_part_id_hash");
  DBUG_RETURN((uint32)(part_expr->val_int() % no_parts));
}


/*
  Calculate part_id for (SUB)PARTITION BY LINEAR HASH
  SYNOPSIS
    get_part_id_linear_hash()
    part_info           A reference to the partition_info struct where all the
                        desired information is given
    no_parts            Number of hash partitions
    part_expr           Item tree of hash function
  RETURN VALUE
    Calculated partition id
*/

inline
static uint32 get_part_id_linear_hash(partition_info *part_info,
                                      uint no_parts,
                                      Item *part_expr)
{
  DBUG_ENTER("get_part_id_linear_hash");
  DBUG_RETURN(get_part_id_from_linear_hash(part_expr->val_int(),
                                           part_info->linear_hash_mask,
                                           no_parts));
}


/*
  Calculate part_id for (SUB)PARTITION BY KEY
  SYNOPSIS
    get_part_id_key()
    field_array         Array of fields for PARTTION KEY
    no_parts            Number of KEY partitions
  RETURN VALUE
    Calculated partition id
*/

inline
static uint32 get_part_id_key(Field **field_array,
                              uint no_parts)
{
  DBUG_ENTER("get_part_id_key");
  DBUG_RETURN(calculate_key_value(field_array) % no_parts);
}


/*
  Calculate part_id for (SUB)PARTITION BY LINEAR KEY
  SYNOPSIS
    get_part_id_linear_key()
    part_info           A reference to the partition_info struct where all the
                        desired information is given
    field_array         Array of fields for PARTTION KEY
    no_parts            Number of KEY partitions
  RETURN VALUE
    Calculated partition id
*/

inline
static uint32 get_part_id_linear_key(partition_info *part_info,
                                     Field **field_array,
                                     uint no_parts)
{
  DBUG_ENTER("get_partition_id_linear_key");
  DBUG_RETURN(get_part_id_from_linear_hash(calculate_key_value(field_array),
                                           part_info->linear_hash_mask,
                                           no_parts));
}

/*
  This function is used to calculate the partition id where all partition
  fields have been prepared to point to a record where the partition field
  values are bound.
  SYNOPSIS
    get_partition_id()
    part_info           A reference to the partition_info struct where all the
                        desired information is given
    part_id             The partition id is returned through this pointer
  RETURN VALUE
    part_id
    return TRUE means that the fields of the partition function didn't fit
    into any partition and thus the values of the PF-fields are not allowed.
  DESCRIPTION
    A routine used from write_row, update_row and delete_row from any
    handler supporting partitioning. It is also a support routine for
    get_partition_set used to find the set of partitions needed to scan
    for a certain index scan or full table scan.
    
    It is actually 14 different variants of this function which are called
    through a function pointer.

    get_partition_id_list
    get_partition_id_range
    get_partition_id_hash_nosub
    get_partition_id_key_nosub
    get_partition_id_linear_hash_nosub
    get_partition_id_linear_key_nosub
    get_partition_id_range_sub_hash
    get_partition_id_range_sub_key
    get_partition_id_range_sub_linear_hash
    get_partition_id_range_sub_linear_key
    get_partition_id_list_sub_hash
    get_partition_id_list_sub_key
    get_partition_id_list_sub_linear_hash
    get_partition_id_list_sub_linear_key
*/

/*
  This function is used to calculate the main partition to use in the case of
  subpartitioning and we don't know enough to get the partition identity in
  total.
  SYNOPSIS
    get_part_partition_id()
    part_info           A reference to the partition_info struct where all the
                        desired information is given
    part_id             The partition id is returned through this pointer
  RETURN VALUE
    part_id
    return TRUE means that the fields of the partition function didn't fit
    into any partition and thus the values of the PF-fields are not allowed.
  DESCRIPTION
    
    It is actually 6 different variants of this function which are called
    through a function pointer.

    get_partition_id_list
    get_partition_id_range
    get_partition_id_hash_nosub
    get_partition_id_key_nosub
    get_partition_id_linear_hash_nosub
    get_partition_id_linear_key_nosub
*/


bool get_partition_id_list(partition_info *part_info,
                           uint32 *part_id)
{
  DBUG_ENTER("get_partition_id_list");
  LIST_PART_ENTRY *list_array= part_info->list_array;
  uint list_index;
  longlong list_value;
  uint min_list_index= 0, max_list_index= part_info->no_list_values - 1;
  longlong part_func_value= part_info->part_expr->val_int();
  while (max_list_index >= min_list_index)
  {
    list_index= (max_list_index + min_list_index) >> 1;
    list_value= list_array[list_index].list_value;
    if (list_value < part_func_value)
      min_list_index= list_index + 1;
    else if (list_value > part_func_value)
      max_list_index= list_index - 1;
    else {
      *part_id= (uint32)list_array[list_index].partition_id;
      DBUG_RETURN(FALSE);
    }
  }
  *part_id= 0;
  DBUG_RETURN(TRUE);
}


bool get_partition_id_range(partition_info *part_info,
                            uint32 *part_id)
{
  DBUG_ENTER("get_partition_id_int_range");
  longlong *range_array= part_info->range_int_array;
  uint max_partition= part_info->no_parts - 1;
  uint min_part_id= 0, max_part_id= max_partition, loc_part_id;
  longlong part_func_value= part_info->part_expr->val_int();
  while (max_part_id > min_part_id)
  {
    loc_part_id= (max_part_id + min_part_id + 1) >> 1;
    if (range_array[loc_part_id] < part_func_value)
      min_part_id= loc_part_id + 1;
    else
      max_part_id= loc_part_id - 1;
  }
  loc_part_id= max_part_id;
  if (part_func_value >= range_array[loc_part_id])
    if (loc_part_id != max_partition)
      loc_part_id++;
  *part_id= (uint32)loc_part_id;
  if (loc_part_id == max_partition)
    if (range_array[loc_part_id] != LONGLONG_MAX)
      if (part_func_value >= range_array[loc_part_id])
        DBUG_RETURN(TRUE);
  DBUG_RETURN(FALSE);
}

bool get_partition_id_hash_nosub(partition_info *part_info,
                                 uint32 *part_id)
{
  *part_id= get_part_id_hash(part_info->no_parts, part_info->part_expr);
  return FALSE;
}


bool get_partition_id_linear_hash_nosub(partition_info *part_info,
                                        uint32 *part_id)
{
  *part_id= get_part_id_linear_hash(part_info, part_info->no_parts,
                                    part_info->part_expr);
  return FALSE;
}


bool get_partition_id_key_nosub(partition_info *part_info,
                                uint32 *part_id)
{
  *part_id= get_part_id_key(part_info->part_field_array, part_info->no_parts);
  return FALSE;
}


bool get_partition_id_linear_key_nosub(partition_info *part_info,
                                       uint32 *part_id)
{
  *part_id= get_part_id_linear_key(part_info,
                                   part_info->part_field_array,
                                   part_info->no_parts);
  return FALSE;
}


bool get_partition_id_range_sub_hash(partition_info *part_info,
                                     uint32 *part_id)
{
  uint32 loc_part_id, sub_part_id;
  uint no_subparts;
  DBUG_ENTER("get_partition_id_range_sub_hash");
  if (unlikely(get_partition_id_range(part_info, &loc_part_id)))
  {
    DBUG_RETURN(TRUE);
  }
  no_subparts= part_info->no_subparts;
  sub_part_id= get_part_id_hash(no_subparts, part_info->subpart_expr);
  *part_id= get_part_id_for_sub(loc_part_id, sub_part_id, no_subparts);
  DBUG_RETURN(FALSE);
}


bool get_partition_id_range_sub_linear_hash(partition_info *part_info,
                                            uint32 *part_id)
{
  uint32 loc_part_id, sub_part_id;
  uint no_subparts;
  DBUG_ENTER("get_partition_id_range_sub_linear_hash");
  if (unlikely(get_partition_id_range(part_info, &loc_part_id)))
  {
    DBUG_RETURN(TRUE);
  }
  no_subparts= part_info->no_subparts;
  sub_part_id= get_part_id_linear_hash(part_info, no_subparts,
                                       part_info->subpart_expr);
  *part_id= get_part_id_for_sub(loc_part_id, sub_part_id, no_subparts);
  DBUG_RETURN(FALSE);
}


bool get_partition_id_range_sub_key(partition_info *part_info,
                                    uint32 *part_id)
{
  uint32 loc_part_id, sub_part_id;
  uint no_subparts;
  DBUG_ENTER("get_partition_id_range_sub_key");
  if (unlikely(get_partition_id_range(part_info, &loc_part_id)))
  {
    DBUG_RETURN(TRUE);
  }
  no_subparts= part_info->no_subparts;
  sub_part_id= get_part_id_key(part_info->subpart_field_array, no_subparts);
  *part_id= get_part_id_for_sub(loc_part_id, sub_part_id, no_subparts);
  DBUG_RETURN(FALSE);
}


bool get_partition_id_range_sub_linear_key(partition_info *part_info,
                                           uint32 *part_id)
{
  uint32 loc_part_id, sub_part_id;
  uint no_subparts;
  DBUG_ENTER("get_partition_id_range_sub_linear_key");
  if (unlikely(get_partition_id_range(part_info, &loc_part_id)))
  {
    DBUG_RETURN(TRUE);
  }
  no_subparts= part_info->no_subparts;
  sub_part_id= get_part_id_linear_key(part_info,
                                      part_info->subpart_field_array,
                                      no_subparts);
  *part_id= get_part_id_for_sub(loc_part_id, sub_part_id, no_subparts);
  DBUG_RETURN(FALSE);
}


bool get_partition_id_list_sub_hash(partition_info *part_info,
                                    uint32 *part_id)
{
  uint32 loc_part_id, sub_part_id;
  uint no_subparts;
  DBUG_ENTER("get_partition_id_list_sub_hash");
  if (unlikely(get_partition_id_list(part_info, &loc_part_id)))
  {
    DBUG_RETURN(TRUE);
  }
  no_subparts= part_info->no_subparts;
  sub_part_id= get_part_id_hash(no_subparts, part_info->subpart_expr);
  *part_id= get_part_id_for_sub(loc_part_id, sub_part_id, no_subparts);
  DBUG_RETURN(FALSE);
}


bool get_partition_id_list_sub_linear_hash(partition_info *part_info,
                                           uint32 *part_id)
{
  uint32 loc_part_id, sub_part_id;
  uint no_subparts;
  DBUG_ENTER("get_partition_id_list_sub_linear_hash");
  if (unlikely(get_partition_id_list(part_info, &loc_part_id)))
  {
    DBUG_RETURN(TRUE);
  }
  no_subparts= part_info->no_subparts;
  sub_part_id= get_part_id_hash(no_subparts, part_info->subpart_expr);
  *part_id= get_part_id_for_sub(loc_part_id, sub_part_id, no_subparts);
  DBUG_RETURN(FALSE);
}


bool get_partition_id_list_sub_key(partition_info *part_info,
                                   uint32 *part_id)
{
  uint32 loc_part_id, sub_part_id;
  uint no_subparts;
  DBUG_ENTER("get_partition_id_range_sub_key");
  if (unlikely(get_partition_id_list(part_info, &loc_part_id)))
  {
    DBUG_RETURN(TRUE);
  }
  no_subparts= part_info->no_subparts;
  sub_part_id= get_part_id_key(part_info->subpart_field_array, no_subparts);
  *part_id= get_part_id_for_sub(loc_part_id, sub_part_id, no_subparts);
  DBUG_RETURN(FALSE);
}


bool get_partition_id_list_sub_linear_key(partition_info *part_info,
                                          uint32 *part_id)
{
  uint32 loc_part_id, sub_part_id;
  uint no_subparts;
  DBUG_ENTER("get_partition_id_list_sub_linear_key");
  if (unlikely(get_partition_id_list(part_info, &loc_part_id)))
  {
    DBUG_RETURN(TRUE);
  }
  no_subparts= part_info->no_subparts;
  sub_part_id= get_part_id_linear_key(part_info,
                                      part_info->subpart_field_array,
                                      no_subparts);
  *part_id= get_part_id_for_sub(loc_part_id, sub_part_id, no_subparts);
  DBUG_RETURN(FALSE);
}


/*
  This function is used to calculate the subpartition id
  SYNOPSIS
    get_subpartition_id()
    part_info           A reference to the partition_info struct where all the
                        desired information is given
  RETURN VALUE
    part_id
    The subpartition identity
  DESCRIPTION
    A routine used in some SELECT's when only partial knowledge of the
    partitions is known.
    
    It is actually 4 different variants of this function which are called
    through a function pointer.

    get_partition_id_hash_sub
    get_partition_id_key_sub
    get_partition_id_linear_hash_sub
    get_partition_id_linear_key_sub
*/

uint32 get_partition_id_hash_sub(partition_info *part_info)
{
  return get_part_id_hash(part_info->no_subparts, part_info->subpart_expr);
}


uint32 get_partition_id_linear_hash_sub(partition_info *part_info)
{
  return get_part_id_linear_hash(part_info, part_info->no_subparts,
                                 part_info->subpart_expr);
}


uint32 get_partition_id_key_sub(partition_info *part_info)
{
  return get_part_id_key(part_info->subpart_field_array,
                         part_info->no_subparts);
}


uint32 get_partition_id_linear_key_sub(partition_info *part_info)
{
  return get_part_id_linear_key(part_info,
                                part_info->subpart_field_array,
                                part_info->no_subparts);
}


/*
  Set an indicator on all partition fields that are set by the key 
  SYNOPSIS
    set_PF_fields_in_key()
    key_info                   Information about the index
    key_length                 Length of key
  RETURN VALUE
    TRUE                       Found partition field set by key
    FALSE                      No partition field set by key
*/

static bool set_PF_fields_in_key(KEY *key_info, uint key_length)
{
  KEY_PART_INFO *key_part;
  bool found_part_field= FALSE;
  DBUG_ENTER("set_PF_fields_in_key");

  for (key_part= key_info->key_part; (int)key_length > 0; key_part++)
  {
    if (key_part->null_bit)
      key_length--;
    if (key_part->type == HA_KEYTYPE_BIT)
    {
      if (((Field_bit*)key_part->field)->bit_len)
        key_length--;
    }
    if (key_part->key_part_flag & (HA_BLOB_PART + HA_VAR_LENGTH_PART))
    {
      key_length-= HA_KEY_BLOB_LENGTH;
    }
    if (key_length < key_part->length)
      break;
    key_length-= key_part->length;
    if (key_part->field->flags & FIELD_IN_PART_FUNC_FLAG)
    {
      found_part_field= TRUE;
      key_part->field->flags|= GET_FIXED_FIELDS_FLAG;
    }
  }
  DBUG_RETURN(found_part_field);
}


/*
  We have found that at least one partition field was set by a key, now
  check if a partition function has all its fields bound or not.
  SYNOPSIS
    check_part_func_bound()
    ptr                     Array of fields NULL terminated (partition fields)
  RETURN VALUE
    TRUE                    All fields in partition function are set
    FALSE                   Not all fields in partition function are set
*/

static bool check_part_func_bound(Field **ptr)
{
  bool result= TRUE;
  DBUG_ENTER("check_part_func_bound");

  for (; *ptr; ptr++)
  {
    if (!((*ptr)->flags & GET_FIXED_FIELDS_FLAG))
    {
      result= FALSE;
      break;
    }
  }
  DBUG_RETURN(result);
}


/*
  Get the id of the subpartitioning part by using the key buffer of the
  index scan.
  SYNOPSIS
    get_sub_part_id_from_key()
    table         The table object
    buf           A buffer that can be used to evaluate the partition function
    key_info      The index object
    key_spec      A key_range containing key and key length
  RETURN VALUES
    part_id       Subpartition id to use
  DESCRIPTION
    Use key buffer to set-up record in buf, move field pointers and
    get the partition identity and restore field pointers afterwards.
*/

static uint32 get_sub_part_id_from_key(const TABLE *table,byte *buf,
                                       KEY *key_info,
                                       const key_range *key_spec)
{
  byte *rec0= table->record[0];
  partition_info *part_info= table->s->part_info;
  uint32 part_id;
  DBUG_ENTER("get_sub_part_id_from_key");

  key_restore(buf, (byte*)key_spec->key, key_info, key_spec->length);
  if (likely(rec0 == buf))
    part_id= part_info->get_subpartition_id(part_info);
  else
  {
    Field **part_field_array= part_info->subpart_field_array;
    set_field_ptr(part_field_array, buf, rec0);
    part_id= part_info->get_subpartition_id(part_info);
    set_field_ptr(part_field_array, rec0, buf);
  }
  DBUG_RETURN(part_id);
}

/*
  Get the id of the partitioning part by using the key buffer of the
  index scan.
  SYNOPSIS
    get_part_id_from_key()
    table         The table object
    buf           A buffer that can be used to evaluate the partition function
    key_info      The index object
    key_spec      A key_range containing key and key length
    part_id       Partition to use
  RETURN VALUES
    TRUE          Partition to use not found
    FALSE         Ok, part_id indicates partition to use
  DESCRIPTION
    Use key buffer to set-up record in buf, move field pointers and
    get the partition identity and restore field pointers afterwards.
*/
bool get_part_id_from_key(const TABLE *table, byte *buf, KEY *key_info,
                          const key_range *key_spec, uint32 *part_id)
{
  bool result;
  byte *rec0= table->record[0];
  partition_info *part_info= table->s->part_info;
  DBUG_ENTER("get_part_id_from_key");

  key_restore(buf, (byte*)key_spec->key, key_info, key_spec->length);
  if (likely(rec0 == buf))
    result= part_info->get_part_partition_id(part_info, part_id);
  else
  {
    Field **part_field_array= part_info->part_field_array;
    set_field_ptr(part_field_array, buf, rec0);
    result= part_info->get_part_partition_id(part_info, part_id);
    set_field_ptr(part_field_array, rec0, buf);
  }
  DBUG_RETURN(result);
}

/*
  Get the partitioning id of the full PF by using the key buffer of the
  index scan.
  SYNOPSIS
    get_full_part_id_from_key()
    table         The table object
    buf           A buffer that is used to evaluate the partition function
    key_info      The index object
    key_spec      A key_range containing key and key length
    part_spec     A partition id containing start part and end part
  RETURN VALUES
    part_spec
    No partitions to scan is indicated by end_part > start_part when returning
  DESCRIPTION
    Use key buffer to set-up record in buf, move field pointers if needed and
    get the partition identity and restore field pointers afterwards.
*/

void get_full_part_id_from_key(const TABLE *table, byte *buf,
                               KEY *key_info,
                               const key_range *key_spec,
                               part_id_range *part_spec)
{
  bool result;
  partition_info *part_info= table->s->part_info;
  byte *rec0= table->record[0];
  DBUG_ENTER("get_full_part_id_from_key");

  key_restore(buf, (byte*)key_spec->key, key_info, key_spec->length);
  if (likely(rec0 == buf))
    result= part_info->get_partition_id(part_info, &part_spec->start_part);
  else
  {
    Field **part_field_array= part_info->full_part_field_array;
    set_field_ptr(part_field_array, buf, rec0);
    result= part_info->get_partition_id(part_info, &part_spec->start_part);
    set_field_ptr(part_field_array, rec0, buf);
  }
  part_spec->end_part= part_spec->start_part;
  if (unlikely(result))
    part_spec->start_part++;
  DBUG_VOID_RETURN;
}
    
/*
  Get the set of partitions to use in query.
  SYNOPSIS
    get_partition_set()
    table         The table object
    buf           A buffer that can be used to evaluate the partition function
    index         The index of the key used, if MAX_KEY no index used
    key_spec      A key_range containing key and key length
    part_spec     Contains start part, end part and indicator if bitmap is
                  used for which partitions to scan
  DESCRIPTION
    This function is called to discover which partitions to use in an index
    scan or a full table scan.
    It returns a range of partitions to scan. If there are holes in this
    range with partitions that are not needed to scan a bit array is used
    to signal which partitions to use and which not to use.
    If start_part > end_part at return it means no partition needs to be
    scanned. If start_part == end_part it always means a single partition
    needs to be scanned.
  RETURN VALUE
    part_spec
*/
void get_partition_set(const TABLE *table, byte *buf, const uint index,
                       const key_range *key_spec, part_id_range *part_spec)
{
  partition_info *part_info= table->s->part_info;
  uint no_parts= get_tot_partitions(part_info), i, part_id;
  uint sub_part= no_parts, part_part= no_parts;
  KEY *key_info= NULL;
  bool found_part_field= FALSE;
  DBUG_ENTER("get_partition_set");

  part_spec->use_bit_array= FALSE;
  part_spec->start_part= 0;
  part_spec->end_part= no_parts - 1;
  if ((index < MAX_KEY) && 
       key_spec->flag == (uint)HA_READ_KEY_EXACT &&
       part_info->some_fields_in_PF.is_set(index))
  {
    key_info= table->key_info+index;
    /*
      The index can potentially provide at least one PF-field (field in the
      partition function). Thus it is interesting to continue our probe.
    */
    if (key_spec->length == key_info->key_length)
    {
      /*
        The entire key is set so we can check whether we can immediately
        derive either the complete PF or if we can derive either
        the top PF or the subpartitioning PF. This can be established by
        checking precalculated bits on each index.
      */
      if (part_info->all_fields_in_PF.is_set(index))
      {
        /*
          We can derive the exact partition to use, no more than this one
          is needed.
        */
        get_full_part_id_from_key(table,buf,key_info,key_spec,part_spec);
        DBUG_VOID_RETURN;
      }
      else if (is_sub_partitioned(part_info))
      {
        if (part_info->all_fields_in_SPF.is_set(index))
          sub_part= get_sub_part_id_from_key(table, buf, key_info, key_spec);
        else if (part_info->all_fields_in_PPF.is_set(index))
        {
          if (get_part_id_from_key(table,buf,key_info,key_spec,&part_part))
          {
            /*
              The value of the RANGE or LIST partitioning was outside of
              allowed values. Thus it is certain that the result of this
              scan will be empty.
            */
            part_spec->start_part= no_parts;
            DBUG_VOID_RETURN;
          }
        }
      }
    }
    else
    {
      /*
        Set an indicator on all partition fields that are bound.
        If at least one PF-field was bound it pays off to check whether
        the PF or PPF or SPF has been bound.
        (PF = Partition Function, SPF = Subpartition Function and
         PPF = Partition Function part of subpartitioning)
      */
      if ((found_part_field= set_PF_fields_in_key(key_info,
                                                  key_spec->length)))
      {
        if (check_part_func_bound(part_info->full_part_field_array))
        {
          /*
            We were able to bind all fields in the partition function even
            by using only a part of the key. Calculate the partition to use.
          */
          get_full_part_id_from_key(table,buf,key_info,key_spec,part_spec);
          clear_indicator_in_key_fields(key_info);
          DBUG_VOID_RETURN; 
        }
        else if (check_part_func_bound(part_info->part_field_array))
          sub_part= get_sub_part_id_from_key(table, buf, key_info, key_spec);
        else if (check_part_func_bound(part_info->subpart_field_array))
        {
          if (get_part_id_from_key(table,buf,key_info,key_spec,&part_part))
          {
            part_spec->start_part= no_parts;
            clear_indicator_in_key_fields(key_info);
            DBUG_VOID_RETURN;
          }
        }
      }
    }
  }
  {
    /*
      The next step is to analyse the table condition to see whether any
      information about which partitions to scan can be derived from there.
      Currently not implemented.
    */
  }
  /*
    If we come here we have found a range of sorts we have either discovered
    nothing or we have discovered a range of partitions with possible holes
    in it. We need a bitvector to further the work here.
  */
  if (!(part_part == no_parts && sub_part == no_parts))
  {
    /*
      We can only arrive here if we are using subpartitioning.
    */
    if (part_part != no_parts)
    {
      /*
        We know the top partition and need to scan all underlying
        subpartitions. This is a range without holes.
      */
      DBUG_ASSERT(sub_part == no_parts);
      part_spec->start_part= part_part * part_info->no_parts;
      part_spec->end_part= part_spec->start_part+part_info->no_subparts - 1;
    }
    else
    {
      DBUG_ASSERT(sub_part != no_parts);
      part_spec->use_bit_array= TRUE;
      part_spec->start_part= sub_part;
      part_spec->end_part=sub_part+
                           (part_info->no_subparts*(part_info->no_parts-1));
      for (i= 0, part_id= sub_part; i < part_info->no_parts;
           i++, part_id+= part_info->no_subparts)
        ; //Set bit part_id in bit array
    }
  }
  if (found_part_field)
    clear_indicator_in_key_fields(key_info);
  DBUG_VOID_RETURN;
}


/*
   If the table is partitioned we will read the partition info into the
   .frm file here.
   -------------------------------
   |  Fileinfo     64 bytes      |
   -------------------------------
   | Formnames     7 bytes       |
   -------------------------------
   | Not used    4021 bytes      |
   -------------------------------
   | Keyinfo + record            |
   -------------------------------
   | Padded to next multiple     |
   | of IO_SIZE                  |
   -------------------------------
   | Forminfo     288 bytes      |
   -------------------------------
   | Screen buffer, to make      |
   | field names readable        |
   -------------------------------
   | Packed field info           |
   | 17 + 1 + strlen(field_name) |
   | + 1 end of file character   |
   -------------------------------
   | Partition info              |
   -------------------------------
   We provide the length of partition length in Fileinfo[55-58].

   Read the partition syntax from the frm file and parse it to get the
   data structures of the partitioning.
   SYNOPSIS
     mysql_unpack_partition()
     file                          File reference of frm file
     thd                           Thread object
     part_info_len                 Length of partition syntax
     table                         Table object of partitioned table
   RETURN VALUE
     TRUE                          Error
     FALSE                         Sucess
   DESCRIPTION
     Read the partition syntax from the current position in the frm file.
     Initiate a LEX object, save the list of item tree objects to free after
     the query is done. Set-up partition info object such that parser knows
     it is called from internally. Call parser to create data structures
     (best possible recreation of item trees and so forth since there is no
     serialisation of these objects other than in parseable text format).
     We need to save the text of the partition functions since it is not
     possible to retrace this given an item tree.
*/

bool mysql_unpack_partition(File file, THD *thd, uint part_info_len,
                            TABLE* table)
{
  Item *thd_free_list= thd->free_list;
  bool result= TRUE;
  uchar* part_buf= NULL;
  partition_info *part_info;
  LEX *old_lex= thd->lex, lex;
  DBUG_ENTER("mysql_unpack_partition");
  if (read_string(file, (gptr*)&part_buf, part_info_len))
    DBUG_RETURN(result);
  thd->lex= &lex;
  lex_start(thd, part_buf, part_info_len);
  /*
    We need to use the current SELECT_LEX since I need to keep the
    Name_resolution_context object which is referenced from the
    Item_field objects.
    This is not a nice solution since if the parser uses current_select
    for anything else it will corrupt the current LEX object.
  */
  thd->lex->current_select= old_lex->current_select; 
  /*
    All Items created is put into a free list on the THD object. This list
    is used to free all Item objects after completing a query. We don't
    want that to happen with the Item tree created as part of the partition
    info. This should be attached to the table object and remain so until
    the table object is released.
    Thus we move away the current list temporarily and start a new list that
    we then save in the partition info structure.
  */
  thd->free_list= NULL;
  lex.part_info= (partition_info*)1; //Indicate yyparse from this place
  if (yyparse((void*)thd) || thd->is_fatal_error)
  {
    free_items(thd->free_list);
    goto end;
  }
  part_info= lex.part_info;
  table->s->part_info= part_info;
  part_info->item_free_list= thd->free_list;

  {
  /*
    This code part allocates memory for the serialised item information for
    the partition functions. In most cases this is not needed but if the
    table is used for SHOW CREATE TABLES or ALTER TABLE that modifies
    partition information it is needed and the info is lost if we don't
    save it here so unfortunately we have to do it here even if in most
    cases it is not needed. This is a consequence of that item trees are
    not serialisable.
  */
    uint part_func_len= part_info->part_func_len;
    uint subpart_func_len= part_info->subpart_func_len; 
    char *part_func_string, *subpart_func_string= NULL;
    if (!((part_func_string= sql_alloc(part_func_len))) ||
        (subpart_func_len &&
        !((subpart_func_string= sql_alloc(subpart_func_len)))))
    {
      my_error(ER_OUTOFMEMORY, MYF(0), part_func_len);
      free_items(thd->free_list);
      part_info->item_free_list= 0;
      goto end;
    }
    memcpy(part_func_string, part_info->part_func_string, part_func_len);
    if (subpart_func_len)
      memcpy(subpart_func_string, part_info->subpart_func_string,
             subpart_func_len);
    part_info->part_func_string= part_func_string;
    part_info->subpart_func_string= subpart_func_string;
  }

  result= FALSE;
end:
  thd->free_list= thd_free_list;
  x_free((gptr)part_buf);
  thd->lex= old_lex;
  DBUG_RETURN(result);
}
#endif

/*
  Prepare for calling val_int on partition function by setting fields to
  point to the record where the values of the PF-fields are stored.
  SYNOPSIS
    set_field_ptr()
    ptr                 Array of fields to change ptr
    new_buf             New record pointer
    old_buf             Old record pointer
  DESCRIPTION
    Set ptr in field objects of field array to refer to new_buf record
    instead of previously old_buf. Used before calling val_int and after
    it is used to restore pointers to table->record[0].
    This routine is placed outside of partition code since it can be useful
    also for other programs.
*/

void set_field_ptr(Field **ptr, const byte *new_buf,
                            const byte *old_buf)
{
  my_ptrdiff_t diff= (new_buf - old_buf);
  DBUG_ENTER("set_nullable_field_ptr");

  do
  {
    (*ptr)->move_field(diff);
  } while (*(++ptr));
  DBUG_VOID_RETURN;
}


/*
  Prepare for calling val_int on partition function by setting fields to
  point to the record where the values of the PF-fields are stored.
  This variant works on a key_part reference.
  It is not required that all fields are NOT NULL fields.
  SYNOPSIS
    set_key_field_ptr()
    key_part            key part with a set of fields to change ptr
    new_buf             New record pointer
    old_buf             Old record pointer
  DESCRIPTION
    Set ptr in field objects of field array to refer to new_buf record
    instead of previously old_buf. Used before calling val_int and after
    it is used to restore pointers to table->record[0].
    This routine is placed outside of partition code since it can be useful
    also for other programs.
*/

void set_key_field_ptr(KEY *key_info, const byte *new_buf,
                       const byte *old_buf)
{
  KEY_PART_INFO *key_part= key_info->key_part;
  uint key_parts= key_info->key_parts, i= 0;
  my_ptrdiff_t diff= (new_buf - old_buf);
  DBUG_ENTER("set_key_field_ptr");

  do
  {
    key_part->field->move_field(diff);
    key_part++;
  } while (++i < key_parts);
  DBUG_VOID_RETURN;
}

