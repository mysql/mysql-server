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

  The first version was written by Mikael Ronstrom.

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
#include "ha_partition.h"

#ifdef WITH_PARTITION_STORAGE_ENGINE


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

#define MAX_PART_NAME_SIZE 16

char *partition_info::create_default_partition_names(uint part_no, uint no_parts, 
                                                     uint start_no, bool is_subpart)
{
  char *ptr= sql_calloc(no_parts*MAX_PART_NAME_SIZE);
  char *move_ptr= ptr;
  uint i= 0;
  DBUG_ENTER("create_default_partition_names");

  if (likely(ptr != 0))
  {
    do
    {
      if (is_subpart)
        my_sprintf(move_ptr, (move_ptr,"p%usp%u", part_no, (start_no + i)));
      else
        my_sprintf(move_ptr, (move_ptr,"p%u", (start_no + i)));
      move_ptr+=MAX_PART_NAME_SIZE;
    } while (++i < no_parts);
  }
  else
  {
    mem_alloc_error(no_parts*MAX_PART_NAME_SIZE);
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

bool partition_info::set_up_default_partitions(handler *file, ulonglong max_rows,
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
  if (no_parts == 0)
    no_parts= file->get_default_no_partitions(max_rows);
  if (unlikely(no_parts > MAX_PARTITIONS))
  {
    my_error(ER_TOO_MANY_PARTITIONS_ERROR, MYF(0));
    goto end;
  }
  if (unlikely((!(default_name= create_default_partition_names(0, no_parts,
                                                               start_no,
                                                               FALSE)))))
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

bool partition_info::set_up_default_subpartitions(handler *file, 
                                                  ulonglong max_rows)
{
  uint i, j; //, no_parts, no_subparts;
  char *default_name, *name_ptr;
  bool result= TRUE;
  partition_element *part_elem;
  List_iterator<partition_element> part_it(partitions);
  DBUG_ENTER("partition_info::set_up_default_subpartitions");

  if (no_subparts == 0)
    no_subparts= file->get_default_no_partitions(max_rows);
//  no_parts= part_info->no_parts;
  //no_subparts= part_info->no_subparts;
  if (unlikely((no_parts * no_subparts) > MAX_PARTITIONS))
  {
    my_error(ER_TOO_MANY_PARTITIONS_ERROR, MYF(0));
    goto end;
  }
//  if (unlikely((!(default_name=
  //           create_default_partition_names(no_subparts, (uint)0, TRUE)))))
    //goto end;
  i= 0;
  do
  {
    part_elem= part_it++;
    j= 0;
    name_ptr= create_default_partition_names(i, no_subparts, (uint)0, TRUE);
    if (unlikely(!name_ptr))
      goto end;
    do
    {
      partition_element *subpart_elem= new partition_element();
      if (likely(subpart_elem != 0 &&
          (!part_elem->subpartitions.push_back(subpart_elem))))
      {
        subpart_elem->engine_type= default_engine_type;
        subpart_elem->partition_name= name_ptr;
        name_ptr+= MAX_PART_NAME_SIZE;
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
    part_info           The reference to all partition information
    file                A reference to a handler of the table
    max_rows            Maximum number of rows stored in the table
    start_no            Starting partition number

  RETURN VALUE
    TRUE                Error, attempted default values not possible
    FALSE               Ok, default partitions set-up

  DESCRIPTION
    Set up defaults for partition or subpartition (cannot set-up for both,
    this will return an error.
*/

bool partition_info::set_up_defaults_for_partitioning(handler *file,
                                                      ulonglong max_rows, 
                                                      uint start_no)
{
  DBUG_ENTER("partition_info::set_up_defaults_for_partitioning");

  if (!default_partitions_setup)
  {
    default_partitions_setup= TRUE;
    if (use_default_partitions)
      DBUG_RETURN(set_up_default_partitions(file, max_rows, start_no));
    if (is_sub_partitioned() && 
        use_default_subpartitions)
      DBUG_RETURN(set_up_default_subpartitions(file, max_rows));
  }
  DBUG_RETURN(FALSE);
}

bool partition_info::has_unique_name(partition_element *element)
{
  DBUG_ENTER("partition_info::has_unique_name");
  
  const char *name_to_check= element->partition_name;
  List_iterator<partition_element> parts_it(partitions);
  
  partition_element *el;
  while (el= (parts_it++))
  {
    if (!(my_strcasecmp(system_charset_info, el->partition_name, 
                        name_to_check)) && el != element)
        DBUG_RETURN(FALSE);

    if (el->subpartitions.is_empty()) continue;
    List_iterator<partition_element> subparts_it(el->subpartitions);
    partition_element *sub_el;
    while (sub_el= (subparts_it++))
    {
      if (!(my_strcasecmp(system_charset_info, sub_el->partition_name, 
                          name_to_check)) && sub_el != element)
          DBUG_RETURN(FALSE);
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
  while (el= (parts_it++))
  {
    if (! has_unique_name(el))
      DBUG_RETURN(el->partition_name);
      
    if (el->subpartitions.is_empty()) continue;
    List_iterator<partition_element> subparts_it(el->subpartitions);
    partition_element *subel;
    while (subel= (subparts_it++))
    {
      if (! has_unique_name(subel))
        DBUG_RETURN(subel->partition_name);
    }
  } 
  DBUG_RETURN(NULL);
}

#endif /* WITH_PARTITION_STORAGE_ENGINE */
