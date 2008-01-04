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

/* Some general useful functions */

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation
#endif

#include "mysql_priv.h"

#ifdef WITH_PARTITION_STORAGE_ENGINE
#include "ha_partition.h"


partition_info *partition_info::get_clone()
{
  if (!this)
    return 0;
  List_iterator<partition_element> part_it(partitions);
  partition_element *part;
  partition_info *clone= new partition_info();
  if (!clone)
  {
    mem_alloc_error(sizeof(partition_info));
    return NULL;
  }
  memcpy(clone, this, sizeof(partition_info));
  clone->partitions.empty();

  while ((part= (part_it++)))
  {
    List_iterator<partition_element> subpart_it(part->subpartitions);
    partition_element *subpart;
    partition_element *part_clone= new partition_element();
    if (!part_clone)
    {
      mem_alloc_error(sizeof(partition_element));
      return NULL;
    }
    memcpy(part_clone, part, sizeof(partition_element));
    part_clone->subpartitions.empty();
    while ((subpart= (subpart_it++)))
    {
      partition_element *subpart_clone= new partition_element();
      if (!subpart_clone)
      {
        mem_alloc_error(sizeof(partition_element));
        return NULL;
      }
      memcpy(subpart_clone, subpart, sizeof(partition_element));
      part_clone->subpartitions.push_back(subpart_clone);
    }
    clone->partitions.push_back(part_clone);
  }
  return clone;
}

/*
  Create a memory area where default partition names are stored and fill it
  up with the names.

  SYNOPSIS
    create_default_partition_names()
    part_no                         Partition number for subparts
    no_parts                        Number of partitions
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
                                                     uint no_parts_arg,
                                                     uint start_no)
{
  char *ptr= (char*) sql_calloc(no_parts_arg*MAX_PART_NAME_SIZE);
  char *move_ptr= ptr;
  uint i= 0;
  DBUG_ENTER("create_default_partition_names");

  if (likely(ptr != 0))
  {
    do
    {
      my_sprintf(move_ptr, (move_ptr,"p%u", (start_no + i)));
      move_ptr+=MAX_PART_NAME_SIZE;
    } while (++i < no_parts_arg);
  }
  else
  {
    mem_alloc_error(no_parts_arg*MAX_PART_NAME_SIZE);
  }
  DBUG_RETURN(ptr);
}


/*
  Create a unique name for the subpartition as part_name'sp''subpart_no'
  SYNOPSIS
    create_subpartition_name()
    subpart_no                  Number of subpartition
    part_name                   Name of partition
  RETURN VALUES
    >0                          A reference to the created name string
    0                           Memory allocation error
*/

char *partition_info::create_subpartition_name(uint subpart_no,
                                               const char *part_name)
{
  uint size_alloc= strlen(part_name) + MAX_PART_NAME_SIZE;
  char *ptr= (char*) sql_calloc(size_alloc);
  DBUG_ENTER("create_subpartition_name");

  if (likely(ptr != NULL))
  {
    my_sprintf(ptr, (ptr, "%ssp%u", part_name, subpart_no));
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

  if ((no_parts == 0) &&
      ((no_parts= file->get_default_no_partitions(info)) == 0))
  {
    my_error(ER_PARTITION_NOT_DEFINED_ERROR, MYF(0), "partitions");
    goto end;
  }

  if (unlikely(no_parts > MAX_PARTITIONS))
  {
    my_error(ER_TOO_MANY_PARTITIONS_ERROR, MYF(0));
    goto end;
  }
  if (unlikely((!(default_name= create_default_partition_names(0, no_parts,
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

  if (no_subparts == 0)
    no_subparts= file->get_default_no_partitions(info);
  if (unlikely((no_parts * no_subparts) > MAX_PARTITIONS))
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
        char *ptr= create_subpartition_name(j, part_elem->partition_name);
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
    } while (++j < no_subparts);
  } while (++i < no_parts);
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
  A support function to check if a partition element's name is unique
  
  SYNOPSIS
    has_unique_name()
    partition_element  element to check

  RETURN VALUES
    TRUE               Has unique name
    FALSE              Doesn't
*/

bool partition_info::has_unique_name(partition_element *element)
{
  DBUG_ENTER("partition_info::has_unique_name");
  
  const char *name_to_check= element->partition_name;
  List_iterator<partition_element> parts_it(partitions);
  
  partition_element *el;
  while ((el= (parts_it++)))
  {
    if (!(my_strcasecmp(system_charset_info, el->partition_name, 
                        name_to_check)) && el != element)
        DBUG_RETURN(FALSE);

    if (!el->subpartitions.is_empty()) 
    {
      partition_element *sub_el;    
      List_iterator<partition_element> subparts_it(el->subpartitions);
      while ((sub_el= (subparts_it++)))
      {
        if (!(my_strcasecmp(system_charset_info, sub_el->partition_name, 
                            name_to_check)) && sub_el != element)
            DBUG_RETURN(FALSE);
      }
    }
  } 
  DBUG_RETURN(TRUE);
}


/*
  A support function to check partition names for duplication in a
  partitioned table

  SYNOPSIS
    has_unique_names()

  RETURN VALUES
    TRUE               Has unique part and subpart names
    FALSE              Doesn't

  DESCRIPTION
    Checks that the list of names in the partitions doesn't contain any
    duplicated names.
*/

char *partition_info::has_unique_names()
{
  DBUG_ENTER("partition_info::has_unique_names");
  
  List_iterator<partition_element> parts_it(partitions);

  partition_element *el;  
  while ((el= (parts_it++)))
  {
    if (! has_unique_name(el))
      DBUG_RETURN(el->partition_name);
      
    if (!el->subpartitions.is_empty())
    {
      List_iterator<partition_element> subparts_it(el->subpartitions);
      partition_element *subel;
      while ((subel= (subparts_it++)))
      {
        if (! has_unique_name(subel))
          DBUG_RETURN(subel->partition_name);
      }
    }
  } 
  DBUG_RETURN(NULL);
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
  DESCRIPTION
    Current check verifies only that all handlers are the same.
    Later this check will be more sophisticated.
*/

bool partition_info::check_engine_mix(handlerton **engine_array, uint no_parts)
{
  uint i= 0;
  DBUG_ENTER("partition_info::check_engine_mix");

  do
  {
    if (engine_array[i] != engine_array[0])
    {
      my_error(ER_MIX_HANDLER_ERROR, MYF(0));
      DBUG_RETURN(TRUE);
    }
  } while (++i < no_parts);
  if (engine_array[0]->flags & HTON_NO_PARTITION)
  {
    my_error(ER_PARTITION_MERGE_ERROR, MYF(0));
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


/*
  This routine allocates an array for all range constants to achieve a fast
  check what partition a certain value belongs to. At the same time it does
  also check that the range constants are defined in increasing order and
  that the expressions are constant integer expressions.

  SYNOPSIS
    check_range_constants()

  RETURN VALUE
    TRUE                An error occurred during creation of range constants
    FALSE               Successful creation of range constant mapping

  DESCRIPTION
    This routine is called from check_partition_info to get a quick error
    before we came too far into the CREATE TABLE process. It is also called
    from fix_partition_func every time we open the .frm file. It is only
    called for RANGE PARTITIONed tables.
*/

bool partition_info::check_range_constants()
{
  partition_element* part_def;
  longlong current_largest;
  longlong part_range_value;
  bool first= TRUE;
  uint i;
  List_iterator<partition_element> it(partitions);
  bool result= TRUE;
  bool signed_flag= !part_expr->unsigned_flag;
  DBUG_ENTER("partition_info::check_range_constants");
  DBUG_PRINT("enter", ("INT_RESULT with %d parts", no_parts));

  LINT_INIT(current_largest);

  part_result_type= INT_RESULT;
  range_int_array= (longlong*)sql_alloc(no_parts * sizeof(longlong));
  if (unlikely(range_int_array == NULL))
  {
    mem_alloc_error(no_parts * sizeof(longlong));
    goto end;
  }
  i= 0;
  do
  {
    part_def= it++;
    if ((i != (no_parts - 1)) || !defined_max_value)
    {
      part_range_value= part_def->range_value;
      if (!signed_flag)
        part_range_value-= 0x8000000000000000ULL;
    }
    else
      part_range_value= LONGLONG_MAX;
    if (first)
    {
      current_largest= part_range_value;
      range_int_array[0]= part_range_value;
      first= FALSE;
    }
    else
    {
      if (likely(current_largest < part_range_value))
      {
        current_largest= part_range_value;
        range_int_array[i]= part_range_value;
      }
      else if (defined_max_value &&
               current_largest == part_range_value &&
               part_range_value == LONGLONG_MAX &&
               i == (no_parts - 1))
      {
        range_int_array[i]= part_range_value;
      }
      else
      {
        my_error(ER_RANGE_NOT_INCREASING_ERROR, MYF(0));
        goto end;
      }
    }
  } while (++i < no_parts);
  result= FALSE;
end:
  DBUG_RETURN(result);
}


/*
  Support routines for check_list_constants used by qsort to sort the
  constant list expressions. One routine for unsigned and one for signed.

  SYNOPSIS
    list_part_cmp()
      a                First list constant to compare with
      b                Second list constant to compare with

  RETURN VALUE
    +1                 a > b
    0                  a  == b
    -1                 a < b
*/

int partition_info::list_part_cmp(const void* a, const void* b)
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


/*
  This routine allocates an array for all list constants to achieve a fast
  check what partition a certain value belongs to. At the same time it does
  also check that there are no duplicates among the list constants and that
  that the list expressions are constant integer expressions.

  SYNOPSIS
    check_list_constants()

  RETURN VALUE
    TRUE                  An error occurred during creation of list constants
    FALSE                 Successful creation of list constant mapping

  DESCRIPTION
    This routine is called from check_partition_info to get a quick error
    before we came too far into the CREATE TABLE process. It is also called
    from fix_partition_func every time we open the .frm file. It is only
    called for LIST PARTITIONed tables.
*/

bool partition_info::check_list_constants()
{
  uint i;
  uint list_index= 0;
  part_elem_value *list_value;
  bool result= TRUE;
  longlong curr_value, prev_value, type_add, calc_value;
  partition_element* part_def;
  bool found_null= FALSE;
  List_iterator<partition_element> list_func_it(partitions);
  DBUG_ENTER("partition_info::check_list_constants");

  part_result_type= INT_RESULT;
  no_list_values= 0;
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
      no_list_values++;
  } while (++i < no_parts);
  list_func_it.rewind();
  list_array= (LIST_PART_ENTRY*)sql_alloc((no_list_values+1) *
                                          sizeof(LIST_PART_ENTRY));
  if (unlikely(list_array == NULL))
  {
    mem_alloc_error(no_list_values * sizeof(LIST_PART_ENTRY));
    goto end;
  }

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
  } while (++i < no_parts);

  if (fixed && no_list_values)
  {
    bool first= TRUE;
    my_qsort((void*)list_array, no_list_values, sizeof(LIST_PART_ENTRY), 
             &list_part_cmp);
 
    i= 0;
    LINT_INIT(prev_value);
    do
    {
      DBUG_ASSERT(i < no_list_values);
      curr_value= list_array[i].list_value;
      if (likely(first || prev_value != curr_value))
      {
        prev_value= curr_value;
        first= FALSE;
      }
      else
      {
        my_error(ER_MULTIPLE_DEF_CONST_IN_LIST_PART_ERROR, MYF(0));
        goto end;
      }
    } while (++i < no_list_values);
  }
  result= FALSE;
end:
  DBUG_RETURN(result);
}


/*
  This code is used early in the CREATE TABLE and ALTER TABLE process.

  SYNOPSIS
    check_partition_info()
    file                A reference to a handler of the table
    info                Create info
    engine_type         Return value for used engine in partitions
    check_partition_function Should we check the partition function

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
                                          bool check_partition_function)
{
  handlerton **engine_array= NULL;
  uint part_count= 0;
  uint i, tot_partitions;
  bool result= TRUE;
  char *same_name;
  DBUG_ENTER("partition_info::check_partition_info");

  if (check_partition_function)
  {
    int err= 0;

    if (part_type != HASH_PARTITION || !list_of_part_fields)
    {
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
  }
  if (unlikely(!is_sub_partitioned() && 
               !(use_default_subpartitions && use_default_no_subpartitions)))
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
  if ((same_name= has_unique_names()))
  {
    my_error(ER_SAME_NAME_PARTITION, MYF(0), same_name);
    goto end;
  }
  engine_array= (handlerton**)my_malloc(tot_partitions * sizeof(handlerton *), 
                                        MYF(MY_WME));
  if (unlikely(!engine_array))
    goto end;
  i= 0;
  {
    List_iterator<partition_element> part_it(partitions);
    do
    {
      partition_element *part_elem= part_it++;
      if (part_elem->engine_type == NULL)
        part_elem->engine_type= default_engine_type;
#ifdef HAVE_READLINK
      if (!my_use_symdir || (thd->variables.sql_mode & MODE_NO_DIR_IN_CREATE))
#endif
      {
        if (part_elem->data_file_name)
          push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 0,
                       "DATA DIRECTORY option ignored");
        if (part_elem->index_file_name)
          push_warning(thd, MYSQL_ERROR::WARN_LEVEL_WARN, 0,
                       "INDEX DIRECTORY option ignored");
        part_elem->data_file_name= part_elem->index_file_name= NULL;
      }
      if (!is_sub_partitioned())
      {
        if (check_table_name(part_elem->partition_name,
                             strlen(part_elem->partition_name)))
        {
          my_error(ER_WRONG_PARTITION_NAME, MYF(0));
          goto end;
        }
        DBUG_PRINT("info", ("engine = %d",
                   ha_legacy_type(part_elem->engine_type)));
        engine_array[part_count++]= part_elem->engine_type;
      }
      else
      {
        uint j= 0;
        List_iterator<partition_element> sub_it(part_elem->subpartitions);
        do
        {
          partition_element *sub_elem= sub_it++;
          if (check_table_name(sub_elem->partition_name,
                               strlen(sub_elem->partition_name)))
          {
            my_error(ER_WRONG_PARTITION_NAME, MYF(0));
            goto end;
          }
          if (sub_elem->engine_type == NULL)
            sub_elem->engine_type= default_engine_type;
          DBUG_PRINT("info", ("engine = %u",
                     ha_legacy_type(sub_elem->engine_type)));
          engine_array[part_count++]= sub_elem->engine_type;
        } while (++j < no_subparts);
      }
    } while (++i < no_parts);
  }
  if (unlikely(partition_info::check_engine_mix(engine_array, part_count)))
    goto end;

  if (eng_type)
    *eng_type= (handlerton*)engine_array[0];

  /*
    We need to check all constant expressions that they are of the correct
    type and that they are increasing for ranges and not overlapping for
    list constants.
  */

  if (fixed)
  {
    if (unlikely((part_type == RANGE_PARTITION && check_range_constants()) ||
                  (part_type == LIST_PARTITION && check_list_constants())))
      goto end;
  }
  result= FALSE;
end:
  my_free((char*)engine_array,MYF(MY_ALLOW_ZERO_PTR));
  DBUG_RETURN(result);
}


/*
  Print error for no partition found

  SYNOPSIS
    print_no_partition_found()
    table                        Table object

  RETURN VALUES
*/

void partition_info::print_no_partition_found(TABLE *table)
{
  char buf[100];
  char *buf_ptr= (char*)&buf;
  TABLE_LIST table_list;

  bzero(&table_list, sizeof(table_list));
  table_list.db= table->s->db.str;
  table_list.table_name= table->s->table_name.str;

  if (check_single_table_access(current_thd,
                                SELECT_ACL, &table_list, TRUE))
    my_message(ER_NO_PARTITION_FOR_GIVEN_VALUE,
               ER(ER_NO_PARTITION_FOR_GIVEN_VALUE_SILENT), MYF(0));
  else
  {
    my_bitmap_map *old_map= dbug_tmp_use_all_columns(table, table->read_set);
    if (part_expr->null_value)
      buf_ptr= (char*)"NULL";
    else
      longlong2str(err_value, buf,
                   part_expr->unsigned_flag ? 10 : -10);
    my_error(ER_NO_PARTITION_FOR_GIVEN_VALUE, MYF(0), buf_ptr);
    dbug_tmp_restore_column_map(table->read_set, old_map);
  }
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
      CHARSET_INFO *cs;
      uchar *field_buf;
      LINT_INIT(field_buf);

      if (!field_is_partition_charset(field))
        continue;
      cs= ((Field_str*)field)->charset();
      size= field->pack_length();
      if (!(field_buf= (uchar*) sql_calloc(size)))
        goto error;
      subpart_charset_field_array[i]= field;
      subpart_field_buffers[i++]= field_buf;
    }
    subpart_charset_field_array[i]= NULL;
  }
  if (tot_fields)
  {
    uint k;
    size= tot_fields*sizeof(char**);
    if (!(char_ptrs= (uchar**)sql_calloc(size)))
      goto error;
    full_part_field_buffers= char_ptrs;
    if (!(char_ptrs= (uchar**)sql_calloc(size)))
      goto error;
    restore_full_part_field_ptrs= char_ptrs;
    size= (tot_fields + 1) * sizeof(char**);
    if (!(char_ptrs= (uchar**)sql_calloc(size)))
      goto error;
    full_part_charset_field_array= (Field**)char_ptrs;
    for (i= 0; i < tot_part_fields; i++)
    {
      full_part_charset_field_array[i]= part_charset_field_array[i];
      full_part_field_buffers[i]= part_field_buffers[i];
    }
    k= tot_part_fields;
    for (i= 0; i < tot_subpart_fields; i++)
    {
      uint j;
      bool found= FALSE;
      field= subpart_charset_field_array[i];

      for (j= 0; j < tot_part_fields; j++)
      {
        if (field == part_charset_field_array[i])
          found= TRUE;
      }
      if (!found)
      {
        full_part_charset_field_array[k]= subpart_charset_field_array[i];
        full_part_field_buffers[k]= subpart_field_buffers[i];
        k++;
      }
    }
    full_part_charset_field_array[k]= NULL;
  }
  DBUG_RETURN(FALSE);
error:
  mem_alloc_error(size);
  DBUG_RETURN(TRUE);
}
#endif /* WITH_PARTITION_STORAGE_ENGINE */
