/* Copyright (C) 2005, 2006 MySQL AB

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
  This file is a container for general functionality related
  to partitioning introduced in MySQL version 5.1. It contains functionality
  used by all handlers that support partitioning, such as
  the partitioning handler itself and the NDB handler.

  The first version was written by Mikael Ronstrom.

  This version supports RANGE partitioning, LIST partitioning, HASH
  partitioning and composite partitioning (hereafter called subpartitioning)
  where each RANGE/LIST partitioning is HASH partitioned. The hash function
  can either be supplied by the user or by only a list of fields (also
  called KEY partitioning), where the MySQL server will use an internal
  hash function.
  There are quite a few defaults that can be used as well.
*/

/* Some general useful functions */

#define MYSQL_LEX 1
#include "mysql_priv.h"
#include <errno.h>
#include <m_ctype.h>
#include "md5.h"

#ifdef WITH_PARTITION_STORAGE_ENGINE
#include "ha_partition.h"
/*
  Partition related functions declarations and some static constants;
*/
const LEX_STRING partition_keywords[]=
{
  { C_STRING_WITH_LEN("HASH") },
  { C_STRING_WITH_LEN("RANGE") },
  { C_STRING_WITH_LEN("LIST") }, 
  { C_STRING_WITH_LEN("KEY") },
  { C_STRING_WITH_LEN("MAXVALUE") },
  { C_STRING_WITH_LEN("LINEAR ") }
};
static const char *part_str= "PARTITION";
static const char *sub_str= "SUB";
static const char *by_str= "BY";
static const char *space_str= " ";
static const char *equal_str= "=";
static const char *end_paren_str= ")";
static const char *begin_paren_str= "(";
static const char *comma_str= ",";

int get_partition_id_list(partition_info *part_info,
                           uint32 *part_id,
                           longlong *func_value);
int get_partition_id_range(partition_info *part_info,
                            uint32 *part_id,
                            longlong *func_value);
int get_partition_id_hash_nosub(partition_info *part_info,
                                 uint32 *part_id,
                                 longlong *func_value);
int get_partition_id_key_nosub(partition_info *part_info,
                                uint32 *part_id,
                                longlong *func_value);
int get_partition_id_linear_hash_nosub(partition_info *part_info,
                                        uint32 *part_id,
                                        longlong *func_value);
int get_partition_id_linear_key_nosub(partition_info *part_info,
                                       uint32 *part_id,
                                       longlong *func_value);
int get_partition_id_range_sub_hash(partition_info *part_info,
                                     uint32 *part_id,
                                     longlong *func_value);
int get_partition_id_range_sub_key(partition_info *part_info,
                                    uint32 *part_id,
                                    longlong *func_value);
int get_partition_id_range_sub_linear_hash(partition_info *part_info,
                                            uint32 *part_id,
                                            longlong *func_value);
int get_partition_id_range_sub_linear_key(partition_info *part_info,
                                           uint32 *part_id,
                                           longlong *func_value);
int get_partition_id_list_sub_hash(partition_info *part_info,
                                    uint32 *part_id,
                                    longlong *func_value);
int get_partition_id_list_sub_key(partition_info *part_info,
                                   uint32 *part_id,
                                   longlong *func_value);
int get_partition_id_list_sub_linear_hash(partition_info *part_info,
                                           uint32 *part_id,
                                           longlong *func_value);
int get_partition_id_list_sub_linear_key(partition_info *part_info,
                                          uint32 *part_id,
                                          longlong *func_value);
uint32 get_partition_id_hash_sub(partition_info *part_info); 
uint32 get_partition_id_key_sub(partition_info *part_info); 
uint32 get_partition_id_linear_hash_sub(partition_info *part_info); 
uint32 get_partition_id_linear_key_sub(partition_info *part_info); 
#endif

static uint32 get_next_partition_via_walking(PARTITION_ITERATOR*);
static uint32 get_next_subpartition_via_walking(PARTITION_ITERATOR*);
uint32 get_next_partition_id_range(PARTITION_ITERATOR* part_iter);
uint32 get_next_partition_id_list(PARTITION_ITERATOR* part_iter);
int get_part_iter_for_interval_via_mapping(partition_info *part_info,
                                           bool is_subpart,
                                           char *min_value, char *max_value,
                                           uint flags,
                                           PARTITION_ITERATOR *part_iter);
int get_part_iter_for_interval_via_walking(partition_info *part_info,
                                           bool is_subpart,
                                           char *min_value, char *max_value,
                                           uint flags,
                                           PARTITION_ITERATOR *part_iter);
static void set_up_range_analysis_info(partition_info *part_info);

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
    This needs to be outside of WITH_PARTITION_STORAGE_ENGINE since it is
    used from the sql parser that doesn't have any #ifdef's
*/

my_bool is_partition_management(LEX *lex)
{
  return (lex->sql_command == SQLCOM_ALTER_TABLE &&
          (lex->alter_info.flags == ALTER_ADD_PARTITION ||
           lex->alter_info.flags == ALTER_REORGANIZE_PARTITION));
}

#ifdef WITH_PARTITION_STORAGE_ENGINE
/*
  A support function to check if a name is in a list of strings

  SYNOPSIS
    is_name_in_list()
    name               String searched for
    list_names         A list of names searched in

  RETURN VALUES
    TRUE               String found
    FALSE              String not found
*/

bool is_name_in_list(char *name,
                          List<char> list_names)
{
  List_iterator<char> names_it(list_names);
  uint no_names= list_names.elements;
  uint i= 0;

  do
  {
    char *list_name= names_it++;
    if (!(my_strcasecmp(system_charset_info, name, list_name)))
      return TRUE;
  } while (++i < no_names);
  return FALSE;
}



/*
  Set-up defaults for partitions. 

  SYNOPSIS
    partition_default_handling()
    table                         Table object
    part_info                     Partition info to set up
    is_create_table_ind           Is this part of a table creation
    normalized_path               Normalized path name of table and database

  RETURN VALUES
    TRUE                          Error
    FALSE                         Success
*/

bool partition_default_handling(TABLE *table, partition_info *part_info,
                                bool is_create_table_ind,
                                const char *normalized_path)
{
  DBUG_ENTER("partition_default_handling");

  if (part_info->use_default_no_partitions)
  {
    if (!is_create_table_ind &&
        table->file->get_no_parts(normalized_path, &part_info->no_parts))
    {
      DBUG_RETURN(TRUE);
    }
  }
  else if (part_info->is_sub_partitioned() &&
           part_info->use_default_no_subpartitions)
  {
    uint no_parts;
    if (!is_create_table_ind &&
        (table->file->get_no_parts(normalized_path, &no_parts)))
    {
      DBUG_RETURN(TRUE);
    }
    DBUG_ASSERT(part_info->no_parts > 0);
    part_info->no_subparts= no_parts / part_info->no_parts;
    DBUG_ASSERT((no_parts % part_info->no_parts) == 0);
  }
  part_info->set_up_defaults_for_partitioning(table->file,
                                              (ulonglong)0, (uint)0);
  DBUG_RETURN(FALSE);
}


/*
  Check that the reorganized table will not have duplicate partitions.

  SYNOPSIS
    check_reorganise_list()
    new_part_info      New partition info
    old_part_info      Old partition info
    list_part_names    The list of partition names that will go away and
                       can be reused in the new table.

  RETURN VALUES
    TRUE               Inacceptable name conflict detected.
    FALSE              New names are OK.

  DESCRIPTION
    Can handle that the 'new_part_info' and 'old_part_info' the same
    in which case it checks that the list of names in the partitions
    doesn't contain any duplicated names.
*/

bool check_reorganise_list(partition_info *new_part_info,
                           partition_info *old_part_info,
                           List<char> list_part_names)
{
  uint new_count, old_count;
  uint no_new_parts= new_part_info->partitions.elements;
  uint no_old_parts= old_part_info->partitions.elements;
  List_iterator<partition_element> new_parts_it(new_part_info->partitions);
  bool same_part_info= (new_part_info == old_part_info);
  DBUG_ENTER("check_reorganise_list");

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
        if (!is_name_in_list(old_name, list_part_names))
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
    out:old_part_id         The returned partition id of old record 
    out:new_part_id         The returned partition id of new record

  RETURN VALUE
    0                       Success
    > 0                     Error code
*/

int get_parts_for_update(const byte *old_data, byte *new_data,
                         const byte *rec0, partition_info *part_info,
                         uint32 *old_part_id, uint32 *new_part_id,
                         longlong *new_func_value)
{
  Field **part_field_array= part_info->full_part_field_array;
  int error;
  longlong old_func_value;
  DBUG_ENTER("get_parts_for_update");

  DBUG_ASSERT(new_data == rec0);
  set_field_ptr(part_field_array, old_data, rec0);
  error= part_info->get_partition_id(part_info, old_part_id,
                                     &old_func_value);
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
    if (unlikely(error= part_info->get_partition_id(part_info,
                                                    new_part_id,
                                                    new_func_value)))
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
    error= part_info->get_partition_id(part_info, new_part_id,
                                       new_func_value);
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
    out:part_id             The returned partition id to delete from

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
  longlong func_value;
  DBUG_ENTER("get_part_for_delete");

  if (likely(buf == rec0))
  {
    if (unlikely((error= part_info->get_partition_id(part_info, part_id,
                                                     &func_value))))
    {
      DBUG_RETURN(error);
    }
    DBUG_PRINT("info", ("Delete from partition %d", *part_id));
  }
  else
  {
    Field **part_field_array= part_info->full_part_field_array;
    set_field_ptr(part_field_array, buf, rec0);
    error= part_info->get_partition_id(part_info, part_id, &func_value);
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
  This method is used to set-up both partition and subpartitioning
  field array and used for all types of partitioning.
  It is part of the logic around fix_partition_func.

  SYNOPSIS
    set_up_field_array()
    table                TABLE object for which partition fields are set-up
    sub_part             Is the table subpartitioned as well

  RETURN VALUE
    TRUE                 Error, some field didn't meet requirements
    FALSE                Ok, partition field array set-up

  DESCRIPTION

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
    1) All fields of partition function in Primary keys and unique indexes
       (if not supported)


    Create an array of partition fields (NULL terminated). Before this method
    is called fix_fields or find_table_in_sef has been called to set
    GET_FIXED_FIELDS_FLAG on all fields that are part of the partition
    function.
*/

static bool set_up_field_array(TABLE *table,
                              bool is_sub_part)
{
  Field **ptr, *field, **field_array;
  uint no_fields= 0;
  uint size_field_array;
  uint i= 0;
  partition_info *part_info= table->part_info;
  int result= FALSE;
  DBUG_ENTER("set_up_field_array");

  ptr= table->field;
  while ((field= *(ptr++))) 
  {
    if (field->flags & GET_FIXED_FIELDS_FLAG)
      no_fields++;
  }
  if (no_fields == 0)
  {
    /*
      We are using hidden key as partitioning field
    */
    DBUG_ASSERT(!is_sub_part);
    DBUG_RETURN(result);
  }
  size_field_array= (no_fields+1)*sizeof(Field*);
  field_array= (Field**)sql_alloc(size_field_array);
  if (unlikely(!field_array))
  {
    mem_alloc_error(size_field_array);
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
        */

        if (unlikely(field->flags & BLOB_FLAG))
        {
          my_error(ER_BLOB_FIELD_IN_PART_FUNC_ERROR, MYF(0));
          result= TRUE;
        }
      }
    }
  }
  field_array[no_fields]= 0;
  if (!is_sub_part)
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

  if (!part_info->is_sub_partitioned())
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
      mem_alloc_error(size_field_array);
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

  Clear flag GET_FIXED_FIELDS_FLAG in all fields of a key previously set by
  set_indicator_in_key_fields (always used in pairs).

  SYNOPSIS
    clear_indicator_in_key_fields()
    key_info                  Reference to find the key fields

  RETURN VALUE
    NONE

  DESCRIPTION
    These support routines is used to set/reset an indicator of all fields
    in a certain key. It is used in conjunction with another support routine
    that traverse all fields in the PF to find if all or some fields in the
    PF is part of the key. This is used to check primary keys and unique
    keys involve all fields in PF (unless supported) and to derive the
    key_map's used to quickly decide whether the index can be used to
    derive which partitions are needed to scan.
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

  RETURN VALUE
    NONE
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
    out:all_fields       Is all fields of partition field array used in key
    out:some_fields      Is some fields of partition field array used in key

  RETURN VALUE
    all_fields, some_fields
*/

static void check_fields_in_PF(Field **ptr, bool *all_fields,
                               bool *some_fields)
{
  DBUG_ENTER("check_fields_in_PF");

  *all_fields= TRUE;
  *some_fields= FALSE;
  if ((!ptr) || !(*ptr))
  {
    *all_fields= FALSE;
    DBUG_VOID_RETURN;
  }
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

  RETURN VALUE
    NONE
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
  find_field_in_table_sef finds the field given its name. All fields get
  GET_FIXED_FIELDS_FLAG set.

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
    This routine sets-up the partition field array for KEY partitioning, it
    also verifies that all fields in the list of fields is actually a part of
    the table.

*/


static bool handle_list_of_fields(List_iterator<char> it,
                                  TABLE *table,
                                  partition_info *part_info,
                                  bool is_sub_part)
{
  Field *field;
  bool result;
  char *field_name;
  bool is_list_empty= TRUE;
  DBUG_ENTER("handle_list_of_fields");

  while ((field_name= it++))
  {
    is_list_empty= FALSE;
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
  if (is_list_empty)
  {
    uint primary_key= table->s->primary_key;
    if (primary_key != MAX_KEY)
    {
      uint no_key_parts= table->key_info[primary_key].key_parts, i;
      /*
        In the case of an empty list we use primary key as partition key.
      */
      for (i= 0; i < no_key_parts; i++)
      {
        Field *field= table->key_info[primary_key].key_part[i].field;
        field->flags|= GET_FIXED_FIELDS_FLAG;
      }
    }
    else
    {
      if (table->s->db_type->partition_flags &&
          (table->s->db_type->partition_flags() & HA_USE_AUTO_PARTITION) &&
          (table->s->db_type->partition_flags() & HA_CAN_PARTITION))
      {
        /*
          This engine can handle automatic partitioning and there is no
          primary key. In this case we rely on that the engine handles
          partitioning based on a hidden key. Thus we allocate no
          array for partitioning fields.
        */
        DBUG_RETURN(FALSE);
      }
      else
      {
        my_error(ER_FIELD_NOT_FOUND_PART_ERROR, MYF(0));
        DBUG_RETURN(TRUE);
      }
    }
  }
  result= set_up_field_array(table, is_sub_part);
end:
  DBUG_RETURN(result);
}


/*
  Support function to check if all VALUES * (expression) is of the
  right sign (no signed constants when unsigned partition function)

  SYNOPSIS
    check_signed_flag()
    part_info                Partition info object

  RETURN VALUES
    0                        No errors due to sign errors
    >0                       Sign error
*/

int check_signed_flag(partition_info *part_info)
{
  int error= 0;
  uint i= 0;
  if (part_info->part_type != HASH_PARTITION &&
      part_info->part_expr->unsigned_flag)
  {
    List_iterator<partition_element> part_it(part_info->partitions);
    do
    {
      partition_element *part_elem= part_it++;

      if (part_elem->signed_flag)
      {
        my_error(ER_PARTITION_CONST_DOMAIN_ERROR, MYF(0));
        error= ER_PARTITION_CONST_DOMAIN_ERROR;
        break;
      }
    } while (++i < part_info->no_parts);
  }
  return error;
}


/*
  The function uses a new feature in fix_fields where the flag 
  GET_FIXED_FIELDS_FLAG is set for all fields in the item tree.
  This field must always be reset before returning from the function
  since it is used for other purposes as well.

  SYNOPSIS
    fix_fields_part_func()
    thd                  The thread object
    func_expr            The item tree reference of the partition function
    table                The table object
    part_info            Reference to partitioning data structure
    is_sub_part          Is the table subpartitioned as well
    is_field_to_be_setup Flag if we are to set-up field arrays

  RETURN VALUE
    TRUE                 An error occurred, something was wrong with the
                         partition function.
    FALSE                Ok, a partition field array was created

  DESCRIPTION
    This function is used to build an array of partition fields for the
    partitioning function and subpartitioning function. The partitioning
    function is an item tree that must reference at least one field in the
    table. This is checked first in the parser that the function doesn't
    contain non-cacheable parts (like a random function) and by checking
    here that the function isn't a constant function.

    Calculate the number of fields in the partition function.
    Use it allocate memory for array of Field pointers.
    Initialise array of field pointers. Use information set when
    calling fix_fields and reset it immediately after.
    The get_fields_in_item_tree activates setting of bit in flags
    on the field object.
*/

bool fix_fields_part_func(THD *thd, Item* func_expr, TABLE *table,
                          bool is_sub_part, bool is_field_to_be_setup)
{
  partition_info *part_info= table->part_info;
  uint dir_length, home_dir_length;
  bool result= TRUE;
  TABLE_LIST tables;
  TABLE_LIST *save_table_list, *save_first_table, *save_last_table;
  int error;
  Name_resolution_context *context;
  const char *save_where;
  char* db_name;
  char db_name_string[FN_REFLEN];
  DBUG_ENTER("fix_fields_part_func");

  if (part_info->fixed)
  {
    if (!(is_sub_part || (error= check_signed_flag(part_info))))
      result= FALSE;
    goto end;
  }

  /*
    Set-up the TABLE_LIST object to be a list with a single table
    Set the object to zero to create NULL pointers and set alias
    and real name to table name and get database name from file name.
  */

  bzero((void*)&tables, sizeof(TABLE_LIST));
  tables.alias= tables.table_name= (char*) table->s->table_name.str;
  tables.table= table;
  tables.next_local= 0;
  tables.next_name_resolution_table= 0;
  strmov(db_name_string, table->s->normalized_path.str);
  dir_length= dirname_length(db_name_string);
  db_name_string[dir_length - 1]= 0;
  home_dir_length= dirname_length(db_name_string);
  db_name= &db_name_string[home_dir_length];
  tables.db= db_name;

  context= thd->lex->current_context();
  table->map= 1; //To ensure correct calculation of const item
  table->get_fields_in_item_tree= TRUE;
  save_table_list= context->table_list;
  save_first_table= context->first_name_resolution_table;
  save_last_table= context->last_name_resolution_table;
  context->table_list= &tables;
  context->first_name_resolution_table= &tables;
  context->last_name_resolution_table= NULL;
  func_expr->walk(&Item::change_context_processor, 0, (byte*) context);
  save_where= thd->where;
  thd->where= "partition function";
  error= func_expr->fix_fields(thd, (Item**)0);
  context->table_list= save_table_list;
  context->first_name_resolution_table= save_first_table;
  context->last_name_resolution_table= save_last_table;
  if (unlikely(error))
  {
    DBUG_PRINT("info", ("Field in partition function not part of table"));
    if (is_field_to_be_setup)
      clear_field_flag(table);
    goto end;
  }
  thd->where= save_where;
  if (unlikely(func_expr->const_item()))
  {
    my_error(ER_CONST_EXPR_IN_PARTITION_FUNC_ERROR, MYF(0));
    clear_field_flag(table);
    goto end;
  }
  if ((!is_sub_part) && (error= check_signed_flag(part_info)))
    goto end;
  result= FALSE;
  if (is_field_to_be_setup)
    result= set_up_field_array(table, is_sub_part);
  if (!is_sub_part)
    part_info->fixed= TRUE;
end:
  table->get_fields_in_item_tree= FALSE;
  table->map= 0; //Restore old value
  DBUG_RETURN(result);
}


/*
  Check that the primary key contains all partition fields if defined

  SYNOPSIS
    check_primary_key()
    table                TABLE object for which partition fields are set-up

  RETURN VALUES
    TRUE                 Not all fields in partitioning function was part
                         of primary key
    FALSE                Ok, all fields of partitioning function were part
                         of primary key

  DESCRIPTION
    This function verifies that if there is a primary key that it contains
    all the fields of the partition function.
    This is a temporary limitation that will hopefully be removed after a
    while.
*/

static bool check_primary_key(TABLE *table)
{
  uint primary_key= table->s->primary_key;
  bool all_fields, some_fields;
  bool result= FALSE;
  DBUG_ENTER("check_primary_key");

  if (primary_key < MAX_KEY)
  {
    set_indicator_in_key_fields(table->key_info+primary_key);
    check_fields_in_PF(table->part_info->full_part_field_array,
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
  Check that unique keys contains all partition fields

  SYNOPSIS
    check_unique_keys()
    table                TABLE object for which partition fields are set-up

  RETURN VALUES
    TRUE                 Not all fields in partitioning function was part
                         of all unique keys
    FALSE                Ok, all fields of partitioning function were part
                         of unique keys

  DESCRIPTION
    This function verifies that if there is a unique index that it contains
    all the fields of the partition function.
    This is a temporary limitation that will hopefully be removed after a
    while.
*/

static bool check_unique_keys(TABLE *table)
{
  bool all_fields, some_fields;
  bool result= FALSE;
  uint keys= table->s->keys;
  uint i;
  DBUG_ENTER("check_unique_keys");

  for (i= 0; i < keys; i++)
  {
    if (table->key_info[i].flags & HA_NOSAME) //Unique index
    {
      set_indicator_in_key_fields(table->key_info+i);
      check_fields_in_PF(table->part_info->full_part_field_array,
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
  Set up partition bitmap

  SYNOPSIS
    set_up_partition_bitmap()
    thd                  Thread object
    part_info            Reference to partitioning data structure

  RETURN VALUE
    TRUE                 Memory allocation failure
    FALSE                Success

  DESCRIPTION
    Allocate memory for bitmap of the partitioned table
    and initialise it.
*/

static bool set_up_partition_bitmap(THD *thd, partition_info *part_info)
{
  uint32 *bitmap_buf;
  uint bitmap_bits= part_info->no_subparts? 
                     (part_info->no_subparts* part_info->no_parts):
                      part_info->no_parts;
  uint bitmap_bytes= bitmap_buffer_size(bitmap_bits);
  DBUG_ENTER("set_up_partition_bitmap");

  if (!(bitmap_buf= (uint32*)thd->alloc(bitmap_bytes)))
  {
    mem_alloc_error(bitmap_bytes);
    DBUG_RETURN(TRUE);
  }
  bitmap_init(&part_info->used_partitions, bitmap_buf, bitmap_bytes*8, FALSE);
  bitmap_set_all(&part_info->used_partitions);
  DBUG_RETURN(FALSE);
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
  uint keys= table->s->keys;
  uint i;
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
    if (part_info->is_sub_partitioned())
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
  Set up function pointers for partition function

  SYNOPSIS
    set_up_partition_func_pointers()
    part_info            Reference to partitioning data structure

  RETURN VALUE
    NONE

  DESCRIPTION
    Set-up all function pointers for calculation of partition id,
    subpartition id and the upper part in subpartitioning. This is to speed up
    execution of get_partition_id which is executed once every record to be
    written and deleted and twice for updates.
*/

static void set_up_partition_func_pointers(partition_info *part_info)
{
  DBUG_ENTER("set_up_partition_func_pointers");

  if (part_info->is_sub_partitioned())
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
    else /* LIST Partitioning */
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
  else /* No subpartitioning */
  {
    part_info->get_part_partition_id= NULL;
    part_info->get_subpartition_id= NULL;
    if (part_info->part_type == RANGE_PARTITION)
      part_info->get_partition_id= get_partition_id_range;
    else if (part_info->part_type == LIST_PARTITION)
      part_info->get_partition_id= get_partition_id_list;
    else /* HASH partitioning */
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
  DBUG_VOID_RETURN;
}


/*
  For linear hashing we need a mask which is on the form 2**n - 1 where
  2**n >= no_parts. Thus if no_parts is 6 then mask is 2**3 - 1 = 8 - 1 = 7.

  SYNOPSIS
    set_linear_hash_mask()
    part_info            Reference to partitioning data structure
    no_parts             Number of parts in linear hash partitioning

  RETURN VALUE
    NONE
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
    part_id= (uint32)(hash_value & new_mask);
  }
  return part_id;
}

/*
  fix partition functions

  SYNOPSIS
    fix_partition_func()
    thd                  The thread object
    table                TABLE object for which partition fields are set-up
    is_create_table_ind  Indicator of whether openfrm was called as part of
                         CREATE or ALTER TABLE

  RETURN VALUE
    TRUE                 Error
    FALSE                Success

  DESCRIPTION
    The name parameter contains the full table name and is used to get the
    database name of the table which is used to set-up a correct
    TABLE_LIST object for use in fix_fields.

NOTES
    This function is called as part of opening the table by opening the .frm
    file. It is a part of CREATE TABLE to do this so it is quite permissible
    that errors due to erroneus syntax isn't found until we come here.
    If the user has used a non-existing field in the table is one such example
    of an error that is not discovered until here.
*/

bool fix_partition_func(THD *thd, TABLE *table,
                        bool is_create_table_ind)
{
  bool result= TRUE;
  partition_info *part_info= table->part_info;
  enum_mark_columns save_mark_used_columns= thd->mark_used_columns;
  Item *thd_free_list= thd->free_list;
  DBUG_ENTER("fix_partition_func");

  if (part_info->fixed)
  {
    DBUG_RETURN(FALSE);
  }
  thd->mark_used_columns= MARK_COLUMNS_NONE;
  DBUG_PRINT("info", ("thd->mark_used_columns: %d", thd->mark_used_columns));

  if (!is_create_table_ind ||
       thd->lex->sql_command != SQLCOM_CREATE_TABLE)
  {
    if (partition_default_handling(table, part_info,
                                   is_create_table_ind,
                                   table->s->normalized_path.str))
    {
      DBUG_RETURN(TRUE);
    }
  }
  thd->free_list= part_info->item_free_list;
  if (part_info->is_sub_partitioned())
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
      if (unlikely(fix_fields_part_func(thd, part_info->subpart_expr,
                                        table, TRUE, TRUE)))
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
      if (unlikely(fix_fields_part_func(thd, part_info->part_expr,
                                        table, FALSE, TRUE)))
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
    const char *error_str;
    if (unlikely(fix_fields_part_func(thd, part_info->part_expr,
                                      table, FALSE, TRUE)))
      goto end;
    if (part_info->part_type == RANGE_PARTITION)
    {
      error_str= partition_keywords[PKW_RANGE].str; 
      if (unlikely(part_info->check_range_constants()))
        goto end;
    }
    else if (part_info->part_type == LIST_PARTITION)
    {
      error_str= partition_keywords[PKW_LIST].str; 
      if (unlikely(part_info->check_list_constants()))
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
  if (unlikely((!(table->s->db_type->partition_flags &&
      (table->s->db_type->partition_flags() & HA_CAN_PARTITION_UNIQUE))) &&
               check_unique_keys(table)))
    goto end;
  if (unlikely(set_up_partition_bitmap(thd, part_info)))
    goto end;
  check_range_capable_PF(table);
  set_up_partition_key_maps(table, part_info);
  set_up_partition_func_pointers(part_info);
  set_up_range_analysis_info(part_info);
  result= FALSE;
end:
  thd->free_list= thd_free_list;
  thd->mark_used_columns= save_mark_used_columns;
  DBUG_PRINT("info", ("thd->mark_used_columns: %d", thd->mark_used_columns));
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
  uint len_written= my_write(fptr, (const byte*)buf, len, MYF(0));

  if (likely(len == len_written))
    return 0;
  else
    return 1;
}

static int add_string_object(File fptr, String *string)
{
  return add_write(fptr, string->ptr(), string->length());
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
  return add_part_key_word(fptr, partition_keywords[PKW_HASH].str);
}

static int add_partition(File fptr)
{
  char buff[22];
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
  char buff[22];
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
  err= add_part_key_word(fptr, partition_keywords[PKW_KEY].str);
  no_fields= field_list.elements;
  i= 0;
  while (i < no_fields)
  {
    const char *field_str= part_it++;
    String field_string("", 0, system_charset_info);
    THD *thd= current_thd;
    ulonglong save_options= thd->options;
    thd->options= 0;
    append_identifier(thd, &field_string, field_str,
                      strlen(field_str));
    thd->options= save_options;
    err+= add_string_object(fptr, &field_string);
    if (i != (no_fields-1))
      err+= add_comma(fptr);
    i++;
  }
  return err;
}

static int add_name_string(File fptr, const char *name)
{
  int err;
  String name_string("", 0, system_charset_info);
  THD *thd= current_thd;
  ulonglong save_options= thd->options;

  thd->options= 0;
  append_identifier(thd, &name_string, name,
                    strlen(name));
  thd->options= save_options;
  err= add_string_object(fptr, &name_string);
  return err;
}

static int add_int(File fptr, longlong number)
{
  char buff[32];
  llstr(number, buff);
  return add_string(fptr, buff);
}

static int add_uint(File fptr, ulonglong number)
{
  char buff[32];
  longlong2str(number, buff, 10);
  return add_string(fptr, buff);
}

static int add_keyword_string(File fptr, const char *keyword,
                              bool should_use_quotes, 
                              const char *keystr)
{
  int err= add_string(fptr, keyword);

  err+= add_space(fptr);
  err+= add_equal(fptr);
  err+= add_space(fptr);
  if (should_use_quotes)
    err+= add_string(fptr, "'");
  err+= add_string(fptr, keystr);
  if (should_use_quotes)
    err+= add_string(fptr, "'");
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

static int add_engine(File fptr, handlerton *engine_type)
{
  const char *engine_str= hton2plugin[engine_type->slot]->name.str;
  DBUG_PRINT("info", ("ENGINE: %s", engine_str));
  int err= add_string(fptr, "ENGINE = ");
  return err + add_string(fptr, engine_str);
}

static int add_partition_options(File fptr, partition_element *p_elem)
{
  int err= 0;

  err+= add_space(fptr);
  if (p_elem->tablespace_name)
    err+= add_keyword_string(fptr,"TABLESPACE", FALSE,
                             p_elem->tablespace_name);
  if (p_elem->nodegroup_id != UNDEF_NODEGROUP)
    err+= add_keyword_int(fptr,"NODEGROUP",(longlong)p_elem->nodegroup_id);
  if (p_elem->part_max_rows)
    err+= add_keyword_int(fptr,"MAX_ROWS",(longlong)p_elem->part_max_rows);
  if (p_elem->part_min_rows)
    err+= add_keyword_int(fptr,"MIN_ROWS",(longlong)p_elem->part_min_rows);
  if (p_elem->data_file_name)
    err+= add_keyword_string(fptr, "DATA DIRECTORY", TRUE, 
                             p_elem->data_file_name);
  if (p_elem->index_file_name)
    err+= add_keyword_string(fptr, "INDEX DIRECTORY", TRUE, 
                             p_elem->index_file_name);
  if (p_elem->part_comment)
    err+= add_keyword_string(fptr, "COMMENT", TRUE, p_elem->part_comment);
  return err + add_engine(fptr,p_elem->engine_type);
}

static int add_partition_values(File fptr, partition_info *part_info, partition_element *p_elem)
{
  int err= 0;

  if (part_info->part_type == RANGE_PARTITION)
  {
    err+= add_string(fptr, " VALUES LESS THAN ");
    if (!p_elem->max_value)
    {
      err+= add_begin_parenthesis(fptr);
      if (p_elem->signed_flag)
        err+= add_int(fptr, p_elem->range_value);
      else
        err+= add_uint(fptr, p_elem->range_value);
      err+= add_end_parenthesis(fptr);
    }
    else
      err+= add_string(fptr, partition_keywords[PKW_MAXVALUE].str);
  }
  else if (part_info->part_type == LIST_PARTITION)
  {
    uint i;
    List_iterator<part_elem_value> list_val_it(p_elem->list_val_list);
    err+= add_string(fptr, " VALUES IN ");
    uint no_items= p_elem->list_val_list.elements;

    err+= add_begin_parenthesis(fptr);
    if (p_elem->has_null_value)
    {
      err+= add_string(fptr, "NULL");
      if (no_items == 0)
      {
        err+= add_end_parenthesis(fptr);
        goto end;
      }
      err+= add_comma(fptr);
    }
    i= 0;
    do
    {
      part_elem_value *list_value= list_val_it++;

      if (!list_value->unsigned_flag)
        err+= add_int(fptr, list_value->value);
      else
        err+= add_uint(fptr, list_value->value);
      if (i != (no_items-1))
        err+= add_comma(fptr);
    } while (++i < no_items);
    err+= add_end_parenthesis(fptr);
  }
end:
  return err;
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
    show_partition_options     Should we display partition options

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
                                bool use_sql_alloc,
                                bool show_partition_options)
{
  uint i,j, tot_no_parts, no_subparts, no_parts;
  partition_element *part_elem;
  partition_element *save_part_elem= NULL;
  ulonglong buffer_length;
  char path[FN_REFLEN];
  int err= 0;
  List_iterator<partition_element> part_it(part_info->partitions);
  File fptr;
  char *buf= NULL; //Return buffer
  DBUG_ENTER("generate_partition_syntax");

  if (unlikely(((fptr= create_temp_file(path,mysql_tmpdir,"psy", 
                                        O_RDWR | O_BINARY | O_TRUNC |  
                                        O_TEMPORARY, MYF(MY_WME)))) < 0))
    DBUG_RETURN(NULL);
#ifndef __WIN__
  unlink(path);
#endif
  err+= add_space(fptr);
  err+= add_partition_by(fptr);
  switch (part_info->part_type)
  {
    case RANGE_PARTITION:
      err+= add_part_key_word(fptr, partition_keywords[PKW_RANGE].str);
      break;
    case LIST_PARTITION:
      err+= add_part_key_word(fptr, partition_keywords[PKW_LIST].str);
      break;
    case HASH_PARTITION:
      if (part_info->linear_hash_ind)
        err+= add_string(fptr, partition_keywords[PKW_LINEAR].str);
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
  if ((!part_info->use_default_no_partitions) &&
       part_info->use_default_partitions)
  {
    err+= add_string(fptr, "PARTITIONS ");
    err+= add_int(fptr, part_info->no_parts);
    err+= add_space(fptr);
  }
  if (part_info->is_sub_partitioned())
  {
    err+= add_subpartition_by(fptr);
    /* Must be hash partitioning for subpartitioning */
    if (part_info->linear_hash_ind)
      err+= add_string(fptr, partition_keywords[PKW_LINEAR].str);
    if (part_info->list_of_subpart_fields)
      err+= add_key_partition(fptr, part_info->subpart_field_list);
    else
      err+= add_hash(fptr);
    if (part_info->subpart_expr)
      err+= add_string_len(fptr, part_info->subpart_func_string,
                           part_info->subpart_func_len);
    err+= add_end_parenthesis(fptr);
    err+= add_space(fptr);
    if ((!part_info->use_default_no_subpartitions) && 
          part_info->use_default_subpartitions)
    {
      err+= add_string(fptr, "SUBPARTITIONS ");
      err+= add_int(fptr, part_info->no_subparts);
      err+= add_space(fptr);
    }
  }
  tot_no_parts= part_info->partitions.elements;
  no_subparts= part_info->no_subparts;

  if (!part_info->use_default_partitions)
  {
    bool first= TRUE;
    err+= add_begin_parenthesis(fptr);
    i= 0;
    do
    {
      part_elem= part_it++;
      if (part_elem->part_state != PART_TO_BE_DROPPED &&
          part_elem->part_state != PART_REORGED_DROPPED)
      {
        if (!first)
        {
          err+= add_comma(fptr);
          err+= add_space(fptr);
        }
        first= FALSE;
        err+= add_partition(fptr);
        err+= add_name_string(fptr, part_elem->partition_name);
        err+= add_partition_values(fptr, part_info, part_elem);
        if (!part_info->is_sub_partitioned() ||
            part_info->use_default_subpartitions)
        {
          if (show_partition_options)
            err+= add_partition_options(fptr, part_elem);
        }
        else
        {
          err+= add_space(fptr);
          err+= add_begin_parenthesis(fptr);
          List_iterator<partition_element> sub_it(part_elem->subpartitions);
          j= 0;
          do
          {
            part_elem= sub_it++;
            err+= add_subpartition(fptr);
            err+= add_name_string(fptr, part_elem->partition_name);
            if (show_partition_options)
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
      }
      if (i == (tot_no_parts-1))
        err+= add_end_parenthesis(fptr);
    } while (++i < tot_no_parts);
  }
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

  if (unlikely(my_read(fptr, (byte*)buf, *buf_length, MYF(MY_FNABP))))
  {
    if (!use_sql_alloc)
      my_free(buf, MYF(0));
    else
      buf= NULL;
  }
  else
    buf[*buf_length]= 0;

close_file:
  my_close(fptr, MYF(0));
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
  partition_info *part_info= table->part_info;
  Item_field *item_field;
  DBUG_ENTER("partition_key_modified");

  if (!part_info)
    DBUG_RETURN(FALSE);
  if (table->s->db_type->partition_flags &&
      (table->s->db_type->partition_flags() & HA_CAN_UPDATE_PARTITION_KEY))
    DBUG_RETURN(FALSE);
  f.rewind();
  while ((item_field=(Item_field*) f++))
    if (item_field->field->flags & FIELD_IN_PART_FUNC_FLAG)
      DBUG_RETURN(TRUE);
  DBUG_RETURN(FALSE);
}


/*
  A function to handle correct handling of NULL values in partition
  functions.
  SYNOPSIS
    part_val_int()
    item_expr                 The item expression to evaluate
  RETURN VALUES
    The value of the partition function, LONGLONG_MIN if any null value
    in function
*/

static inline longlong part_val_int(Item *item_expr)
{
  longlong value= item_expr->val_int();
  if (item_expr->null_value)
    value= LONGLONG_MIN;
  return value;
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
  ulong nr1= 1;
  ulong nr2= 4;

  do
  {
    Field *field= *field_array;
    field->hash(&nr1, &nr2);
  } while (*(++field_array));
  return (uint32) nr1;
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
    out:func_value      Value of hash function

  RETURN VALUE
    Calculated partition id
*/

inline
static uint32 get_part_id_hash(uint no_parts,
                               Item *part_expr,
                               longlong *func_value)
{
  DBUG_ENTER("get_part_id_hash");
  *func_value= part_val_int(part_expr);
  longlong int_hash_id= *func_value % no_parts;
  DBUG_RETURN(int_hash_id < 0 ? -int_hash_id : int_hash_id);
}


/*
  Calculate part_id for (SUB)PARTITION BY LINEAR HASH

  SYNOPSIS
    get_part_id_linear_hash()
    part_info           A reference to the partition_info struct where all the
                        desired information is given
    no_parts            Number of hash partitions
    part_expr           Item tree of hash function
    out:func_value      Value of hash function

  RETURN VALUE
    Calculated partition id
*/

inline
static uint32 get_part_id_linear_hash(partition_info *part_info,
                                      uint no_parts,
                                      Item *part_expr,
                                      longlong *func_value)
{
  DBUG_ENTER("get_part_id_linear_hash");

  *func_value= part_val_int(part_expr);
  DBUG_RETURN(get_part_id_from_linear_hash(*func_value,
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
                              uint no_parts,
                              longlong *func_value)
{
  DBUG_ENTER("get_part_id_key");
  *func_value= calculate_key_value(field_array);
  DBUG_RETURN(*func_value % no_parts);
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
                                     uint no_parts,
                                     longlong *func_value)
{
  DBUG_ENTER("get_partition_id_linear_key");

  *func_value= calculate_key_value(field_array);
  DBUG_RETURN(get_part_id_from_linear_hash(*func_value,
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
    out:part_id         The partition id is returned through this pointer

  RETURN VALUE
    part_id                     Partition id of partition that would contain
                                row with given values of PF-fields
    HA_ERR_NO_PARTITION_FOUND   The fields of the partition function didn't
                                fit into any partition and thus the values of 
                                the PF-fields are not allowed.

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
    out:part_id         The partition id is returned through this pointer

  RETURN VALUE
    part_id                     Partition id of partition that would contain
                                row with given values of PF-fields
    HA_ERR_NO_PARTITION_FOUND   The fields of the partition function didn't
                                fit into any partition and thus the values of 
                                the PF-fields are not allowed.

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


int get_partition_id_list(partition_info *part_info,
                          uint32 *part_id,
                          longlong *func_value)
{
  LIST_PART_ENTRY *list_array= part_info->list_array;
  int list_index;
  int min_list_index= 0;
  int max_list_index= part_info->no_list_values - 1;
  longlong part_func_value= part_val_int(part_info->part_expr);
  longlong list_value;
  bool unsigned_flag= part_info->part_expr->unsigned_flag;
  DBUG_ENTER("get_partition_id_list");

  if (part_info->part_expr->null_value)
  {
    if (part_info->has_null_value)
    {
      *part_id= part_info->has_null_part_id;
      DBUG_RETURN(0);
    }
    goto notfound;
  }
  *func_value= part_func_value;
  if (unsigned_flag)
    part_func_value-= 0x8000000000000000ULL;
  while (max_list_index >= min_list_index)
  {
    list_index= (max_list_index + min_list_index) >> 1;
    list_value= list_array[list_index].list_value;
    if (list_value < part_func_value)
      min_list_index= list_index + 1;
    else if (list_value > part_func_value)
    {
      if (!list_index)
        goto notfound;
      max_list_index= list_index - 1;
    }
    else
    {
      *part_id= (uint32)list_array[list_index].partition_id;
      DBUG_RETURN(0);
    }
  }
notfound:
  *part_id= 0;
  DBUG_RETURN(HA_ERR_NO_PARTITION_FOUND);
}


/*
  Find the sub-array part_info->list_array that corresponds to given interval

  SYNOPSIS 
    get_list_array_idx_for_endpoint()
      part_info         Partitioning info (partitioning type must be LIST)
      left_endpoint     TRUE  - the interval is [a; +inf) or (a; +inf)
                        FALSE - the interval is (-inf; a] or (-inf; a)
      include_endpoint  TRUE iff the interval includes the endpoint

  DESCRIPTION
    This function finds the sub-array of part_info->list_array where values of
    list_array[idx].list_value are contained within the specifed interval.
    list_array is ordered by list_value, so
    1. For [a; +inf) or (a; +inf)-type intervals (left_endpoint==TRUE), the 
       sought sub-array starts at some index idx and continues till array end.
       The function returns first number idx, such that 
       list_array[idx].list_value is contained within the passed interval.
       
    2. For (-inf; a] or (-inf; a)-type intervals (left_endpoint==FALSE), the
       sought sub-array starts at array start and continues till some last 
       index idx.
       The function returns first number idx, such that 
       list_array[idx].list_value is NOT contained within the passed interval.
       If all array elements are contained, part_info->no_list_values is
       returned.

  NOTE
    The caller will call this function and then will run along the sub-array of
    list_array to collect partition ids. If the number of list values is 
    significantly higher then number of partitions, this could be slow and
    we could invent some other approach. The "run over list array" part is
    already wrapped in a get_next()-like function.

  RETURN
    The edge of corresponding sub-array of part_info->list_array
*/

uint32 get_list_array_idx_for_endpoint(partition_info *part_info,
                                       bool left_endpoint,
                                       bool include_endpoint)
{
  LIST_PART_ENTRY *list_array= part_info->list_array;
  uint list_index;
  uint min_list_index= 0, max_list_index= part_info->no_list_values - 1;
  longlong list_value;
  /* Get the partitioning function value for the endpoint */
  longlong part_func_value= part_val_int(part_info->part_expr);
  bool unsigned_flag= part_info->part_expr->unsigned_flag;
  DBUG_ENTER("get_list_array_idx_for_endpoint");

  if (part_info->part_expr->null_value)
  {
    DBUG_RETURN(0);
  }
  if (unsigned_flag)
    part_func_value-= 0x8000000000000000ULL;
  DBUG_ASSERT(part_info->no_list_values);
  do
  {
    list_index= (max_list_index + min_list_index) >> 1;
    list_value= list_array[list_index].list_value;
    if (list_value < part_func_value)
      min_list_index= list_index + 1;
    else if (list_value > part_func_value)
    {
      if (!list_index)
        goto notfound;
      max_list_index= list_index - 1;
    }
    else 
    {
      DBUG_RETURN(list_index + test(left_endpoint ^ include_endpoint));
    }
  } while (max_list_index >= min_list_index);
notfound:
  if (list_value < part_func_value)
    list_index++;
  DBUG_RETURN(list_index);
}


int get_partition_id_range(partition_info *part_info,
                            uint32 *part_id,
                            longlong *func_value)
{
  longlong *range_array= part_info->range_int_array;
  uint max_partition= part_info->no_parts - 1;
  uint min_part_id= 0;
  uint max_part_id= max_partition;
  uint loc_part_id;
  longlong part_func_value= part_val_int(part_info->part_expr);
  bool unsigned_flag= part_info->part_expr->unsigned_flag;
  DBUG_ENTER("get_partition_id_range");

  if (part_info->part_expr->null_value)
  {
    *part_id= 0;
    DBUG_RETURN(0);
  }
  *func_value= part_func_value;
  if (unsigned_flag)
    part_func_value-= 0x8000000000000000ULL;
  while (max_part_id > min_part_id)
  {
    loc_part_id= (max_part_id + min_part_id + 1) >> 1;
    if (range_array[loc_part_id] <= part_func_value)
      min_part_id= loc_part_id + 1;
    else
      max_part_id= loc_part_id - 1;
  }
  loc_part_id= max_part_id;
  if (part_func_value >= range_array[loc_part_id])
    if (loc_part_id != max_partition)
      loc_part_id++;
  *part_id= (uint32)loc_part_id;
  if (loc_part_id == max_partition &&
      range_array[loc_part_id] != LONGLONG_MAX &&
      part_func_value >= range_array[loc_part_id])
    DBUG_RETURN(HA_ERR_NO_PARTITION_FOUND);

  DBUG_PRINT("exit",("partition: %d", *part_id));
  DBUG_RETURN(0);
}


/*
  Find the sub-array of part_info->range_int_array that covers given interval
 
  SYNOPSIS 
    get_partition_id_range_for_endpoint()
      part_info         Partitioning info (partitioning type must be RANGE)
      left_endpoint     TRUE  - the interval is [a; +inf) or (a; +inf)
                        FALSE - the interval is (-inf; a] or (-inf; a).
      include_endpoint  TRUE <=> the endpoint itself is included in the
                        interval

  DESCRIPTION
    This function finds the sub-array of part_info->range_int_array where the
    elements have non-empty intersections with the given interval.
 
    A range_int_array element at index idx represents the interval
      
      [range_int_array[idx-1], range_int_array[idx]),

    intervals are disjoint and ordered by their right bound, so
    
    1. For [a; +inf) or (a; +inf)-type intervals (left_endpoint==TRUE), the
       sought sub-array starts at some index idx and continues till array end.
       The function returns first number idx, such that the interval
       represented by range_int_array[idx] has non empty intersection with 
       the passed interval.
       
    2. For (-inf; a] or (-inf; a)-type intervals (left_endpoint==FALSE), the
       sought sub-array starts at array start and continues till some last
       index idx.
       The function returns first number idx, such that the interval
       represented by range_int_array[idx] has EMPTY intersection with the
       passed interval.
       If the interval represented by the last array element has non-empty 
       intersection with the passed interval, part_info->no_parts is
       returned.
       
  RETURN
    The edge of corresponding part_info->range_int_array sub-array.
*/

uint32 get_partition_id_range_for_endpoint(partition_info *part_info,
                                           bool left_endpoint,
                                           bool include_endpoint)
{
  longlong *range_array= part_info->range_int_array;
  uint max_partition= part_info->no_parts - 1;
  uint min_part_id= 0, max_part_id= max_partition, loc_part_id;
  /* Get the partitioning function value for the endpoint */
  longlong part_func_value= part_val_int(part_info->part_expr);
  bool unsigned_flag= part_info->part_expr->unsigned_flag;
  DBUG_ENTER("get_partition_id_range_for_endpoint");

  if (part_info->part_expr->null_value)
  {
    uint32 ret_part_id= 0;
    if (!left_endpoint && include_endpoint)
      ret_part_id= 1;
    DBUG_RETURN(ret_part_id);
  }
  if (unsigned_flag)
    part_func_value-= 0x8000000000000000ULL;
  while (max_part_id > min_part_id)
  {
    loc_part_id= (max_part_id + min_part_id + 1) >> 1;
    if (range_array[loc_part_id] <= part_func_value)
      min_part_id= loc_part_id + 1;
    else
      max_part_id= loc_part_id - 1;
  }
  loc_part_id= max_part_id;
  if (loc_part_id < max_partition && 
      part_func_value >= range_array[loc_part_id+1])
  {
   loc_part_id++;
  }
  if (left_endpoint)
  {
    if (part_func_value >= range_array[loc_part_id])
      loc_part_id++;
  }
  else 
  {
    if (part_func_value == range_array[loc_part_id])
      loc_part_id += test(include_endpoint);
    else if (part_func_value > range_array[loc_part_id])
      loc_part_id++;
    loc_part_id++;
  }
  DBUG_RETURN(loc_part_id);
}


int get_partition_id_hash_nosub(partition_info *part_info,
                                 uint32 *part_id,
                                 longlong *func_value)
{
  *part_id= get_part_id_hash(part_info->no_parts, part_info->part_expr,
                             func_value);
  return 0;
}


int get_partition_id_linear_hash_nosub(partition_info *part_info,
                                        uint32 *part_id,
                                        longlong *func_value)
{
  *part_id= get_part_id_linear_hash(part_info, part_info->no_parts,
                                    part_info->part_expr, func_value);
  return 0;
}


int get_partition_id_key_nosub(partition_info *part_info,
                                uint32 *part_id,
                                longlong *func_value)
{
  *part_id= get_part_id_key(part_info->part_field_array,
                            part_info->no_parts, func_value);
  return 0;
}


int get_partition_id_linear_key_nosub(partition_info *part_info,
                                       uint32 *part_id,
                                       longlong *func_value)
{
  *part_id= get_part_id_linear_key(part_info,
                                   part_info->part_field_array,
                                   part_info->no_parts, func_value);
  return 0;
}


int get_partition_id_range_sub_hash(partition_info *part_info,
                                     uint32 *part_id,
                                     longlong *func_value)
{
  uint32 loc_part_id, sub_part_id;
  uint no_subparts;
  longlong local_func_value;
  int error;
  DBUG_ENTER("get_partition_id_range_sub_hash");

  if (unlikely((error= get_partition_id_range(part_info, &loc_part_id,
                                              func_value))))
  {
    DBUG_RETURN(error);
  }
  no_subparts= part_info->no_subparts;
  sub_part_id= get_part_id_hash(no_subparts, part_info->subpart_expr,
                                &local_func_value);
  *part_id= get_part_id_for_sub(loc_part_id, sub_part_id, no_subparts);
  DBUG_RETURN(0);
}


int get_partition_id_range_sub_linear_hash(partition_info *part_info,
                                            uint32 *part_id,
                                            longlong *func_value)
{
  uint32 loc_part_id, sub_part_id;
  uint no_subparts;
  longlong local_func_value;
  int error;
  DBUG_ENTER("get_partition_id_range_sub_linear_hash");

  if (unlikely((error= get_partition_id_range(part_info, &loc_part_id,
                                              func_value))))
  {
    DBUG_RETURN(error);
  }
  no_subparts= part_info->no_subparts;
  sub_part_id= get_part_id_linear_hash(part_info, no_subparts,
                                       part_info->subpart_expr,
                                       &local_func_value);
  *part_id= get_part_id_for_sub(loc_part_id, sub_part_id, no_subparts);
  DBUG_RETURN(0);
}


int get_partition_id_range_sub_key(partition_info *part_info,
                                    uint32 *part_id,
                                    longlong *func_value)
{
  uint32 loc_part_id, sub_part_id;
  uint no_subparts;
  longlong local_func_value;
  int error;
  DBUG_ENTER("get_partition_id_range_sub_key");

  if (unlikely((error= get_partition_id_range(part_info, &loc_part_id,
                                              func_value))))
  {
    DBUG_RETURN(error);
  }
  no_subparts= part_info->no_subparts;
  sub_part_id= get_part_id_key(part_info->subpart_field_array,
                               no_subparts, &local_func_value);
  *part_id= get_part_id_for_sub(loc_part_id, sub_part_id, no_subparts);
  DBUG_RETURN(0);
}


int get_partition_id_range_sub_linear_key(partition_info *part_info,
                                           uint32 *part_id,
                                           longlong *func_value)
{
  uint32 loc_part_id, sub_part_id;
  uint no_subparts;
  longlong local_func_value;
  int error;
  DBUG_ENTER("get_partition_id_range_sub_linear_key");

  if (unlikely((error= get_partition_id_range(part_info, &loc_part_id,
                                              func_value))))
  {
    DBUG_RETURN(error);
  }
  no_subparts= part_info->no_subparts;
  sub_part_id= get_part_id_linear_key(part_info,
                                      part_info->subpart_field_array,
                                      no_subparts, &local_func_value);
  *part_id= get_part_id_for_sub(loc_part_id, sub_part_id, no_subparts);
  DBUG_RETURN(0);
}


int get_partition_id_list_sub_hash(partition_info *part_info,
                                    uint32 *part_id,
                                    longlong *func_value)
{
  uint32 loc_part_id, sub_part_id;
  uint no_subparts;
  longlong local_func_value;
  int error;
  DBUG_ENTER("get_partition_id_list_sub_hash");

  if (unlikely((error= get_partition_id_list(part_info, &loc_part_id,
                                             func_value))))
  {
    DBUG_RETURN(error);
  }
  no_subparts= part_info->no_subparts;
  sub_part_id= get_part_id_hash(no_subparts, part_info->subpart_expr,
                                &local_func_value);
  *part_id= get_part_id_for_sub(loc_part_id, sub_part_id, no_subparts);
  DBUG_RETURN(0);
}


int get_partition_id_list_sub_linear_hash(partition_info *part_info,
                                           uint32 *part_id,
                                           longlong *func_value)
{
  uint32 loc_part_id, sub_part_id;
  uint no_subparts;
  longlong local_func_value;
  int error;
  DBUG_ENTER("get_partition_id_list_sub_linear_hash");

  if (unlikely((error= get_partition_id_list(part_info, &loc_part_id,
                                             func_value))))
  {
    DBUG_RETURN(error);
  }
  no_subparts= part_info->no_subparts;
  sub_part_id= get_part_id_linear_hash(part_info, no_subparts,
                                       part_info->subpart_expr,
                                       &local_func_value);
  *part_id= get_part_id_for_sub(loc_part_id, sub_part_id, no_subparts);
  DBUG_RETURN(0);
}


int get_partition_id_list_sub_key(partition_info *part_info,
                                   uint32 *part_id,
                                   longlong *func_value)
{
  uint32 loc_part_id, sub_part_id;
  uint no_subparts;
  longlong local_func_value;
  int error;
  DBUG_ENTER("get_partition_id_range_sub_key");

  if (unlikely((error= get_partition_id_list(part_info, &loc_part_id,
                                             func_value))))
  {
    DBUG_RETURN(error);
  }
  no_subparts= part_info->no_subparts;
  sub_part_id= get_part_id_key(part_info->subpart_field_array,
                               no_subparts, &local_func_value);
  *part_id= get_part_id_for_sub(loc_part_id, sub_part_id, no_subparts);
  DBUG_RETURN(0);
}


int get_partition_id_list_sub_linear_key(partition_info *part_info,
                                          uint32 *part_id,
                                          longlong *func_value)
{
  uint32 loc_part_id, sub_part_id;
  uint no_subparts;
  longlong local_func_value;
  int error;
  DBUG_ENTER("get_partition_id_list_sub_linear_key");

  if (unlikely((error= get_partition_id_list(part_info, &loc_part_id,
                                             func_value))))
  {
    DBUG_RETURN(error);
  }
  no_subparts= part_info->no_subparts;
  sub_part_id= get_part_id_linear_key(part_info,
                                      part_info->subpart_field_array,
                                      no_subparts, &local_func_value);
  *part_id= get_part_id_for_sub(loc_part_id, sub_part_id, no_subparts);
  DBUG_RETURN(0);
}


/*
  This function is used to calculate the subpartition id

  SYNOPSIS
    get_subpartition_id()
    part_info           A reference to the partition_info struct where all the
                        desired information is given

  RETURN VALUE
    part_id             The subpartition identity

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
  longlong func_value;
  return get_part_id_hash(part_info->no_subparts, part_info->subpart_expr,
                          &func_value);
}


uint32 get_partition_id_linear_hash_sub(partition_info *part_info)
{
  longlong func_value;
  return get_part_id_linear_hash(part_info, part_info->no_subparts,
                                 part_info->subpart_expr, &func_value);
}


uint32 get_partition_id_key_sub(partition_info *part_info)
{
  longlong func_value;
  return get_part_id_key(part_info->subpart_field_array,
                         part_info->no_subparts, &func_value);
}


uint32 get_partition_id_linear_key_sub(partition_info *part_info)
{
  longlong func_value;
  return get_part_id_linear_key(part_info,
                                part_info->subpart_field_array,
                                part_info->no_subparts, &func_value);
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
  partition_info *part_info= table->part_info;
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
    out:part_id   Partition to use

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
  partition_info *part_info= table->part_info;
  longlong func_value;
  DBUG_ENTER("get_part_id_from_key");

  key_restore(buf, (byte*)key_spec->key, key_info, key_spec->length);
  if (likely(rec0 == buf))
    result= part_info->get_part_partition_id(part_info, part_id,
                                             &func_value);
  else
  {
    Field **part_field_array= part_info->part_field_array;
    set_field_ptr(part_field_array, buf, rec0);
    result= part_info->get_part_partition_id(part_info, part_id,
                                             &func_value);
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
    out:part_spec A partition id containing start part and end part

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
  partition_info *part_info= table->part_info;
  byte *rec0= table->record[0];
  longlong func_value;
  DBUG_ENTER("get_full_part_id_from_key");

  key_restore(buf, (byte*)key_spec->key, key_info, key_spec->length);
  if (likely(rec0 == buf))
    result= part_info->get_partition_id(part_info, &part_spec->start_part,
                                        &func_value);
  else
  {
    Field **part_field_array= part_info->full_part_field_array;
    set_field_ptr(part_field_array, buf, rec0);
    result= part_info->get_partition_id(part_info, &part_spec->start_part,
                                        &func_value);
    set_field_ptr(part_field_array, rec0, buf);
  }
  part_spec->end_part= part_spec->start_part;
  if (unlikely(result))
    part_spec->start_part++;
  DBUG_VOID_RETURN;
}

/*
  Prune the set of partitions to use in query 

  SYNOPSIS
    prune_partition_set()
    table         The table object
    out:part_spec Contains start part, end part 

  DESCRIPTION
    This function is called to prune the range of partitions to scan by
    checking the used_partitions bitmap.
    If start_part > end_part at return it means no partition needs to be
    scanned. If start_part == end_part it always means a single partition
    needs to be scanned.

  RETURN VALUE
    part_spec
*/
void prune_partition_set(const TABLE *table, part_id_range *part_spec)
{
  int last_partition= -1;
  uint i;
  partition_info *part_info= table->part_info;

  DBUG_ENTER("prune_partition_set");
  for (i= part_spec->start_part; i <= part_spec->end_part; i++)
  {
    if (bitmap_is_set(&(part_info->used_partitions), i))
    {
      DBUG_PRINT("info", ("Partition %d is set", i));
      if (last_partition == -1)
        /* First partition found in set and pruned bitmap */
        part_spec->start_part= i;
      last_partition= i;
    }
  }
  if (last_partition == -1)
    /* No partition found in pruned bitmap */
    part_spec->start_part= part_spec->end_part + 1;  
  else //if (last_partition != -1)
    part_spec->end_part= last_partition;

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
    out:part_spec Contains start part, end part and indicator if bitmap is
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
  partition_info *part_info= table->part_info;
  uint no_parts= part_info->get_tot_partitions();
  uint i, part_id;
  uint sub_part= no_parts;
  uint32 part_part= no_parts;
  KEY *key_info= NULL;
  bool found_part_field= FALSE;
  DBUG_ENTER("get_partition_set");

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
        /*
          Check if range can be adjusted by looking in used_partitions
        */
        prune_partition_set(table, part_spec);
        DBUG_VOID_RETURN;
      }
      else if (part_info->is_sub_partitioned())
      {
        if (part_info->all_fields_in_SPF.is_set(index))
          sub_part= get_sub_part_id_from_key(table, buf, key_info, key_spec);
        else if (part_info->all_fields_in_PPF.is_set(index))
        {
          if (get_part_id_from_key(table,buf,key_info,
                                   key_spec,(uint32*)&part_part))
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
          /*
            Check if range can be adjusted by looking in used_partitions
          */
          prune_partition_set(table, part_spec);
          DBUG_VOID_RETURN; 
        }
        else if (part_info->is_sub_partitioned())
        {
          if (check_part_func_bound(part_info->subpart_field_array))
            sub_part= get_sub_part_id_from_key(table, buf, key_info, key_spec);
          else if (check_part_func_bound(part_info->part_field_array))
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
      part_spec->start_part= part_part * part_info->no_subparts;
      part_spec->end_part= part_spec->start_part+part_info->no_subparts - 1;
    }
    else
    {
      DBUG_ASSERT(sub_part != no_parts);
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
  /*
    Check if range can be adjusted by looking in used_partitions
  */
  prune_partition_set(table, part_spec);
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
   |field names readable        |
   -------------------------------
   | Packed field info           |
   |17 + 1 + strlen(field_name) |
   | + 1 end of file character   |
   -------------------------------
   | Partition info              |
   -------------------------------
   We provide the length of partition length in Fileinfo[55-58].

   Read the partition syntax from the frm file and parse it to get the
   data structures of the partitioning.

   SYNOPSIS
     mysql_unpack_partition()
     thd                           Thread object
     part_buf                      Partition info from frm file
     part_info_len                 Length of partition syntax
     table                         Table object of partitioned table
     create_table_ind              Is it called from CREATE TABLE
     default_db_type               What is the default engine of the table

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

bool mysql_unpack_partition(THD *thd, const uchar *part_buf,
                            uint part_info_len,
                            uchar *part_state, uint part_state_len,
                            TABLE* table, bool is_create_table_ind,
                            handlerton *default_db_type)
{
  Item *thd_free_list= thd->free_list;
  bool result= TRUE;
  partition_info *part_info;
  CHARSET_INFO *old_character_set_client= thd->variables.character_set_client;
  LEX *old_lex= thd->lex;
  LEX lex;
  DBUG_ENTER("mysql_unpack_partition");

  thd->lex= &lex;
  thd->variables.character_set_client= system_charset_info;
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
  lex.part_info= new partition_info();/* Indicates MYSQLparse from this place */
  if (!lex.part_info)
  {
    mem_alloc_error(sizeof(partition_info));
    goto end;
  }
  lex.part_info->part_state= part_state;
  lex.part_info->part_state_len= part_state_len;
  DBUG_PRINT("info", ("Parse: %s", part_buf));
  if (MYSQLparse((void*)thd) || thd->is_fatal_error)
  {
    free_items(thd->free_list);
    goto end;
  }
  /*
    The parsed syntax residing in the frm file can still contain defaults.
    The reason is that the frm file is sometimes saved outside of this
    MySQL Server and used in backup and restore of clusters or partitioned
    tables. It is not certain that the restore will restore exactly the
    same default partitioning.
    
    The easiest manner of handling this is to simply continue using the
    part_info we already built up during mysql_create_table if we are
    in the process of creating a table. If the table already exists we
    need to discover the number of partitions for the default parts. Since
    the handler object hasn't been created here yet we need to postpone this
    to the fix_partition_func method.
  */

  DBUG_PRINT("info", ("Successful parse"));
  part_info= lex.part_info;
  DBUG_PRINT("info", ("default engine = %d, default_db_type = %d",
             ha_legacy_type(part_info->default_engine_type),
             ha_legacy_type(default_db_type)));
  if (is_create_table_ind && old_lex->sql_command == SQLCOM_CREATE_TABLE)
  {
    if (old_lex->like_name)
    {
      /*
        This code is executed when we do a CREATE TABLE t1 LIKE t2
        old_lex->like_name contains the t2 and the table we are opening has 
        name t1.
      */
      Table_ident *table_ident= old_lex->like_name;
      char *src_db= table_ident->db.str ? table_ident->db.str : thd->db;
      char *src_table= table_ident->table.str;
      char buf[FN_REFLEN];
      build_table_filename(buf, sizeof(buf), src_db, src_table, "");
      if (partition_default_handling(table, part_info,
                                     FALSE, buf))
      {
        result= TRUE;
        goto end;
      }
    }
    else
    {
      /*
        When we come here we are doing a create table. In this case we
        have already done some preparatory work on the old part_info
        object. We don't really need this new partition_info object.
        Thus we go back to the old partition info object.
        We need to free any memory objects allocated on item_free_list
        by the parser since we are keeping the old info from the first
        parser call in CREATE TABLE.
        We'll ensure that this object isn't put into table cache also
        just to ensure we don't get into strange situations with the
        item objects.
      */
      free_items(thd->free_list);
      part_info= thd->work_part_info;
      thd->free_list= NULL;
      table->s->version= 0UL;
    }
  }
  table->part_info= part_info;
  table->file->set_part_info(part_info);
  if (part_info->default_engine_type == NULL)
  {
    part_info->default_engine_type= default_db_type;
  }
  else
  {
    DBUG_ASSERT(part_info->default_engine_type == default_db_type);
  }
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
    char *part_func_string= NULL;
    char *subpart_func_string= NULL;
    if ((part_func_len &&
        !((part_func_string= thd->alloc(part_func_len)))) ||
        (subpart_func_len &&
        !((subpart_func_string= thd->alloc(subpart_func_len)))))
    {
      mem_alloc_error(part_func_len);
      free_items(thd->free_list);
      part_info->item_free_list= 0;
      goto end;
    }
    if (part_func_len)
      memcpy(part_func_string, part_info->part_func_string, part_func_len);
    if (subpart_func_len)
      memcpy(subpart_func_string, part_info->subpart_func_string,
             subpart_func_len);
    part_info->part_func_string= part_func_string;
    part_info->subpart_func_string= subpart_func_string;
  }

  result= FALSE;
end:
  lex_end(thd->lex);
  thd->free_list= thd_free_list;
  thd->lex= old_lex;
  thd->variables.character_set_client= old_character_set_client;
  DBUG_RETURN(result);
}


/*
  Set engine type on all partition element objects
  SYNOPSIS
    set_engine_all_partitions()
    part_info                  Partition info
    engine_type                Handlerton reference of engine
  RETURN VALUES
    NONE
*/

static
void
set_engine_all_partitions(partition_info *part_info,
                          handlerton *engine_type)
{
  uint i= 0;
  List_iterator<partition_element> part_it(part_info->partitions);
  do
  {
    partition_element *part_elem= part_it++;

    part_elem->engine_type= engine_type;
    if (part_info->is_sub_partitioned())
    {
      List_iterator<partition_element> sub_it(part_elem->subpartitions);
      uint j= 0;

      do
      {
        partition_element *sub_elem= sub_it++;

        sub_elem->engine_type= engine_type;
      } while (++j < part_info->no_subparts);
    }
  } while (++i < part_info->no_parts);
}
/*
  SYNOPSIS
    fast_end_partition()
    thd                           Thread object
    out:copied                    Number of records copied
    out:deleted                   Number of records deleted
    table_list                    Table list with the one table in it
    empty                         Has nothing been done
    lpt                           Struct to be used by error handler

  RETURN VALUES
    FALSE                         Success
    TRUE                          Failure

  DESCRIPTION
    Support routine to handle the successful cases for partition
    management.
*/

static int fast_end_partition(THD *thd, ulonglong copied,
                              ulonglong deleted,
                              TABLE *table,
                              TABLE_LIST *table_list, bool is_empty,
                              ALTER_PARTITION_PARAM_TYPE *lpt,
                              bool written_bin_log)
{
  int error;
  DBUG_ENTER("fast_end_partition");

  thd->proc_info="end";
  if (!is_empty)
    query_cache_invalidate3(thd, table_list, 0);
  error= ha_commit_stmt(thd);
  if (ha_commit(thd))
    error= 1;
  if (!error || is_empty)
  {
    char tmp_name[80];
    if ((!is_empty) && (!written_bin_log) &&
        (!thd->lex->no_write_to_binlog))
      write_bin_log(thd, FALSE, thd->query, thd->query_length);
    close_thread_tables(thd);
    my_snprintf(tmp_name, sizeof(tmp_name), ER(ER_INSERT_INFO),
                (ulong) (copied + deleted),
                (ulong) deleted,
                (ulong) 0);
    send_ok(thd,copied+deleted,0L,tmp_name);
    DBUG_RETURN(FALSE);
  }
  table->file->print_error(error, MYF(0));
  DBUG_RETURN(TRUE);
}


/*
  Check engine mix that it is correct
  SYNOPSIS
    check_engine_condition()
    p_elem                   Partition element
    default_engine           Have user specified engine on table level
    inout::engine_type       Current engine used
    inout::first             Is it first partition
  RETURN VALUE
    TRUE                     Failed check
    FALSE                    Ok
  DESCRIPTION
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

static bool check_engine_condition(partition_element *p_elem,
                                   bool default_engine,
                                   handlerton **engine_type,
                                   bool *first)
{
  DBUG_ENTER("check_engine_condition");

  DBUG_PRINT("enter", ("def_eng = %u, first = %u", default_engine, *first));
  if (*first && default_engine)
  {
    *engine_type= p_elem->engine_type;
  }
  *first= FALSE;
  if ((!default_engine &&
      (p_elem->engine_type != (*engine_type) &&
       p_elem->engine_type)) ||
      (default_engine &&
       p_elem->engine_type != (*engine_type)))
  {
    DBUG_RETURN(TRUE);
  }
  else
  {
    DBUG_RETURN(FALSE);
  }
}

/*
  We need to check if engine used by all partitions can handle
  partitioning natively.

  SYNOPSIS
    check_native_partitioned()
    create_info            Create info in CREATE TABLE
    out:ret_val            Return value
    part_info              Partition info
    thd                    Thread object

  RETURN VALUES
  Value returned in bool ret_value
    TRUE                   Native partitioning supported by engine
    FALSE                  Need to use partition handler

  Return value from function
    TRUE                   Error
    FALSE                  Success
*/

static bool check_native_partitioned(HA_CREATE_INFO *create_info,bool *ret_val,
                                     partition_info *part_info, THD *thd)
{
  List_iterator<partition_element> part_it(part_info->partitions);
  bool first= TRUE;
  bool default_engine;
  handlerton *engine_type= create_info->db_type;
  handlerton *old_engine_type= engine_type;
  uint i= 0;
  handler *file;
  uint no_parts= part_info->partitions.elements;
  DBUG_ENTER("check_native_partitioned");

  default_engine= (create_info->used_fields & HA_CREATE_USED_ENGINE) ?
                   FALSE : TRUE;
  DBUG_PRINT("info", ("engine_type = %u, default = %u",
                       ha_legacy_type(engine_type),
                       default_engine));
  if (no_parts)
  {
    do
    {
      partition_element *part_elem= part_it++;
      if (part_info->is_sub_partitioned() &&
          part_elem->subpartitions.elements)
      {
        uint no_subparts= part_elem->subpartitions.elements;
        uint j= 0;
        List_iterator<partition_element> sub_it(part_elem->subpartitions);
        do
        {
          partition_element *sub_elem= sub_it++;
          if (check_engine_condition(sub_elem, default_engine,
                                     &engine_type, &first))
            goto error;
        } while (++j < no_subparts);
        /*
          In case of subpartitioning and defaults we allow that only
          subparts have specified engines, as long as the parts haven't
          specified the wrong engine it's ok.
        */
        if (check_engine_condition(part_elem, FALSE,
                                   &engine_type, &first))
          goto error;
      }
      else if (check_engine_condition(part_elem, default_engine,
                                      &engine_type, &first))
        goto error;
    } while (++i < no_parts);
  }

  /*
    All engines are of the same type. Check if this engine supports
    native partitioning.
  */

  if (!engine_type)
    engine_type= old_engine_type;
  DBUG_PRINT("info", ("engine_type = %s",
              ha_resolve_storage_engine_name(engine_type)));
  if (engine_type->partition_flags &&
      (engine_type->partition_flags() & HA_CAN_PARTITION))
  {
    create_info->db_type= engine_type;
    DBUG_PRINT("info", ("Changed to native partitioning"));
    *ret_val= TRUE;
  }
  DBUG_RETURN(FALSE);
error:
  /*
    Mixed engines not yet supported but when supported it will need
    the partition handler
  */
  my_error(ER_MIX_HANDLER_ERROR, MYF(0));
  *ret_val= FALSE;
  DBUG_RETURN(TRUE);
}


/*
  Prepare for ALTER TABLE of partition structure

  SYNOPSIS
    prep_alter_part_table()
    thd                        Thread object
    table                      Table object
    inout:alter_info           Alter information
    inout:create_info          Create info for CREATE TABLE
    old_db_type                Old engine type
    out:partition_changed      Boolean indicating whether partition changed
    out:fast_alter_partition   Boolean indicating whether fast partition
                               change is requested

  RETURN VALUES
    TRUE                       Error
    FALSE                      Success
    partition_changed
    fast_alter_partition

  DESCRIPTION
    This method handles all preparations for ALTER TABLE for partitioned
    tables
    We need to handle both partition management command such as Add Partition
    and others here as well as an ALTER TABLE that completely changes the
    partitioning and yet others that don't change anything at all. We start
    by checking the partition management variants and then check the general
    change patterns.
*/

uint prep_alter_part_table(THD *thd, TABLE *table, ALTER_INFO *alter_info,
                           HA_CREATE_INFO *create_info,
                           handlerton *old_db_type,
                           bool *partition_changed,
                           uint *fast_alter_partition)
{
  DBUG_ENTER("prep_alter_part_table");

  /*
    We are going to manipulate the partition info on the table object
    so we need to ensure that the data structure of the table object
    is freed by setting version to 0. table->s->version= 0 forces a
    flush of the table object in close_thread_tables().
  */
  if (table->part_info)
    table->s->version= 0L;

  thd->work_part_info= thd->lex->part_info;
  if (thd->work_part_info &&
      !(thd->work_part_info= thd->lex->part_info->get_clone()))
    DBUG_RETURN(TRUE);

  if (alter_info->flags &
      (ALTER_ADD_PARTITION | ALTER_DROP_PARTITION |
       ALTER_COALESCE_PARTITION | ALTER_REORGANIZE_PARTITION |
       ALTER_TABLE_REORG | ALTER_OPTIMIZE_PARTITION |
       ALTER_CHECK_PARTITION | ALTER_ANALYZE_PARTITION |
       ALTER_REPAIR_PARTITION | ALTER_REBUILD_PARTITION))
  {
    partition_info *tab_part_info= table->part_info;
    partition_info *alt_part_info= thd->work_part_info;
    uint flags= 0;
    if (!tab_part_info)
    {
      my_error(ER_PARTITION_MGMT_ON_NONPARTITIONED, MYF(0));
      DBUG_RETURN(TRUE);
    }
    if (alter_info->flags == ALTER_TABLE_REORG)
    {
      uint new_part_no, curr_part_no;
      if (tab_part_info->part_type != HASH_PARTITION ||
          tab_part_info->use_default_no_partitions)
      {
        my_error(ER_REORG_NO_PARAM_ERROR, MYF(0));
        DBUG_RETURN(TRUE);
      }
      new_part_no= table->file->get_default_no_partitions(create_info);
      curr_part_no= tab_part_info->no_parts;
      if (new_part_no == curr_part_no)
      {
        /*
          No change is needed, we will have the same number of partitions
          after the change as before. Thus we can reply ok immediately
          without any changes at all.
        */
        DBUG_RETURN(fast_end_partition(thd, ULL(0), ULL(0),
                                       table, NULL,
                                       TRUE, NULL, FALSE));
      }
      else if (new_part_no > curr_part_no)
      {
        /*
          We will add more partitions, we use the ADD PARTITION without
          setting the flag for no default number of partitions
        */
        alter_info->flags|= ALTER_ADD_PARTITION;
        thd->work_part_info->no_parts= new_part_no - curr_part_no;
      }
      else
      {
        /*
          We will remove hash partitions, we use the COALESCE PARTITION
          without setting the flag for no default number of partitions
        */
        alter_info->flags|= ALTER_COALESCE_PARTITION;
        alter_info->no_parts= curr_part_no - new_part_no;
      }
    }
    if (table->s->db_type->alter_table_flags &&
        (!(flags= table->s->db_type->alter_table_flags(alter_info->flags))))
    {
      my_error(ER_PARTITION_FUNCTION_FAILURE, MYF(0));
      DBUG_RETURN(1);
    }
    *fast_alter_partition=
      ((flags & (HA_FAST_CHANGE_PARTITION | HA_PARTITION_ONE_PHASE)) != 0);
    DBUG_PRINT("info", ("*fast_alter_partition: %d  flags: 0x%x",
                        *fast_alter_partition, flags));
    if (((alter_info->flags & ALTER_ADD_PARTITION) ||
         (alter_info->flags & ALTER_REORGANIZE_PARTITION)) &&
         (thd->work_part_info->part_type != tab_part_info->part_type) &&
         (thd->work_part_info->part_type != NOT_A_PARTITION))
    {
      if (thd->work_part_info->part_type == RANGE_PARTITION)
      {
        my_error(ER_PARTITION_WRONG_VALUES_ERROR, MYF(0),
                 "RANGE", "LESS THAN");
      }
      else if (thd->work_part_info->part_type == LIST_PARTITION)
      {
        DBUG_ASSERT(thd->work_part_info->part_type == LIST_PARTITION);
        my_error(ER_PARTITION_WRONG_VALUES_ERROR, MYF(0),
                 "LIST", "IN");
      }
      else if (tab_part_info->part_type == RANGE_PARTITION)
      {
        my_error(ER_PARTITION_REQUIRES_VALUES_ERROR, MYF(0),
                 "RANGE", "LESS THAN");
      }
      else
      {
        DBUG_ASSERT(tab_part_info->part_type == LIST_PARTITION);
        my_error(ER_PARTITION_REQUIRES_VALUES_ERROR, MYF(0),
                 "LIST", "IN");
      }
      DBUG_RETURN(TRUE);
    }
    if (alter_info->flags & ALTER_ADD_PARTITION)
    {
      /*
        We start by moving the new partitions to the list of temporary
        partitions. We will then check that the new partitions fit in the
        partitioning scheme as currently set-up.
        Partitions are always added at the end in ADD PARTITION.
      */
      uint no_new_partitions= alt_part_info->no_parts;
      uint no_orig_partitions= tab_part_info->no_parts;
      uint check_total_partitions= no_new_partitions + no_orig_partitions;
      uint new_total_partitions= check_total_partitions;
      /*
        We allow quite a lot of values to be supplied by defaults, however we
        must know the number of new partitions in this case.
      */
      if (thd->lex->no_write_to_binlog &&
          tab_part_info->part_type != HASH_PARTITION)
      {
        my_error(ER_NO_BINLOG_ERROR, MYF(0));
        DBUG_RETURN(TRUE);
      } 
      if (no_new_partitions == 0)
      {
        my_error(ER_ADD_PARTITION_NO_NEW_PARTITION, MYF(0));
        DBUG_RETURN(TRUE);
      }
      if (tab_part_info->is_sub_partitioned())
      {
        if (alt_part_info->no_subparts == 0)
          alt_part_info->no_subparts= tab_part_info->no_subparts;
        else if (alt_part_info->no_subparts != tab_part_info->no_subparts)
        {
          my_error(ER_ADD_PARTITION_SUBPART_ERROR, MYF(0));
          DBUG_RETURN(TRUE);
        }
        check_total_partitions= new_total_partitions*
                                alt_part_info->no_subparts;
      }
      if (check_total_partitions > MAX_PARTITIONS)
      {
        my_error(ER_TOO_MANY_PARTITIONS_ERROR, MYF(0));
        DBUG_RETURN(TRUE);
      }
      alt_part_info->part_type= tab_part_info->part_type;
      alt_part_info->subpart_type= tab_part_info->subpart_type;
      if (alt_part_info->set_up_defaults_for_partitioning(table->file,
                                                          ULL(0), 
                                                          tab_part_info->no_parts))
      {
        DBUG_RETURN(TRUE);
      }
/*
Handling of on-line cases:

ADD PARTITION for RANGE/LIST PARTITIONING:
------------------------------------------
For range and list partitions add partition is simply adding a
new empty partition to the table. If the handler support this we
will use the simple method of doing this. The figure below shows
an example of this and the states involved in making this change.
            
Existing partitions                                     New added partitions
------       ------        ------        ------      |  ------    ------
|    |       |    |        |    |        |    |      |  |    |    |    |
| p0 |       | p1 |        | p2 |        | p3 |      |  | p4 |    | p5 |
------       ------        ------        ------      |  ------    ------
PART_NORMAL  PART_NORMAL   PART_NORMAL   PART_NORMAL    PART_TO_BE_ADDED*2
PART_NORMAL  PART_NORMAL   PART_NORMAL   PART_NORMAL    PART_IS_ADDED*2

The first line is the states before adding the new partitions and the 
second line is after the new partitions are added. All the partitions are
in the partitions list, no partitions are placed in the temp_partitions
list.

ADD PARTITION for HASH PARTITIONING
-----------------------------------
This little figure tries to show the various partitions involved when
adding two new partitions to a linear hash based partitioned table with
four partitions to start with, which lists are used and the states they
pass through. Adding partitions to a normal hash based is similar except
that it is always all the existing partitions that are reorganised not
only a subset of them.

Existing partitions                                     New added partitions
------       ------        ------        ------      |  ------    ------
|    |       |    |        |    |        |    |      |  |    |    |    |
| p0 |       | p1 |        | p2 |        | p3 |      |  | p4 |    | p5 |
------       ------        ------        ------      |  ------    ------
PART_CHANGED PART_CHANGED  PART_NORMAL   PART_NORMAL    PART_TO_BE_ADDED
PART_IS_CHANGED*2          PART_NORMAL   PART_NORMAL    PART_IS_ADDED
PART_NORMAL  PART_NORMAL   PART_NORMAL   PART_NORMAL    PART_IS_ADDED

Reorganised existing partitions
------      ------
|    |      |    |
| p0'|      | p1'|
------      ------

p0 - p5 will be in the partitions list of partitions.
p0' and p1' will actually not exist as separate objects, there presence can
be deduced from the state of the partition and also the names of those
partitions can be deduced this way.

After adding the partitions and copying the partition data to p0', p1',
p4 and p5 from p0 and p1 the states change to adapt for the new situation
where p0 and p1 is dropped and replaced by p0' and p1' and the new p4 and
p5 are in the table again.

The first line above shows the states of the partitions before we start
adding and copying partitions, the second after completing the adding
and copying and finally the third line after also dropping the partitions
that are reorganised.
*/
      if (*fast_alter_partition &&
          tab_part_info->part_type == HASH_PARTITION)
      {
        uint part_no= 0, start_part= 1, start_sec_part= 1;
        uint end_part= 0, end_sec_part= 0;
        uint upper_2n= tab_part_info->linear_hash_mask + 1;
        uint lower_2n= upper_2n >> 1;
        bool all_parts= TRUE;
        if (tab_part_info->linear_hash_ind &&
            no_new_partitions < upper_2n)
        {
          /*
            An analysis of which parts needs reorganisation shows that it is
            divided into two intervals. The first interval is those parts
            that are reorganised up until upper_2n - 1. From upper_2n and
            onwards it starts again from partition 0 and goes on until
            it reaches p(upper_2n - 1). If the last new partition reaches
            beyond upper_2n - 1 then the first interval will end with
            p(lower_2n - 1) and start with p(no_orig_partitions - lower_2n).
            If lower_2n partitions are added then p0 to p(lower_2n - 1) will
            be reorganised which means that the two interval becomes one
            interval at this point. Thus only when adding less than
            lower_2n partitions and going beyond a total of upper_2n we
            actually get two intervals.

            To exemplify this assume we have 6 partitions to start with and
            add 1, 2, 3, 5, 6, 7, 8, 9 partitions.
            The first to add after p5 is p6 = 110 in bit numbers. Thus we
            can see that 10 = p2 will be partition to reorganise if only one
            partition.
            If 2 partitions are added we reorganise [p2, p3]. Those two
            cases are covered by the second if part below.
            If 3 partitions are added we reorganise [p2, p3] U [p0,p0]. This
            part is covered by the else part below.
            If 5 partitions are added we get [p2,p3] U [p0, p2] = [p0, p3].
            This is covered by the first if part where we need the max check
            to here use lower_2n - 1.
            If 7 partitions are added we get [p2,p3] U [p0, p4] = [p0, p4].
            This is covered by the first if part but here we use the first
            calculated end_part.
            Finally with 9 new partitions we would also reorganise p6 if we
            used the method below but we cannot reorganise more partitions
            than what we had from the start and thus we simply set all_parts
            to TRUE. In this case we don't get into this if-part at all.
          */
          all_parts= FALSE;
          if (no_new_partitions >= lower_2n)
          {
            /*
              In this case there is only one interval since the two intervals
              overlap and this starts from zero to last_part_no - upper_2n
            */
            start_part= 0;
            end_part= new_total_partitions - (upper_2n + 1);
            end_part= max(lower_2n - 1, end_part);
          }
          else if (new_total_partitions <= upper_2n)
          {
            /*
              Also in this case there is only one interval since we are not
              going over a 2**n boundary
            */
            start_part= no_orig_partitions - lower_2n;
            end_part= start_part + (no_new_partitions - 1);
          }
          else
          {
            /* We have two non-overlapping intervals since we are not
               passing a 2**n border and we have not at least lower_2n
               new parts that would ensure that the intervals become
               overlapping.
            */
            start_part= no_orig_partitions - lower_2n;
            end_part= upper_2n - 1;
            start_sec_part= 0;
            end_sec_part= new_total_partitions - (upper_2n + 1);
          }
        }
        List_iterator<partition_element> tab_it(tab_part_info->partitions);
        part_no= 0;
        do
        {
          partition_element *p_elem= tab_it++;
          if (all_parts ||
              (part_no >= start_part && part_no <= end_part) ||
              (part_no >= start_sec_part && part_no <= end_sec_part))
          {
            p_elem->part_state= PART_CHANGED;
          }
        } while (++part_no < no_orig_partitions);
      }
      /*
        Need to concatenate the lists here to make it possible to check the
        partition info for correctness using check_partition_info.
        For on-line add partition we set the state of this partition to
        PART_TO_BE_ADDED to ensure that it is known that it is not yet
        usable (becomes usable when partition is created and the switch of
        partition configuration is made.
      */
      {
        List_iterator<partition_element> alt_it(alt_part_info->partitions);
        uint part_count= 0;
        do
        {
          partition_element *part_elem= alt_it++;
          if (*fast_alter_partition)
            part_elem->part_state= PART_TO_BE_ADDED;
          if (tab_part_info->partitions.push_back(part_elem))
          {
            mem_alloc_error(1);
            DBUG_RETURN(TRUE);
          }
        } while (++part_count < no_new_partitions);
        tab_part_info->no_parts+= no_new_partitions;
      }
      /*
        If we specify partitions explicitly we don't use defaults anymore.
        Using ADD PARTITION also means that we don't have the default number
        of partitions anymore. We use this code also for Table reorganisations
        and here we don't set any default flags to FALSE.
      */
      if (!(alter_info->flags & ALTER_TABLE_REORG))
      {
        if (!alt_part_info->use_default_partitions)
        {
          DBUG_PRINT("info", ("part_info= %x", tab_part_info));
          tab_part_info->use_default_partitions= FALSE;
        }
        tab_part_info->use_default_no_partitions= FALSE;
        tab_part_info->is_auto_partitioned= FALSE;
      }
    }
    else if (alter_info->flags == ALTER_DROP_PARTITION)
    {
      /*
        Drop a partition from a range partition and list partitioning is
        always safe and can be made more or less immediate. It is necessary
        however to ensure that the partition to be removed is safely removed
        and that REPAIR TABLE can remove the partition if for some reason the
        command to drop the partition failed in the middle.
      */
      uint part_count= 0;
      uint no_parts_dropped= alter_info->partition_names.elements;
      uint no_parts_found= 0;
      List_iterator<partition_element> part_it(tab_part_info->partitions);

      tab_part_info->is_auto_partitioned= FALSE;
      if (!(tab_part_info->part_type == RANGE_PARTITION ||
            tab_part_info->part_type == LIST_PARTITION))
      {
        my_error(ER_ONLY_ON_RANGE_LIST_PARTITION, MYF(0), "DROP");
        DBUG_RETURN(TRUE);
      }
      if (no_parts_dropped >= tab_part_info->no_parts)
      {
        my_error(ER_DROP_LAST_PARTITION, MYF(0));
        DBUG_RETURN(TRUE);
      }
      do
      {
        partition_element *part_elem= part_it++;
        if (is_name_in_list(part_elem->partition_name,
                            alter_info->partition_names))
        {
          /*
            Set state to indicate that the partition is to be dropped.
          */
          no_parts_found++;
          part_elem->part_state= PART_TO_BE_DROPPED;
        }
      } while (++part_count < tab_part_info->no_parts);
      if (no_parts_found != no_parts_dropped)
      {
        my_error(ER_DROP_PARTITION_NON_EXISTENT, MYF(0), "DROP");
        DBUG_RETURN(TRUE);
      }
      if (table->file->is_fk_defined_on_table_or_index(MAX_KEY))
      {
        my_error(ER_ROW_IS_REFERENCED, MYF(0));
        DBUG_RETURN(TRUE);
      }
      tab_part_info->no_parts-= no_parts_dropped;
    }
    else if ((alter_info->flags & ALTER_OPTIMIZE_PARTITION) ||
             (alter_info->flags & ALTER_ANALYZE_PARTITION) ||
             (alter_info->flags & ALTER_CHECK_PARTITION) ||
             (alter_info->flags & ALTER_REPAIR_PARTITION) ||
             (alter_info->flags & ALTER_REBUILD_PARTITION))
    {
      uint no_parts_opt= alter_info->partition_names.elements;
      uint part_count= 0;
      uint no_parts_found= 0;
      List_iterator<partition_element> part_it(tab_part_info->partitions);

      do
      {
        partition_element *part_elem= part_it++;
        if ((alter_info->flags & ALTER_ALL_PARTITION) ||
            (is_name_in_list(part_elem->partition_name,
                             alter_info->partition_names)))
        {
          /*
            Mark the partition as a partition to be "changed" by
            analyzing/optimizing/rebuilding/checking/repairing
          */
          no_parts_found++;
          part_elem->part_state= PART_CHANGED;
        }
      } while (++part_count < tab_part_info->no_parts);
      if (no_parts_found != no_parts_opt &&
          (!(alter_info->flags & ALTER_ALL_PARTITION)))
      {
        const char *ptr;
        if (alter_info->flags & ALTER_OPTIMIZE_PARTITION)
          ptr= "OPTIMIZE";
        else if (alter_info->flags & ALTER_ANALYZE_PARTITION)
          ptr= "ANALYZE";
        else if (alter_info->flags & ALTER_CHECK_PARTITION)
          ptr= "CHECK";
        else if (alter_info->flags & ALTER_REPAIR_PARTITION)
          ptr= "REPAIR";
        else
          ptr= "REBUILD";
        my_error(ER_DROP_PARTITION_NON_EXISTENT, MYF(0), ptr);
        DBUG_RETURN(TRUE);
      }
      if (!(*fast_alter_partition))
      {
        table->file->print_error(HA_ERR_WRONG_COMMAND, MYF(0));
        DBUG_RETURN(TRUE);
      }
    }
    else if (alter_info->flags & ALTER_COALESCE_PARTITION)
    {
      uint no_parts_coalesced= alter_info->no_parts;
      uint no_parts_remain= tab_part_info->no_parts - no_parts_coalesced;
      List_iterator<partition_element> part_it(tab_part_info->partitions);
      if (tab_part_info->part_type != HASH_PARTITION)
      {
        my_error(ER_COALESCE_ONLY_ON_HASH_PARTITION, MYF(0));
        DBUG_RETURN(TRUE);
      }
      if (no_parts_coalesced == 0)
      {
        my_error(ER_COALESCE_PARTITION_NO_PARTITION, MYF(0));
        DBUG_RETURN(TRUE);
      }
      if (no_parts_coalesced >= tab_part_info->no_parts)
      {
        my_error(ER_DROP_LAST_PARTITION, MYF(0));
        DBUG_RETURN(TRUE);
      }
/*
Online handling:
COALESCE PARTITION:
-------------------
The figure below shows the manner in which partitions are handled when
performing an on-line coalesce partition and which states they go through
at start, after adding and copying partitions and finally after dropping
the partitions to drop. The figure shows an example using four partitions
to start with, using linear hash and coalescing one partition (always the
last partition).

Using linear hash then all remaining partitions will have a new reorganised
part.

Existing partitions                     Coalesced partition 
------       ------              ------   |      ------
|    |       |    |              |    |   |      |    |
| p0 |       | p1 |              | p2 |   |      | p3 |
------       ------              ------   |      ------
PART_NORMAL  PART_CHANGED        PART_NORMAL     PART_REORGED_DROPPED
PART_NORMAL  PART_IS_CHANGED     PART_NORMAL     PART_TO_BE_DROPPED
PART_NORMAL  PART_NORMAL         PART_NORMAL     PART_IS_DROPPED

Reorganised existing partitions
            ------
            |    |
            | p1'|
            ------

p0 - p3 is in the partitions list.
The p1' partition will actually not be in any list it is deduced from the
state of p1.
*/
      {
        uint part_count= 0, start_part= 1, start_sec_part= 1;
        uint end_part= 0, end_sec_part= 0;
        bool all_parts= TRUE;
        if (*fast_alter_partition &&
            tab_part_info->linear_hash_ind)
        {
          uint upper_2n= tab_part_info->linear_hash_mask + 1;
          uint lower_2n= upper_2n >> 1;
          all_parts= FALSE;
          if (no_parts_coalesced >= lower_2n)
          {
            all_parts= TRUE;
          }
          else if (no_parts_remain >= lower_2n)
          {
            end_part= tab_part_info->no_parts - (lower_2n + 1);
            start_part= no_parts_remain - lower_2n;
          }
          else
          {
            start_part= 0;
            end_part= tab_part_info->no_parts - (lower_2n + 1);
            end_sec_part= (lower_2n >> 1) - 1;
            start_sec_part= end_sec_part - (lower_2n - (no_parts_remain + 1));
          }
        }
        do
        {
          partition_element *p_elem= part_it++;
          if (*fast_alter_partition &&
              (all_parts ||
              (part_count >= start_part && part_count <= end_part) ||
              (part_count >= start_sec_part && part_count <= end_sec_part)))
            p_elem->part_state= PART_CHANGED;
          if (++part_count > no_parts_remain)
          {
            if (*fast_alter_partition)
              p_elem->part_state= PART_REORGED_DROPPED;
            else
              part_it.remove();
          }
        } while (part_count < tab_part_info->no_parts);
        tab_part_info->no_parts= no_parts_remain;
      }
      if (!(alter_info->flags & ALTER_TABLE_REORG))
      {
        tab_part_info->use_default_no_partitions= FALSE;
        tab_part_info->is_auto_partitioned= FALSE;
      }
    }
    else if (alter_info->flags == ALTER_REORGANIZE_PARTITION)
    {
      /*
        Reorganise partitions takes a number of partitions that are next
        to each other (at least for RANGE PARTITIONS) and then uses those
        to create a set of new partitions. So data is copied from those
        partitions into the new set of partitions. Those new partitions
        can have more values in the LIST value specifications or less both
        are allowed. The ranges can be different but since they are 
        changing a set of consecutive partitions they must cover the same
        range as those changed from.
        This command can be used on RANGE and LIST partitions.
      */
      uint no_parts_reorged= alter_info->partition_names.elements;
      uint no_parts_new= thd->work_part_info->partitions.elements;
      partition_info *alt_part_info= thd->work_part_info;
      uint check_total_partitions;

      tab_part_info->is_auto_partitioned= FALSE;
      if (no_parts_reorged > tab_part_info->no_parts)
      {
        my_error(ER_REORG_PARTITION_NOT_EXIST, MYF(0));
        DBUG_RETURN(TRUE);
      }
      if (!(tab_part_info->part_type == RANGE_PARTITION ||
            tab_part_info->part_type == LIST_PARTITION) &&
           (no_parts_new != no_parts_reorged))
      {
        my_error(ER_REORG_HASH_ONLY_ON_SAME_NO, MYF(0));
        DBUG_RETURN(TRUE);
      }
      check_total_partitions= tab_part_info->no_parts + no_parts_new;
      check_total_partitions-= no_parts_reorged;
      if (check_total_partitions > MAX_PARTITIONS)
      {
        my_error(ER_TOO_MANY_PARTITIONS_ERROR, MYF(0));
        DBUG_RETURN(TRUE);
      }
      alt_part_info->part_type= tab_part_info->part_type;
      alt_part_info->subpart_type= tab_part_info->subpart_type;
      DBUG_ASSERT(!alt_part_info->use_default_partitions);
      if (alt_part_info->set_up_defaults_for_partitioning(table->file,
                                                          ULL(0), 
                                                          0))
      {
        DBUG_RETURN(TRUE);
      }
/*
Online handling:
REORGANIZE PARTITION:
---------------------
The figure exemplifies the handling of partitions, their state changes and
how they are organised. It exemplifies four partitions where two of the
partitions are reorganised (p1 and p2) into two new partitions (p4 and p5).
The reason of this change could be to change range limits, change list
values or for hash partitions simply reorganise the partition which could
also involve moving them to new disks or new node groups (MySQL Cluster).

Existing partitions                                  
------       ------        ------        ------
|    |       |    |        |    |        |    |
| p0 |       | p1 |        | p2 |        | p3 |
------       ------        ------        ------
PART_NORMAL  PART_TO_BE_REORGED          PART_NORMAL
PART_NORMAL  PART_TO_BE_DROPPED          PART_NORMAL
PART_NORMAL  PART_IS_DROPPED             PART_NORMAL

Reorganised new partitions (replacing p1 and p2)
------      ------
|    |      |    |
| p4 |      | p5 |
------      ------
PART_TO_BE_ADDED
PART_IS_ADDED
PART_IS_ADDED

All unchanged partitions and the new partitions are in the partitions list
in the order they will have when the change is completed. The reorganised
partitions are placed in the temp_partitions list. PART_IS_ADDED is only a
temporary state not written in the frm file. It is used to ensure we write
the generated partition syntax in a correct manner.
*/
      {
        List_iterator<partition_element> tab_it(tab_part_info->partitions);
        uint part_count= 0;
        bool found_first= FALSE;
        bool found_last= FALSE;
        bool is_last_partition_reorged;
        uint drop_count= 0;
        longlong tab_max_range= 0, alt_max_range= 0;
        do
        {
          partition_element *part_elem= tab_it++;
          is_last_partition_reorged= FALSE;
          if (is_name_in_list(part_elem->partition_name,
                              alter_info->partition_names))
          {
            is_last_partition_reorged= TRUE;
            drop_count++;
            tab_max_range= part_elem->range_value;
            if (*fast_alter_partition &&
                tab_part_info->temp_partitions.push_back(part_elem))
            {
              mem_alloc_error(1);
              DBUG_RETURN(TRUE);
            }
            if (*fast_alter_partition)
              part_elem->part_state= PART_TO_BE_REORGED;
            if (!found_first)
            {
              uint alt_part_count= 0;
              found_first= TRUE;
              List_iterator<partition_element>
                                 alt_it(alt_part_info->partitions);
              do
              {
                partition_element *alt_part_elem= alt_it++;
                alt_max_range= alt_part_elem->range_value;
                if (*fast_alter_partition)
                  alt_part_elem->part_state= PART_TO_BE_ADDED;
                if (alt_part_count == 0)
                  tab_it.replace(alt_part_elem);
                else
                  tab_it.after(alt_part_elem);
              } while (++alt_part_count < no_parts_new);
            }
            else if (found_last)
            {
              my_error(ER_CONSECUTIVE_REORG_PARTITIONS, MYF(0));
              DBUG_RETURN(TRUE);
            }
            else
              tab_it.remove();
          }
          else
          {
            if (found_first)
              found_last= TRUE;
          }
        } while (++part_count < tab_part_info->no_parts);
        if (drop_count != no_parts_reorged)
        {
          my_error(ER_DROP_PARTITION_NON_EXISTENT, MYF(0), "REORGANIZE");
          DBUG_RETURN(TRUE);
        }
        if (tab_part_info->part_type == RANGE_PARTITION &&
            ((is_last_partition_reorged &&
               alt_max_range < tab_max_range) ||
              (!is_last_partition_reorged &&
               alt_max_range != tab_max_range)))
        {
          /*
            For range partitioning the total resulting range before and
            after the change must be the same except in one case. This is
            when the last partition is reorganised, in this case it is
            acceptable to increase the total range.
            The reason is that it is not allowed to have "holes" in the
            middle of the ranges and thus we should not allow to reorganise
            to create "holes". Also we should not allow using REORGANIZE
            to drop data.
          */
          my_error(ER_REORG_OUTSIDE_RANGE, MYF(0));
          DBUG_RETURN(TRUE);
        }
        tab_part_info->no_parts= check_total_partitions;
      }
    }
    else
    {
      DBUG_ASSERT(FALSE);
    }
    *partition_changed= TRUE;
    thd->work_part_info= tab_part_info;
    if (alter_info->flags == ALTER_ADD_PARTITION ||
        alter_info->flags == ALTER_REORGANIZE_PARTITION)
    {
      if (tab_part_info->use_default_subpartitions &&
          !alt_part_info->use_default_subpartitions)
      {
        tab_part_info->use_default_subpartitions= FALSE;
        tab_part_info->use_default_no_subpartitions= FALSE;
      }
      if (tab_part_info->check_partition_info(thd, (handlerton**)NULL,
                                              table->file, ULL(0)))
      {
        DBUG_RETURN(TRUE);
      }
    }
  }
  else
  {
    /*
     When thd->lex->part_info has a reference to a partition_info the
     ALTER TABLE contained a definition of a partitioning.

     Case I:
       If there was a partition before and there is a new one defined.
       We use the new partitioning. The new partitioning is already
       defined in the correct variable so no work is needed to
       accomplish this.
       We do however need to update partition_changed to ensure that not
       only the frm file is changed in the ALTER TABLE command.

     Case IIa:
       There was a partitioning before and there is no new one defined.
       Also the user has not specified to remove partitioning explicitly.

       We use the old partitioning also for the new table. We do this
       by assigning the partition_info from the table loaded in
       open_ltable to the partition_info struct used by mysql_create_table
       later in this method.

     Case IIb:
       There was a partitioning before and there is no new one defined.
       The user has specified explicitly to remove partitioning

       Since the user has specified explicitly to remove partitioning
       we override the old partitioning info and create a new table using
       the specified engine.
       In this case the partition also is changed.

     Case III:
       There was no partitioning before altering the table, there is
       partitioning defined in the altered table. Use the new partitioning.
       No work needed since the partitioning info is already in the
       correct variable.

       In this case we discover one case where the new partitioning is using
       the same partition function as the default (PARTITION BY KEY or
       PARTITION BY LINEAR KEY with the list of fields equal to the primary
       key fields OR PARTITION BY [LINEAR] KEY() for tables without primary
       key)
       Also here partition has changed and thus a new table must be
       created.

     Case IV:
       There was no partitioning before and no partitioning defined.
       Obviously no work needed.
    */
    if (table->part_info)
    {
      if (alter_info->flags & ALTER_REMOVE_PARTITIONING)
      {
        DBUG_PRINT("info", ("Remove partitioning"));
        if (!(create_info->used_fields & HA_CREATE_USED_ENGINE))
        {
          DBUG_PRINT("info", ("No explicit engine used"));
          create_info->db_type= table->part_info->default_engine_type;
        }
        DBUG_PRINT("info", ("New engine type: %s",
                   hton2plugin[create_info->db_type->slot]->name.str));
        thd->work_part_info= NULL;
        *partition_changed= TRUE;
      }
      else if (!thd->work_part_info)
      {
        /*
          Retain partitioning but possibly with a new storage engine
          beneath.
        */
        thd->work_part_info= table->part_info;
        if (create_info->used_fields & HA_CREATE_USED_ENGINE &&
            create_info->db_type != table->part_info->default_engine_type)
        {
          /*
            Make sure change of engine happens to all partitions.
          */
          DBUG_PRINT("info", ("partition changed"));
          if (table->part_info->is_auto_partitioned)
          {
            /*
              If the user originally didn't specify partitioning to be
              used we can remove it now.
            */
            thd->work_part_info= NULL;
          }
          else
          {
            /*
              Ensure that all partitions have the proper engine set-up
            */
            set_engine_all_partitions(thd->work_part_info,
                                      create_info->db_type);
          }
          *partition_changed= TRUE;
        }
      }
    }
    if (thd->work_part_info)
    {
      partition_info *part_info= thd->work_part_info;
      bool is_native_partitioned= FALSE;
      /*
        Need to cater for engine types that can handle partition without
        using the partition handler.
      */
      if (thd->work_part_info != table->part_info)
      {
        DBUG_PRINT("info", ("partition changed"));
        *partition_changed= TRUE;
      }
      if (create_info->db_type == &partition_hton)
        part_info->default_engine_type= table->part_info->default_engine_type;
      else
        part_info->default_engine_type= create_info->db_type;
      if (check_native_partitioned(create_info, &is_native_partitioned,
                                   part_info, thd))
      {
        DBUG_RETURN(TRUE);
      }
      if (!is_native_partitioned)
      {
        DBUG_ASSERT(create_info->db_type);
        create_info->db_type= &partition_hton;
      }
    }
  }
  DBUG_RETURN(FALSE);
}


/*
  Change partitions, used to implement ALTER TABLE ADD/REORGANIZE/COALESCE
  partitions. This method is used to implement both single-phase and multi-
  phase implementations of ADD/REORGANIZE/COALESCE partitions.

  SYNOPSIS
    mysql_change_partitions()
    lpt                        Struct containing parameters

  RETURN VALUES
    TRUE                          Failure
    FALSE                         Success

  DESCRIPTION
    Request handler to add partitions as set in states of the partition

    Elements of the lpt parameters used:
    create_info                Create information used to create partitions
    db                         Database name
    table_name                 Table name
    copied                     Output parameter where number of copied
                               records are added
    deleted                    Output parameter where number of deleted
                               records are added
*/

static bool mysql_change_partitions(ALTER_PARTITION_PARAM_TYPE *lpt)
{
  char path[FN_REFLEN+1];
  int error;
  handler *file= lpt->table->file;
  DBUG_ENTER("mysql_change_partitions");

  build_table_filename(path, sizeof(path), lpt->db, lpt->table_name, "");
  if ((error= file->change_partitions(lpt->create_info, path, &lpt->copied,
                                      &lpt->deleted, lpt->pack_frm_data,
                                      lpt->pack_frm_len)))
  {
    if (error != ER_OUTOFMEMORY)
      file->print_error(error, MYF(0));
    else
      lpt->thd->fatal_error();
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


/*
  Rename partitions in an ALTER TABLE of partitions

  SYNOPSIS
    mysql_rename_partitions()
    lpt                        Struct containing parameters

  RETURN VALUES
    TRUE                          Failure
    FALSE                         Success

  DESCRIPTION
    Request handler to rename partitions as set in states of the partition

    Parameters used:
    db                         Database name
    table_name                 Table name
*/

static bool mysql_rename_partitions(ALTER_PARTITION_PARAM_TYPE *lpt)
{
  char path[FN_REFLEN+1];
  int error;
  DBUG_ENTER("mysql_rename_partitions");

  build_table_filename(path, sizeof(path), lpt->db, lpt->table_name, "");
  if ((error= lpt->table->file->rename_partitions(path)))
  {
    if (error != 1)
      lpt->table->file->print_error(error, MYF(0));
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}


/*
  Drop partitions in an ALTER TABLE of partitions

  SYNOPSIS
    mysql_drop_partitions()
    lpt                        Struct containing parameters

  RETURN VALUES
    TRUE                          Failure
    FALSE                         Success
  DESCRIPTION
    Drop the partitions marked with PART_TO_BE_DROPPED state and remove
    those partitions from the list.

    Parameters used:
    table                       Table object
    db                          Database name
    table_name                  Table name
*/

static bool mysql_drop_partitions(ALTER_PARTITION_PARAM_TYPE *lpt)
{
  char path[FN_REFLEN+1];
  partition_info *part_info= lpt->table->part_info;
  List_iterator<partition_element> part_it(part_info->partitions);
  uint i= 0;
  uint remove_count= 0;
  int error;
  DBUG_ENTER("mysql_drop_partitions");

  build_table_filename(path, sizeof(path), lpt->db, lpt->table_name, "");
  if ((error= lpt->table->file->drop_partitions(path)))
  {
    lpt->table->file->print_error(error, MYF(0));
    DBUG_RETURN(TRUE);
  }
  do
  {
    partition_element *part_elem= part_it++;
    if (part_elem->part_state == PART_IS_DROPPED)
    {
      part_it.remove();
      remove_count++;
    }
  } while (++i < part_info->no_parts);
  part_info->no_parts-= remove_count;
  DBUG_RETURN(FALSE);
}


/*
  Insert log entry into list
  SYNOPSIS
    insert_part_info_log_entry_list()
    log_entry
  RETURN VALUES
    NONE
*/

static void insert_part_info_log_entry_list(partition_info *part_info,
                                            DDL_LOG_MEMORY_ENTRY *log_entry)
{
  log_entry->next_active_log_entry= part_info->first_log_entry;
  part_info->first_log_entry= log_entry;
}


/*
  Release all log entries for this partition info struct
  SYNOPSIS
    release_part_info_log_entries()
    first_log_entry                 First log entry in list to release
  RETURN VALUES
    NONE
*/

static void release_part_info_log_entries(DDL_LOG_MEMORY_ENTRY *log_entry)
{
  DBUG_ENTER("release_part_info_log_entries");

  while (log_entry)
  {
    release_ddl_log_memory_entry(log_entry);
    log_entry= log_entry->next_active_log_entry;
  }
  DBUG_VOID_RETURN;
}


/*
  Log an delete/rename frm file
  SYNOPSIS
    write_log_replace_delete_frm()
    lpt                            Struct for parameters
    next_entry                     Next reference to use in log record
    from_path                      Name to rename from
    to_path                        Name to rename to
    replace_flag                   TRUE if replace, else delete
  RETURN VALUES
    TRUE                           Error
    FALSE                          Success
  DESCRIPTION
    Support routine that writes a replace or delete of an frm file into the
    ddl log. It also inserts an entry that keeps track of used space into
    the partition info object
*/

static bool write_log_replace_delete_frm(ALTER_PARTITION_PARAM_TYPE *lpt,
                                         uint next_entry,
                                         const char *from_path,
                                         const char *to_path,
                                         bool replace_flag)
{
  DDL_LOG_ENTRY ddl_log_entry;
  DDL_LOG_MEMORY_ENTRY *log_entry;
  DBUG_ENTER("write_log_replace_delete_frm");

  if (replace_flag)
    ddl_log_entry.action_type= DDL_LOG_REPLACE_ACTION;
  else
    ddl_log_entry.action_type= DDL_LOG_DELETE_ACTION;
  ddl_log_entry.next_entry= next_entry;
  ddl_log_entry.handler_name= reg_ext;
  ddl_log_entry.name= to_path;
  if (replace_flag)
    ddl_log_entry.from_name= from_path;
  if (write_ddl_log_entry(&ddl_log_entry, &log_entry))
  {
    DBUG_RETURN(TRUE);
  }
  insert_part_info_log_entry_list(lpt->part_info, log_entry);
  DBUG_RETURN(FALSE);
}


/*
  Log final partition changes in change partition
  SYNOPSIS
    write_log_changed_partitions()
    lpt                      Struct containing parameters
  RETURN VALUES
    TRUE                     Error
    FALSE                    Success
  DESCRIPTION
    This code is used to perform safe ADD PARTITION for HASH partitions
    and COALESCE for HASH partitions and REORGANIZE for any type of
    partitions.
    We prepare entries for all partitions except the reorganised partitions
    in REORGANIZE partition, those are handled by
    write_log_dropped_partitions. For those partitions that are replaced
    special care is needed to ensure that this is performed correctly and
    this requires a two-phased approach with this log as a helper for this.

    This code is closely intertwined with the code in rename_partitions in
    the partition handler.
*/

static bool write_log_changed_partitions(ALTER_PARTITION_PARAM_TYPE *lpt,
                                         uint *next_entry, const char *path)
{
  DDL_LOG_ENTRY ddl_log_entry;
  partition_info *part_info= lpt->part_info;
  DDL_LOG_MEMORY_ENTRY *log_entry;
  char tmp_path[FN_LEN];
  char normal_path[FN_LEN];
  List_iterator<partition_element> part_it(part_info->partitions);
  uint temp_partitions= part_info->temp_partitions.elements;
  uint no_elements= part_info->partitions.elements;
  uint i= 0;
  DBUG_ENTER("write_log_changed_partitions");

  do
  {
    partition_element *part_elem= part_it++;
    if (part_elem->part_state == PART_IS_CHANGED ||
        (part_elem->part_state == PART_IS_ADDED && temp_partitions))
    {
      if (part_info->is_sub_partitioned())
      {
        List_iterator<partition_element> sub_it(part_elem->subpartitions);
        uint no_subparts= part_info->no_subparts;
        uint j= 0;
        do
        {
          partition_element *sub_elem= sub_it++;
          ddl_log_entry.next_entry= *next_entry;
          ddl_log_entry.handler_name=
               ha_resolve_storage_engine_name(sub_elem->engine_type);
          create_subpartition_name(tmp_path, path,
                                   part_elem->partition_name,
                                   sub_elem->partition_name,
                                   TEMP_PART_NAME);
          create_subpartition_name(normal_path, path,
                                   part_elem->partition_name,
                                   sub_elem->partition_name,
                                   NORMAL_PART_NAME);
          ddl_log_entry.name= normal_path;
          ddl_log_entry.from_name= tmp_path;
          if (part_elem->part_state == PART_IS_CHANGED)
            ddl_log_entry.action_type= DDL_LOG_REPLACE_ACTION;
          else
            ddl_log_entry.action_type= DDL_LOG_RENAME_ACTION;
          if (write_ddl_log_entry(&ddl_log_entry, &log_entry))
          {
            DBUG_RETURN(TRUE);
          }
          *next_entry= log_entry->entry_pos;
          sub_elem->log_entry= log_entry;
          insert_part_info_log_entry_list(part_info, log_entry);
        } while (++j < no_subparts);
      }
      else
      {
        ddl_log_entry.next_entry= *next_entry;
        ddl_log_entry.handler_name=
               ha_resolve_storage_engine_name(part_elem->engine_type);
        create_partition_name(tmp_path, path,
                              part_elem->partition_name,
                              TEMP_PART_NAME, TRUE);
        create_partition_name(normal_path, path,
                              part_elem->partition_name,
                              NORMAL_PART_NAME, TRUE);
        ddl_log_entry.name= normal_path;
        ddl_log_entry.from_name= tmp_path;
        if (part_elem->part_state == PART_IS_CHANGED)
          ddl_log_entry.action_type= DDL_LOG_REPLACE_ACTION;
        else
          ddl_log_entry.action_type= DDL_LOG_RENAME_ACTION;
        if (write_ddl_log_entry(&ddl_log_entry, &log_entry))
        {
          DBUG_RETURN(TRUE);
        }
        *next_entry= log_entry->entry_pos;
        part_elem->log_entry= log_entry;
        insert_part_info_log_entry_list(part_info, log_entry);
      }
    }
  } while (++i < no_elements);
  DBUG_RETURN(FALSE);
}


/*
  Log dropped partitions
  SYNOPSIS
    write_log_dropped_partitions()
    lpt                      Struct containing parameters
  RETURN VALUES
    TRUE                     Error
    FALSE                    Success
*/

static bool write_log_dropped_partitions(ALTER_PARTITION_PARAM_TYPE *lpt,
                                         uint *next_entry,
                                         const char *path,
                                         bool temp_list)
{
  DDL_LOG_ENTRY ddl_log_entry;
  partition_info *part_info= lpt->part_info;
  DDL_LOG_MEMORY_ENTRY *log_entry;
  char tmp_path[FN_LEN];
  List_iterator<partition_element> part_it(part_info->partitions);
  List_iterator<partition_element> temp_it(part_info->temp_partitions);
  uint no_temp_partitions= part_info->temp_partitions.elements;
  uint no_elements= part_info->partitions.elements;
  uint i= 0;
  DBUG_ENTER("write_log_dropped_partitions");

  ddl_log_entry.action_type= DDL_LOG_DELETE_ACTION;
  if (temp_list)
    no_elements= no_temp_partitions;
  while (no_elements--)
  {
    partition_element *part_elem;
    if (temp_list)
      part_elem= temp_it++;
    else
      part_elem= part_it++;
    if (part_elem->part_state == PART_TO_BE_DROPPED ||
        part_elem->part_state == PART_TO_BE_ADDED ||
        part_elem->part_state == PART_CHANGED)
    {
      uint name_variant;
      if (part_elem->part_state == PART_CHANGED ||
          (part_elem->part_state == PART_TO_BE_ADDED &&
           no_temp_partitions))
        name_variant= TEMP_PART_NAME;
      else
        name_variant= NORMAL_PART_NAME;
      if (part_info->is_sub_partitioned())
      {
        List_iterator<partition_element> sub_it(part_elem->subpartitions);
        uint no_subparts= part_info->no_subparts;
        uint j= 0;
        do
        {
          partition_element *sub_elem= sub_it++;
          ddl_log_entry.next_entry= *next_entry;
          ddl_log_entry.handler_name=
               ha_resolve_storage_engine_name(sub_elem->engine_type);
          create_subpartition_name(tmp_path, path,
                                   part_elem->partition_name,
                                   sub_elem->partition_name,
                                   name_variant);
          ddl_log_entry.name= tmp_path;
          if (write_ddl_log_entry(&ddl_log_entry, &log_entry))
          {
            DBUG_RETURN(TRUE);
          }
          *next_entry= log_entry->entry_pos;
          sub_elem->log_entry= log_entry;
          insert_part_info_log_entry_list(part_info, log_entry);
        } while (++j < no_subparts);
      }
      else
      {
        ddl_log_entry.next_entry= *next_entry;
        ddl_log_entry.handler_name=
               ha_resolve_storage_engine_name(part_elem->engine_type);
        create_partition_name(tmp_path, path,
                              part_elem->partition_name,
                              name_variant, TRUE);
        ddl_log_entry.name= tmp_path;
        if (write_ddl_log_entry(&ddl_log_entry, &log_entry))
        {
          DBUG_RETURN(TRUE);
        }
        *next_entry= log_entry->entry_pos;
        part_elem->log_entry= log_entry;
        insert_part_info_log_entry_list(part_info, log_entry);
      }
    }
  }
  DBUG_RETURN(FALSE);
}


/*
  Set execute log entry in ddl log for this partitioned table
  SYNOPSIS
    set_part_info_exec_log_entry()
    part_info                      Partition info object
    exec_log_entry                 Log entry
  RETURN VALUES
    NONE
*/

static void set_part_info_exec_log_entry(partition_info *part_info,
                                         DDL_LOG_MEMORY_ENTRY *exec_log_entry)
{
  part_info->exec_log_entry= exec_log_entry;
  exec_log_entry->next_active_log_entry= NULL;
}


/*
  Write the log entry to ensure that the shadow frm file is removed at
  crash.
  SYNOPSIS
    write_log_drop_shadow_frm()
    lpt                      Struct containing parameters
    install_frm              Should we log action to install shadow frm or should
                             the action be to remove the shadow frm file.
  RETURN VALUES
    TRUE                     Error
    FALSE                    Success
  DESCRIPTION
    Prepare an entry to the ddl log indicating a drop/install of the shadow frm
    file and its corresponding handler file.
*/

static bool write_log_drop_shadow_frm(ALTER_PARTITION_PARAM_TYPE *lpt)
{
  DDL_LOG_ENTRY ddl_log_entry;
  partition_info *part_info= lpt->part_info;
  DDL_LOG_MEMORY_ENTRY *log_entry;
  DDL_LOG_MEMORY_ENTRY *exec_log_entry= NULL;
  char shadow_path[FN_LEN];
  DBUG_ENTER("write_log_drop_shadow_frm");

  build_table_filename(shadow_path, sizeof(shadow_path), lpt->db,
                       lpt->table_name, "#");
  pthread_mutex_lock(&LOCK_gdl);
  if (write_log_replace_delete_frm(lpt, 0UL, NULL,
                                  (const char*)shadow_path, FALSE))
    goto error;
  log_entry= part_info->first_log_entry;
  if (write_execute_ddl_log_entry(log_entry->entry_pos,
                                    FALSE, &exec_log_entry))
    goto error;
  pthread_mutex_unlock(&LOCK_gdl);
  set_part_info_exec_log_entry(part_info, exec_log_entry);
  DBUG_RETURN(FALSE);

error:
  release_part_info_log_entries(part_info->first_log_entry);
  pthread_mutex_unlock(&LOCK_gdl);
  part_info->first_log_entry= NULL;
  my_error(ER_DDL_LOG_ERROR, MYF(0));
  DBUG_RETURN(TRUE);
}


/*
  Log renaming of shadow frm to real frm name and dropping of old frm
  SYNOPSIS
    write_log_rename_frm()
    lpt                      Struct containing parameters
  RETURN VALUES
    TRUE                     Error
    FALSE                    Success
  DESCRIPTION
    Prepare an entry to ensure that we complete the renaming of the frm
    file if failure occurs in the middle of the rename process.
*/

static bool write_log_rename_frm(ALTER_PARTITION_PARAM_TYPE *lpt)
{
  DDL_LOG_ENTRY ddl_log_entry;
  partition_info *part_info= lpt->part_info;
  DDL_LOG_MEMORY_ENTRY *log_entry;
  DDL_LOG_MEMORY_ENTRY *exec_log_entry= part_info->exec_log_entry;
  char path[FN_LEN];
  char shadow_path[FN_LEN];
  DDL_LOG_MEMORY_ENTRY *old_first_log_entry= part_info->first_log_entry;
  DBUG_ENTER("write_log_rename_frm");

  part_info->first_log_entry= NULL;
  build_table_filename(path, sizeof(path), lpt->db,
                       lpt->table_name, "");
  build_table_filename(shadow_path, sizeof(shadow_path), lpt->db,
                       lpt->table_name, "#");
  pthread_mutex_lock(&LOCK_gdl);
  if (write_log_replace_delete_frm(lpt, 0UL, shadow_path, path, TRUE))
    goto error;
  log_entry= part_info->first_log_entry;
  part_info->frm_log_entry= log_entry;
  if (write_execute_ddl_log_entry(log_entry->entry_pos,
                                    FALSE, &exec_log_entry))
    goto error;
  release_part_info_log_entries(old_first_log_entry);
  pthread_mutex_unlock(&LOCK_gdl);
  DBUG_RETURN(FALSE);

error:
  release_part_info_log_entries(part_info->first_log_entry);
  pthread_mutex_unlock(&LOCK_gdl);
  part_info->first_log_entry= old_first_log_entry;
  part_info->frm_log_entry= NULL;
  my_error(ER_DDL_LOG_ERROR, MYF(0));
  DBUG_RETURN(TRUE);
}


/*
  Write the log entries to ensure that the drop partition command is completed
  even in the presence of a crash.

  SYNOPSIS
    write_log_drop_partition()
    lpt                      Struct containing parameters
  RETURN VALUES
    TRUE                     Error
    FALSE                    Success
  DESCRIPTION
    Prepare entries to the ddl log indicating all partitions to drop and to
    install the shadow frm file and remove the old frm file.
*/

static bool write_log_drop_partition(ALTER_PARTITION_PARAM_TYPE *lpt)
{
  DDL_LOG_ENTRY ddl_log_entry;
  partition_info *part_info= lpt->part_info;
  DDL_LOG_MEMORY_ENTRY *log_entry;
  DDL_LOG_MEMORY_ENTRY *exec_log_entry= part_info->exec_log_entry;
  char tmp_path[FN_LEN];
  char path[FN_LEN];
  uint next_entry= 0;
  DDL_LOG_MEMORY_ENTRY *old_first_log_entry= part_info->first_log_entry;
  DBUG_ENTER("write_log_drop_partition");

  part_info->first_log_entry= NULL;
  build_table_filename(path, sizeof(path), lpt->db,
                       lpt->table_name, "");
  build_table_filename(tmp_path, sizeof(tmp_path), lpt->db,
                       lpt->table_name, "#");
  pthread_mutex_lock(&LOCK_gdl);
  if (write_log_dropped_partitions(lpt, &next_entry, (const char*)path,
                                   FALSE))
    goto error;
  if (write_log_replace_delete_frm(lpt, next_entry, (const char*)tmp_path,
                                  (const char*)path, TRUE))
    goto error;
  log_entry= part_info->first_log_entry;
  part_info->frm_log_entry= log_entry;
  if (write_execute_ddl_log_entry(log_entry->entry_pos,
                                    FALSE, &exec_log_entry))
    goto error;
  release_part_info_log_entries(old_first_log_entry);
  pthread_mutex_unlock(&LOCK_gdl);
  DBUG_RETURN(FALSE);

error:
  release_part_info_log_entries(part_info->first_log_entry);
  pthread_mutex_unlock(&LOCK_gdl);
  part_info->first_log_entry= old_first_log_entry;
  part_info->frm_log_entry= NULL;
  my_error(ER_DDL_LOG_ERROR, MYF(0));
  DBUG_RETURN(TRUE);
}


/*
  Write the log entries to ensure that the add partition command is not
  executed at all if a crash before it has completed

  SYNOPSIS
    write_log_add_change_partition()
    lpt                      Struct containing parameters
  RETURN VALUES
    TRUE                     Error
    FALSE                    Success
  DESCRIPTION
    Prepare entries to the ddl log indicating all partitions to drop and to
    remove the shadow frm file.
    We always inject entries backwards in the list in the ddl log since we
    don't know the entry position until we have written it.
*/

static bool write_log_add_change_partition(ALTER_PARTITION_PARAM_TYPE *lpt)
{
  partition_info *part_info= lpt->part_info;
  DDL_LOG_MEMORY_ENTRY *log_entry;
  DDL_LOG_MEMORY_ENTRY *exec_log_entry= NULL;
  char tmp_path[FN_LEN];
  char path[FN_LEN];
  uint next_entry= 0;
  DBUG_ENTER("write_log_add_change_partition");

  build_table_filename(path, sizeof(path), lpt->db,
                       lpt->table_name, "");
  build_table_filename(tmp_path, sizeof(tmp_path), lpt->db,
                       lpt->table_name, "#");
  pthread_mutex_lock(&LOCK_gdl);
  if (write_log_dropped_partitions(lpt, &next_entry, (const char*)path,
                                   FALSE))
    goto error;
  if (write_log_replace_delete_frm(lpt, next_entry, NULL, tmp_path,
                                  FALSE))
    goto error;
  log_entry= part_info->first_log_entry;
  if (write_execute_ddl_log_entry(log_entry->entry_pos,
                                    FALSE, &exec_log_entry))
    goto error;
  pthread_mutex_unlock(&LOCK_gdl);
  set_part_info_exec_log_entry(part_info, exec_log_entry);
  DBUG_RETURN(FALSE);

error:
  release_part_info_log_entries(part_info->first_log_entry);
  pthread_mutex_unlock(&LOCK_gdl);
  part_info->first_log_entry= NULL;
  my_error(ER_DDL_LOG_ERROR, MYF(0));
  DBUG_RETURN(TRUE);
}


/*
  Write description of how to complete the operation after first phase of
  change partitions.

  SYNOPSIS
    write_log_final_change_partition()
    lpt                      Struct containing parameters
  RETURN VALUES
    TRUE                     Error
    FALSE                    Success
  DESCRIPTION
    We will write log entries that specify to remove all partitions reorganised,
    to rename others to reflect the new naming scheme and to install the shadow
    frm file.
*/

static bool write_log_final_change_partition(ALTER_PARTITION_PARAM_TYPE *lpt)
{
  DDL_LOG_ENTRY ddl_log_entry;
  partition_info *part_info= lpt->part_info;
  DDL_LOG_MEMORY_ENTRY *log_entry;
  DDL_LOG_MEMORY_ENTRY *exec_log_entry= part_info->exec_log_entry;
  char path[FN_LEN];
  char shadow_path[FN_LEN];
  DDL_LOG_MEMORY_ENTRY *old_first_log_entry= part_info->first_log_entry;
  uint next_entry= 0;
  DBUG_ENTER("write_log_final_change_partition");

  part_info->first_log_entry= NULL;
  build_table_filename(path, sizeof(path), lpt->db,
                       lpt->table_name, "");
  build_table_filename(shadow_path, sizeof(shadow_path), lpt->db,
                       lpt->table_name, "#");
  pthread_mutex_lock(&LOCK_gdl);
  if (write_log_dropped_partitions(lpt, &next_entry, (const char*)path,
                      lpt->alter_info->flags & ALTER_REORGANIZE_PARTITION))
    goto error;
  if (write_log_changed_partitions(lpt, &next_entry, (const char*)path))
    goto error;
  if (write_log_replace_delete_frm(lpt, 0UL, shadow_path, path, TRUE))
    goto error;
  log_entry= part_info->first_log_entry;
  part_info->frm_log_entry= log_entry;
  if (write_execute_ddl_log_entry(log_entry->entry_pos,
                                    FALSE, &exec_log_entry))
    goto error;
  release_part_info_log_entries(old_first_log_entry);
  pthread_mutex_unlock(&LOCK_gdl);
  DBUG_RETURN(FALSE);

error:
  release_part_info_log_entries(part_info->first_log_entry);
  pthread_mutex_unlock(&LOCK_gdl);
  part_info->first_log_entry= old_first_log_entry;
  part_info->frm_log_entry= NULL;
  my_error(ER_DDL_LOG_ERROR, MYF(0));
  DBUG_RETURN(TRUE);
}


/*
  Remove entry from ddl log and release resources for others to use

  SYNOPSIS
    write_log_completed()
    lpt                      Struct containing parameters
  RETURN VALUES
    TRUE                     Error
    FALSE                    Success
*/

static void write_log_completed(ALTER_PARTITION_PARAM_TYPE *lpt,
                                bool dont_crash)
{
  partition_info *part_info= lpt->part_info;
  uint count_loop= 0;
  bool not_success;
  DDL_LOG_MEMORY_ENTRY *log_entry= part_info->exec_log_entry;
  DBUG_ENTER("write_log_completed");

  DBUG_ASSERT(log_entry);
  pthread_mutex_lock(&LOCK_gdl);
  if (write_execute_ddl_log_entry(0UL, TRUE, &log_entry))
  {
    /*
      Failed to write, Bad...
      We have completed the operation but have log records to REMOVE
      stuff that shouldn't be removed. What clever things could one do
      here? An error output was written to the error output by the
      above method so we don't do anything here.
    */
    ;
  }
  release_part_info_log_entries(part_info->first_log_entry);
  release_part_info_log_entries(part_info->exec_log_entry);
  pthread_mutex_unlock(&LOCK_gdl);
  part_info->exec_log_entry= NULL;
  part_info->first_log_entry= NULL;
  DBUG_VOID_RETURN;
}


/*
   Release all log entries
   SYNOPSIS
     release_log_entries()
     part_info                  Partition info struct
   RETURN VALUES
     NONE
*/

static void release_log_entries(partition_info *part_info)
{
  pthread_mutex_lock(&LOCK_gdl);
  release_part_info_log_entries(part_info->first_log_entry);
  release_part_info_log_entries(part_info->exec_log_entry);
  pthread_mutex_unlock(&LOCK_gdl);
  part_info->first_log_entry= NULL;
  part_info->exec_log_entry= NULL;
}


/*
  Get a lock on table name to avoid that anyone can open the table in
  a critical part of the ALTER TABLE.
  SYNOPSIS
    get_name_lock()
    lpt                        Struct carrying parameters
  RETURN VALUES
    FALSE                      Success
    TRUE                       Failure
*/

static int get_name_lock(ALTER_PARTITION_PARAM_TYPE *lpt)
{
  int error= 0;
  DBUG_ENTER("get_name_lock");

  bzero(&lpt->table_list, sizeof(lpt->table_list));
  lpt->table_list.db= (char*)lpt->db;
  lpt->table_list.table= lpt->table;
  lpt->table_list.table_name= (char*)lpt->table_name;
  pthread_mutex_lock(&LOCK_open);
  error= lock_table_name(lpt->thd, &lpt->table_list, FALSE);
  pthread_mutex_unlock(&LOCK_open);
  DBUG_RETURN(error);
}


/*
  Unlock and close table before renaming and dropping partitions
  SYNOPSIS
    alter_close_tables()
    lpt                        Struct carrying parameters
  RETURN VALUES
    0
*/

static int alter_close_tables(ALTER_PARTITION_PARAM_TYPE *lpt)
{
  THD *thd= lpt->thd;
  TABLE *table= lpt->table;
  DBUG_ENTER("alter_close_tables");
  /*
    We need to also unlock tables and close all handlers.
    We set lock to zero to ensure we don't do this twice
    and we set db_stat to zero to ensure we don't close twice.
  */
  mysql_unlock_tables(thd, thd->lock);
  thd->lock= 0;
  table->file->close();
  table->db_stat= 0;
  DBUG_RETURN(0);
}


/*
  Release a lock name
  SYNOPSIS
    release_name_lock()
    lpt
  RETURN VALUES
    0
*/

static int release_name_lock(ALTER_PARTITION_PARAM_TYPE *lpt)
{
  DBUG_ENTER("release_name_lock");
  pthread_mutex_lock(&LOCK_open);
  unlock_table_name(lpt->thd, &lpt->table_list);
  pthread_mutex_unlock(&LOCK_open);
  DBUG_RETURN(0);
}


/*
  Handle errors for ALTER TABLE for partitioning
  SYNOPSIS
    handle_alter_part_error()
    lpt                        Struct carrying parameters
    not_completed              Was request in complete phase when error occurred
  RETURN VALUES
    NONE
*/

void handle_alter_part_error(ALTER_PARTITION_PARAM_TYPE *lpt,
                             bool not_completed,
                             bool drop_partition,
                             bool frm_install)
{
  partition_info *part_info= lpt->part_info;
  DBUG_ENTER("handle_alter_part_error");

  if (!part_info->first_log_entry &&
      execute_ddl_log_entry(current_thd,
                            part_info->first_log_entry->entry_pos))
  {
    /*
      We couldn't recover from error, most likely manual interaction
      is required.
    */
    write_log_completed(lpt, FALSE);
    release_log_entries(part_info);
    if (not_completed)
    {
      if (drop_partition)
      {
        /* Table is still ok, but we left a shadow frm file behind. */
        push_warning_printf(lpt->thd, MYSQL_ERROR::WARN_LEVEL_WARN, 1,
                            "%s %s",
           "Operation was unsuccessful, table is still intact,",
           "but it is possible that a shadow frm file was left behind");
      }
      else
      {
        push_warning_printf(lpt->thd, MYSQL_ERROR::WARN_LEVEL_WARN, 1,
                            "%s %s %s %s",
           "Operation was unsuccessful, table is still intact,",
           "but it is possible that a shadow frm file was left behind.",
           "It is also possible that temporary partitions are left behind,",
           "these could be empty or more or less filled with records");
      }
    }
    else
    {
      if (frm_install)
      {
        /*
           Failed during install of shadow frm file, table isn't intact
           and dropped partitions are still there
        */
        push_warning_printf(lpt->thd, MYSQL_ERROR::WARN_LEVEL_WARN, 1,
                            "%s %s %s",
          "Failed during alter of partitions, table is no longer intact.",
          "The frm file is in an unknown state, and a backup",
          "is required.");
      }
      else if (drop_partition)
      {
        /*
          Table is ok, we have switched to new table but left dropped
          partitions still in their places. We remove the log records and
          ask the user to perform the action manually. We remove the log
          records and ask the user to perform the action manually.
        */
        push_warning_printf(lpt->thd, MYSQL_ERROR::WARN_LEVEL_WARN, 1,
                            "%s %s",
              "Failed during drop of partitions, table is intact.",
              "Manual drop of remaining partitions is required");
      }
      else
      {
        /*
          We failed during renaming of partitions. The table is most
          certainly in a very bad state so we give user warning and disable
          the table by writing an ancient frm version into it.
        */
        push_warning_printf(lpt->thd, MYSQL_ERROR::WARN_LEVEL_WARN, 1,
                            "%s %s %s",
           "Failed during renaming of partitions. We are now in a position",
           "where table is not reusable",
           "Table is disabled by writing ancient frm file version into it");
      }
    }
  }
  else
  {
    release_log_entries(part_info);
    if (not_completed)
    {
      /*
        We hit an error before things were completed but managed
        to recover from the error. An error occurred and we have
        restored things to original so no need for further action.
      */
      ;
    }
    else
    {
      /*
        We hit an error after we had completed most of the operation
        and were successful in a second attempt so the operation
        actually is successful now. We need to issue a warning that
        even though we reported an error the operation was successfully
        completed.
      */
      push_warning_printf(lpt->thd, MYSQL_ERROR::WARN_LEVEL_WARN, 1,"%s %s",
         "Operation was successfully completed by failure handling,",
         "after failure of normal operation");
    }
  }
  DBUG_VOID_RETURN;
}


/*
  Actually perform the change requested by ALTER TABLE of partitions
  previously prepared.

  SYNOPSIS
    fast_alter_partition_table()
    thd                           Thread object
    table                         Table object
    alter_info                    ALTER TABLE info
    create_info                   Create info for CREATE TABLE
    table_list                    List of the table involved
    create_list                   The fields in the resulting table
    key_list                      The keys in the resulting table
    db                            Database name of new table
    table_name                    Table name of new table

  RETURN VALUES
    TRUE                          Error
    FALSE                         Success

  DESCRIPTION
    Perform all ALTER TABLE operations for partitioned tables that can be
    performed fast without a full copy of the original table.
*/

uint fast_alter_partition_table(THD *thd, TABLE *table,
                                ALTER_INFO *alter_info,
                                HA_CREATE_INFO *create_info,
                                TABLE_LIST *table_list,
                                List<create_field> *create_list,
                                List<Key> *key_list, char *db,
                                const char *table_name,
                                uint fast_alter_partition)
{
  /* Set-up struct used to write frm files */
  ulonglong copied= 0;
  ulonglong deleted= 0;
  partition_info *part_info= table->part_info;
  ALTER_PARTITION_PARAM_TYPE lpt_obj;
  ALTER_PARTITION_PARAM_TYPE *lpt= &lpt_obj;
  bool written_bin_log= TRUE;
  bool not_completed= TRUE;
  bool frm_install= FALSE;
  DBUG_ENTER("fast_alter_partition_table");

  lpt->thd= thd;
  lpt->part_info= part_info;
  lpt->alter_info= alter_info;
  lpt->create_info= create_info;
  lpt->create_list= create_list;
  lpt->key_list= key_list;
  lpt->db_options= create_info->table_options;
  if (create_info->row_type == ROW_TYPE_DYNAMIC)
    lpt->db_options|= HA_OPTION_PACK_RECORD;
  lpt->table= table;
  lpt->key_info_buffer= 0;
  lpt->key_count= 0;
  lpt->db= db;
  lpt->table_name= table_name;
  lpt->copied= 0;
  lpt->deleted= 0;
  lpt->pack_frm_data= NULL;
  lpt->pack_frm_len= 0;
  thd->work_part_info= part_info;

  if (alter_info->flags & ALTER_OPTIMIZE_PARTITION ||
      alter_info->flags & ALTER_ANALYZE_PARTITION ||
      alter_info->flags & ALTER_CHECK_PARTITION ||
      alter_info->flags & ALTER_REPAIR_PARTITION)
  {
    /*
      In this case the user has specified that he wants a set of partitions
      to be optimised and the partition engine can handle optimising
      partitions natively without requiring a full rebuild of the
      partitions.

      In this case it is enough to call optimise_partitions, there is no
      need to change frm files or anything else.
    */
    int error;
    written_bin_log= FALSE;
    if (((alter_info->flags & ALTER_OPTIMIZE_PARTITION) &&
         (error= table->file->optimize_partitions(thd))) ||
        ((alter_info->flags & ALTER_ANALYZE_PARTITION) &&
         (error= table->file->analyze_partitions(thd))) ||
        ((alter_info->flags & ALTER_CHECK_PARTITION) &&
         (error= table->file->check_partitions(thd))) ||
        ((alter_info->flags & ALTER_REPAIR_PARTITION) &&
         (error= table->file->repair_partitions(thd))))
    {
      table->file->print_error(error, MYF(0));
      DBUG_RETURN(TRUE);
    }
  }
  else if (fast_alter_partition & HA_PARTITION_ONE_PHASE)
  {
    /*
      In the case where the engine supports one phase online partition
      changes it is not necessary to have any exclusive locks. The
      correctness is upheld instead by transactions being aborted if they
      access the table after its partition definition has changed (if they
      are still using the old partition definition).

      The handler is in this case responsible to ensure that all users
      start using the new frm file after it has changed. To implement
      one phase it is necessary for the handler to have the master copy
      of the frm file and use discovery mechanisms to renew it. Thus
      write frm will write the frm, pack the new frm and finally
      the frm is deleted and the discovery mechanisms will either restore
      back to the old or installing the new after the change is activated.

      Thus all open tables will be discovered that they are old, if not
      earlier as soon as they try an operation using the old table. One
      should ensure that this is checked already when opening a table,
      even if it is found in the cache of open tables.

      change_partitions will perform all operations and it is the duty of
      the handler to ensure that the frm files in the system gets updated
      in synch with the changes made and if an error occurs that a proper
      error handling is done.

      If the MySQL Server crashes at this moment but the handler succeeds
      in performing the change then the binlog is not written for the
      change. There is no way to solve this as long as the binlog is not
      transactional and even then it is hard to solve it completely.
 
      The first approach here was to downgrade locks. Now a different approach
      is decided upon. The idea is that the handler will have access to the
      ALTER_INFO when store_lock arrives with TL_WRITE_ALLOW_READ. So if the
      handler knows that this functionality can be handled with a lower lock
      level it will set the lock level to TL_WRITE_ALLOW_WRITE immediately.
      Thus the need to downgrade the lock disappears.
      1) Write the new frm, pack it and then delete it
      2) Perform the change within the handler
    */
    if (mysql_write_frm(lpt, WFRM_WRITE_SHADOW | WFRM_PACK_FRM) ||
        mysql_change_partitions(lpt))
    {
      DBUG_RETURN(TRUE);
    }
  }
  else if (alter_info->flags == ALTER_DROP_PARTITION)
  {
    /*
      Now after all checks and setting state on dropped partitions we can
      start the actual dropping of the partitions.

      Drop partition is actually two things happening. The first is that
      a lot of records are deleted. The second is that the behaviour of
      subsequent updates and writes and deletes will change. The delete
      part can be handled without any particular high lock level by
      transactional engines whereas non-transactional engines need to
      ensure that this change is done with an exclusive lock on the table.
      The second part, the change of partitioning does however require
      an exclusive lock to install the new partitioning as one atomic
      operation. If this is not the case, it is possible for two
      transactions to see the change in a different order than their
      serialisation order. Thus we need an exclusive lock for both
      transactional and non-transactional engines.

      For LIST partitions it could be possible to avoid the exclusive lock
      (and for RANGE partitions if they didn't rearrange range definitions
      after a DROP PARTITION) if one ensured that failed accesses to the
      dropped partitions was aborted for sure (thus only possible for
      transactional engines).

      0) Write an entry that removes the shadow frm file if crash occurs 
      1) Write the new frm file as a shadow frm
      2) Write the ddl log to ensure that the operation is completed
         even in the presence of a MySQL Server crash
      3) Lock the table in TL_WRITE_ONLY to ensure all other accesses to
         the table have completed. This ensures that other threads can not
         execute on the table in parallel.
      4) Get a name lock on the table. This ensures that we can release all
         locks on the table and since no one can open the table, there can
         be no new threads accessing the table. They will be hanging on the
         name lock.
      5) Close all tables that have already been opened but didn't stumble on
         the abort locked previously. This is done as part of the
         get_name_lock call.
      6) We are now ready to release all locks we got in this thread.
      7) Write the bin log
         Unfortunately the writing of the binlog is not synchronised with
         other logging activities. So no matter in which order the binlog
         is written compared to other activities there will always be cases
         where crashes make strange things occur. In this placement it can
         happen that the ALTER TABLE DROP PARTITION gets performed in the
         master but not in the slaves if we have a crash, after writing the
         ddl log but before writing the binlog. A solution to this would
         require writing the statement first in the ddl log and then
         when recovering from the crash read the binlog and insert it into
         the binlog if not written already.
      8) Install the previously written shadow frm file
      9) Prepare handlers for drop of partitions
      10) Drop the partitions
      11) Remove entries from ddl log
      12) Release name lock so that all other threads can access the table
          again.
      13) Complete query

      We insert Error injections at all places where it could be interesting
      to test if recovery is properly done.
    */
    if (write_log_drop_shadow_frm(lpt) ||
        ERROR_INJECT_CRASH("crash_drop_partition_1") ||
        mysql_write_frm(lpt, WFRM_WRITE_SHADOW) ||
        ERROR_INJECT_CRASH("crash_drop_partition_2") ||
        write_log_drop_partition(lpt) ||
        ERROR_INJECT_CRASH("crash_drop_partition_3") ||
        (not_completed= FALSE) ||
        abort_and_upgrade_lock(lpt) || /* Always returns 0 */
        ERROR_INJECT_CRASH("crash_drop_partition_4") ||
        get_name_lock(lpt) ||
        ERROR_INJECT_CRASH("crash_drop_partition_5") ||
        alter_close_tables(lpt) ||
        ERROR_INJECT_CRASH("crash_drop_partition_6") ||
        ((!thd->lex->no_write_to_binlog) &&
         (write_bin_log(thd, FALSE,
                        thd->query, thd->query_length), FALSE)) ||
        ERROR_INJECT_CRASH("crash_drop_partition_7") ||
        ((frm_install= TRUE), FALSE) ||
        mysql_write_frm(lpt, WFRM_INSTALL_SHADOW) ||
        ((frm_install= FALSE), FALSE) ||
        ERROR_INJECT_CRASH("crash_drop_partition_8") ||
        mysql_drop_partitions(lpt) ||
        ERROR_INJECT_CRASH("crash_drop_partition_9") ||
        (write_log_completed(lpt, FALSE), FALSE) ||
        ERROR_INJECT_CRASH("crash_drop_partition_10") ||
        (release_name_lock(lpt), FALSE)) 
    {
      handle_alter_part_error(lpt, not_completed, TRUE, frm_install);
      DBUG_RETURN(TRUE);
    }
  }
  else if ((alter_info->flags & ALTER_ADD_PARTITION) &&
           (part_info->part_type == RANGE_PARTITION ||
            part_info->part_type == LIST_PARTITION))
  {
    /*
      ADD RANGE/LIST PARTITIONS
      In this case there are no tuples removed and no tuples are added.
      Thus the operation is merely adding a new partition. Thus it is
      necessary to perform the change as an atomic operation. Otherwise
      someone reading without seeing the new partition could potentially
      miss updates made by a transaction serialised before it that are
      inserted into the new partition.

      0) Write an entry that removes the shadow frm file if crash occurs 
      1) Write the new frm file as a shadow frm file
      2) Log the changes to happen in ddl log
      2) Add the new partitions
      3) Lock all partitions in TL_WRITE_ONLY to ensure that no users
         are still using the old partitioning scheme. Wait until all
         ongoing users have completed before progressing.
      4) Get a name lock on the table. This ensures that we can release all
         locks on the table and since no one can open the table, there can
         be no new threads accessing the table. They will be hanging on the
         name lock.
      5) Close all tables that have already been opened but didn't stumble on
         the abort locked previously. This is done as part of the
         get_name_lock call.
      6) Close all table handlers and unlock all handlers but retain name lock
      7) Write binlog
      8) Now the change is completed except for the installation of the
         new frm file. We thus write an action in the log to change to
         the shadow frm file
      9) Install the new frm file of the table where the partitions are
         added to the table.
      10)Wait until all accesses using the old frm file has completed
      11)Remove entries from ddl log
      12)Release name lock
      13)Complete query
    */
    if (write_log_add_change_partition(lpt) ||
        ERROR_INJECT_CRASH("crash_add_partition_1") ||
        mysql_write_frm(lpt, WFRM_WRITE_SHADOW) ||
        ERROR_INJECT_CRASH("crash_add_partition_2") ||
        mysql_change_partitions(lpt) ||
        ERROR_INJECT_CRASH("crash_add_partition_3") ||
        abort_and_upgrade_lock(lpt) || /* Always returns 0 */
        ERROR_INJECT_CRASH("crash_add_partition_3") ||
        get_name_lock(lpt) ||
        ERROR_INJECT_CRASH("crash_add_partition_4") ||
        alter_close_tables(lpt) ||
        ERROR_INJECT_CRASH("crash_add_partition_5") ||
        ((!thd->lex->no_write_to_binlog) &&
         (write_bin_log(thd, FALSE,
                        thd->query, thd->query_length), FALSE)) ||
        ERROR_INJECT_CRASH("crash_add_partition_6") ||
        write_log_rename_frm(lpt) ||
        (not_completed= FALSE) ||
        ERROR_INJECT_CRASH("crash_add_partition_7") ||
        ((frm_install= TRUE), FALSE) ||
        mysql_write_frm(lpt, WFRM_INSTALL_SHADOW) ||
        ERROR_INJECT_CRASH("crash_add_partition_8") ||
        (write_log_completed(lpt, FALSE), FALSE) ||
        ERROR_INJECT_CRASH("crash_add_partition_9") ||
        (release_name_lock(lpt), FALSE)) 
    {
      handle_alter_part_error(lpt, not_completed, FALSE, frm_install);
      DBUG_RETURN(TRUE);
    }
  }
  else
  {
    /*
      ADD HASH PARTITION/
      COALESCE PARTITION/
      REBUILD PARTITION/
      REORGANIZE PARTITION
 
      In this case all records are still around after the change although
      possibly organised into new partitions, thus by ensuring that all
      updates go to both the old and the new partitioning scheme we can
      actually perform this operation lock-free. The only exception to
      this is when REORGANIZE PARTITION adds/drops ranges. In this case
      there needs to be an exclusive lock during the time when the range
      changes occur.
      This is only possible if the handler can ensure double-write for a
      period. The double write will ensure that it doesn't matter where the
      data is read from since both places are updated for writes. If such
      double writing is not performed then it is necessary to perform the
      change with the usual exclusive lock. With double writes it is even
      possible to perform writes in parallel with the reorganisation of
      partitions.

      Without double write procedure we get the following procedure.
      The only difference with using double write is that we can downgrade
      the lock to TL_WRITE_ALLOW_WRITE. Double write in this case only
      double writes from old to new. If we had double writing in both
      directions we could perform the change completely without exclusive
      lock for HASH partitions.
      Handlers that perform double writing during the copy phase can actually
      use a lower lock level. This can be handled inside store_lock in the
      respective handler.

      0) Write an entry that removes the shadow frm file if crash occurs 
      1) Write the shadow frm file of new partitioning
      2) Log such that temporary partitions added in change phase are
         removed in a crash situation
      3) Add the new partitions
         Copy from the reorganised partitions to the new partitions
      4) Log that operation is completed and log all complete actions
         needed to complete operation from here
      5) Lock all partitions in TL_WRITE_ONLY to ensure that no users
         are still using the old partitioning scheme. Wait until all
         ongoing users have completed before progressing.
      6) Get a name lock of the table
      7) Close all tables opened but not yet locked, after this call we are
         certain that no other thread is in the lock wait queue or has
         opened the table. The name lock will ensure that they are blocked
         on the open call. This is achieved also by get_name_lock call.
      8) Close all partitions opened by this thread, but retain name lock.
      9) Write bin log
      10) Prepare handlers for rename and delete of partitions
      11) Rename and drop the reorged partitions such that they are no
          longer used and rename those added to their real new names.
      12) Install the shadow frm file
      13) Release the name lock to enable other threads to start using the
          table again.
      14) Complete query
    */
    if (write_log_add_change_partition(lpt) ||
        ERROR_INJECT_CRASH("crash_change_partition_1") ||
        mysql_write_frm(lpt, WFRM_WRITE_SHADOW) ||
        ERROR_INJECT_CRASH("crash_change_partition_2") ||
        mysql_change_partitions(lpt) ||
        ERROR_INJECT_CRASH("crash_change_partition_3") ||
        write_log_final_change_partition(lpt) ||
        ERROR_INJECT_CRASH("crash_change_partition_4") ||
        (not_completed= FALSE) ||
        abort_and_upgrade_lock(lpt) || /* Always returns 0 */
        ERROR_INJECT_CRASH("crash_change_partition_5") ||
        get_name_lock(lpt) ||
        ERROR_INJECT_CRASH("crash_change_partition_6") ||
        alter_close_tables(lpt) ||
        ERROR_INJECT_CRASH("crash_change_partition_7") ||
        ((!thd->lex->no_write_to_binlog) &&
         (write_bin_log(thd, FALSE,
                        thd->query, thd->query_length), FALSE)) ||
        ERROR_INJECT_CRASH("crash_change_partition_8") ||
        mysql_write_frm(lpt, WFRM_INSTALL_SHADOW) ||
        ERROR_INJECT_CRASH("crash_change_partition_9") ||
        mysql_drop_partitions(lpt) ||
        ERROR_INJECT_CRASH("crash_change_partition_10") ||
        mysql_rename_partitions(lpt) ||
        ((frm_install= TRUE), FALSE) ||
        ERROR_INJECT_CRASH("crash_change_partition_11") ||
        (write_log_completed(lpt, FALSE), FALSE) ||
        ERROR_INJECT_CRASH("crash_change_partition_12") ||
        (release_name_lock(lpt), FALSE))
    {
      handle_alter_part_error(lpt, not_completed, FALSE, frm_install);
      DBUG_RETURN(TRUE);
    }
  }
  /*
    A final step is to write the query to the binlog and send ok to the
    user
  */
  DBUG_RETURN(fast_end_partition(thd, lpt->copied, lpt->deleted,
                                 table, table_list, FALSE, lpt,
                                 written_bin_log));
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
  DBUG_ENTER("set_field_ptr");

  do
  {
    (*ptr)->move_field_offset(diff);
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
    key_info            key info with a set of fields to change ptr
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
  uint key_parts= key_info->key_parts;
  uint i= 0;
  my_ptrdiff_t diff= (new_buf - old_buf);
  DBUG_ENTER("set_key_field_ptr");

  do
  {
    key_part->field->move_field_offset(diff);
    key_part++;
  } while (++i < key_parts);
  DBUG_VOID_RETURN;
}


/*
  SYNOPSIS
    mem_alloc_error()
    size                Size of memory attempted to allocate
    None

  RETURN VALUES
    None

  DESCRIPTION
    A routine to use for all the many places in the code where memory
    allocation error can happen, a tremendous amount of them, needs
    simple routine that signals this error.
*/

void mem_alloc_error(size_t size)
{
  my_error(ER_OUTOFMEMORY, MYF(0), size);
}

#ifdef WITH_PARTITION_STORAGE_ENGINE
/*
  Return comma-separated list of used partitions in the provided given string

  SYNOPSIS
    make_used_partitions_str()
      part_info  IN  Partitioning info
      parts_str  OUT The string to fill

  DESCRIPTION
    Generate a list of used partitions (from bits in part_info->used_partitions
    bitmap), asd store it into the provided String object.
    
  NOTE
    The produced string must not be longer then MAX_PARTITIONS * (1 + FN_LEN).
*/

void make_used_partitions_str(partition_info *part_info, String *parts_str)
{
  parts_str->length(0);
  partition_element *pe;
  uint partition_id= 0;
  List_iterator<partition_element> it(part_info->partitions);
  
  if (part_info->is_sub_partitioned())
  {
    partition_element *head_pe;
    while ((head_pe= it++))
    {
      List_iterator<partition_element> it2(head_pe->subpartitions);
      while ((pe= it2++))
      {
        if (bitmap_is_set(&part_info->used_partitions, partition_id))
        {
          if (parts_str->length())
            parts_str->append(',');
          parts_str->append(head_pe->partition_name,
                           strlen(head_pe->partition_name),
                           system_charset_info);
          parts_str->append('_');
          parts_str->append(pe->partition_name,
                           strlen(pe->partition_name),
                           system_charset_info);
        }
        partition_id++;
      }
    }
  }
  else
  {
    while ((pe= it++))
    {
      if (bitmap_is_set(&part_info->used_partitions, partition_id))
      {
        if (parts_str->length())
          parts_str->append(',');
        parts_str->append(pe->partition_name, strlen(pe->partition_name),
                         system_charset_info);
      }
      partition_id++;
    }
  }
}
#endif

/****************************************************************************
 * Partition interval analysis support
 ***************************************************************************/

/*
  Setup partition_info::* members related to partitioning range analysis

  SYNOPSIS
    set_up_partition_func_pointers()
      part_info  Partitioning info structure

  DESCRIPTION
    Assuming that passed partition_info structure already has correct values
    for members that specify [sub]partitioning type, table fields, and
    functions, set up partition_info::* members that are related to
    Partitioning Interval Analysis (see get_partitions_in_range_iter for its
    definition)

  IMPLEMENTATION
    There are two available interval analyzer functions:
    (1) get_part_iter_for_interval_via_mapping 
    (2) get_part_iter_for_interval_via_walking

    They both have limited applicability:
    (1) is applicable for "PARTITION BY <RANGE|LIST>(func(t.field))", where
    func is a monotonic function.
    
    (2) is applicable for 
      "[SUB]PARTITION BY <any-partitioning-type>(any_func(t.integer_field))"
      
    If both are applicable, (1) is preferred over (2).
    
    This function sets part_info::get_part_iter_for_interval according to
    this criteria, and also sets some auxilary fields that the function
    uses.
*/
#ifdef WITH_PARTITION_STORAGE_ENGINE
static void set_up_range_analysis_info(partition_info *part_info)
{
  enum_monotonicity_info minfo;

  /* Set the catch-all default */
  part_info->get_part_iter_for_interval= NULL;
  part_info->get_subpart_iter_for_interval= NULL;

  /* 
    Check if get_part_iter_for_interval_via_mapping() can be used for 
    partitioning
  */
  switch (part_info->part_type) {
  case RANGE_PARTITION:
  case LIST_PARTITION:
    minfo= part_info->part_expr->get_monotonicity_info();
    if (minfo != NON_MONOTONIC)
    {
      part_info->range_analysis_include_bounds=
        test(minfo == MONOTONIC_INCREASING);
      part_info->get_part_iter_for_interval=
        get_part_iter_for_interval_via_mapping;
      goto setup_subparts;
    }
  default:
    ;
  }
   
  /*
    Check if get_part_iter_for_interval_via_walking() can be used for
    partitioning
  */
  if (part_info->no_part_fields == 1)
  {
    Field *field= part_info->part_field_array[0];
    switch (field->type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
      part_info->get_part_iter_for_interval=
        get_part_iter_for_interval_via_walking;
      break;
    default:
      ;
    }
  }

setup_subparts:
  /*
    Check if get_part_iter_for_interval_via_walking() can be used for
    subpartitioning
  */
  if (part_info->no_subpart_fields == 1)
  {
    Field *field= part_info->subpart_field_array[0];
    switch (field->type()) {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
      part_info->get_subpart_iter_for_interval=
        get_part_iter_for_interval_via_walking;
      break;
    default:
      ;
    }
  }
}


typedef uint32 (*get_endpoint_func)(partition_info*, bool left_endpoint,
                                    bool include_endpoint);

/*
  Partitioning Interval Analysis: Initialize the iterator for "mapping" case

  SYNOPSIS
    get_part_iter_for_interval_via_mapping()
      part_info   Partition info
      is_subpart  TRUE  - act for subpartitioning
                  FALSE - act for partitioning
      min_value   minimum field value, in opt_range key format.
      max_value   minimum field value, in opt_range key format.
      flags       Some combination of NEAR_MIN, NEAR_MAX, NO_MIN_RANGE,
                  NO_MAX_RANGE.
      part_iter   Iterator structure to be initialized

  DESCRIPTION
    Initialize partition set iterator to walk over the interval in
    ordered-array-of-partitions (for RANGE partitioning) or 
    ordered-array-of-list-constants (for LIST partitioning) space.

  IMPLEMENTATION
    This function is used when partitioning is done by
    <RANGE|LIST>(ascending_func(t.field)), and we can map an interval in
    t.field space into a sub-array of partition_info::range_int_array or
    partition_info::list_array (see get_partition_id_range_for_endpoint,
    get_list_array_idx_for_endpoint for details).
    
    The function performs this interval mapping, and sets the iterator to
    traverse the sub-array and return appropriate partitions.
    
  RETURN
    0 - No matching partitions (iterator not initialized)
    1 - Ok, iterator intialized for traversal of matching partitions.
   -1 - All partitions would match (iterator not initialized)
*/

int get_part_iter_for_interval_via_mapping(partition_info *part_info,
                                           bool is_subpart,
                                           char *min_value, char *max_value,
                                           uint flags,
                                           PARTITION_ITERATOR *part_iter)
{
  DBUG_ASSERT(!is_subpart);
  Field *field= part_info->part_field_array[0];
  uint32             max_endpoint_val;
  get_endpoint_func  get_endpoint;
  uint field_len= field->pack_length_in_rec();

  if (part_info->part_type == RANGE_PARTITION)
  {
    get_endpoint=        get_partition_id_range_for_endpoint;
    max_endpoint_val=    part_info->no_parts;
    part_iter->get_next= get_next_partition_id_range;
  }
  else if (part_info->part_type == LIST_PARTITION)
  {
    get_endpoint=        get_list_array_idx_for_endpoint;
    max_endpoint_val=    part_info->no_list_values;
    part_iter->get_next= get_next_partition_id_list;
    part_iter->part_info= part_info;
    part_iter->ret_null_part= part_iter->ret_null_part_orig= FALSE;
    if (max_endpoint_val == 0)
    {
      /*
        We handle this special case without optimisations since it is
        of little practical value but causes a great number of complex
        checks later in the code.
      */
      part_iter->part_nums.start= part_iter->part_nums.end= 0;
      part_iter->part_nums.cur= 0;
      part_iter->ret_null_part= part_iter->ret_null_part_orig= TRUE;
      return -1;
    }
  }
  else
    DBUG_ASSERT(0);

  /* 
    Find minimum: Do special handling if the interval has left bound in form
     " NULL <= X ":
  */
  if (field->real_maybe_null() && part_info->has_null_value && 
      !(flags & (NO_MIN_RANGE | NEAR_MIN)) && *min_value)
  {
    part_iter->ret_null_part= part_iter->ret_null_part_orig= TRUE;
    part_iter->part_nums.start= part_iter->part_nums.cur= 0;
    if (*max_value && !(flags & NO_MAX_RANGE))
    {
      /* The right bound is X <= NULL, i.e. it is a "X IS NULL" interval */
      part_iter->part_nums.end= 0;
      return 1;
    }
  }
  else
  {
    if (flags & NO_MIN_RANGE)
      part_iter->part_nums.start= part_iter->part_nums.cur= 0;
    else
    {
      /*
        Store the interval edge in the record buffer, and call the
        function that maps the edge in table-field space to an edge
        in ordered-set-of-partitions (for RANGE partitioning) or 
        index-in-ordered-array-of-list-constants (for LIST) space.
      */
      store_key_image_to_rec(field, min_value, field_len);
      bool include_endp= part_info->range_analysis_include_bounds ||
                         !test(flags & NEAR_MIN);
      part_iter->part_nums.start= get_endpoint(part_info, 1, include_endp);
      part_iter->part_nums.cur= part_iter->part_nums.start;
      if (part_iter->part_nums.start == max_endpoint_val)
        return 0; /* No partitions */
    }
  }

  /* Find maximum, do the same as above but for right interval bound */
  if (flags & NO_MAX_RANGE)
    part_iter->part_nums.end= max_endpoint_val;
  else
  {
    store_key_image_to_rec(field, max_value, field_len);
    bool include_endp= part_info->range_analysis_include_bounds ||
                       !test(flags & NEAR_MAX);
    part_iter->part_nums.end= get_endpoint(part_info, 0, include_endp);
    if (part_iter->part_nums.start == part_iter->part_nums.end &&
        !part_iter->ret_null_part)
      return 0; /* No partitions */
  }
  return 1; /* Ok, iterator initialized */
}


/* See get_part_iter_for_interval_via_walking for definition of what this is */
#define MAX_RANGE_TO_WALK 10


/*
  Partitioning Interval Analysis: Initialize iterator to walk field interval

  SYNOPSIS
    get_part_iter_for_interval_via_walking()
      part_info   Partition info
      is_subpart  TRUE  - act for subpartitioning
                  FALSE - act for partitioning
      min_value   minimum field value, in opt_range key format.
      max_value   minimum field value, in opt_range key format.
      flags       Some combination of NEAR_MIN, NEAR_MAX, NO_MIN_RANGE,
                  NO_MAX_RANGE.
      part_iter   Iterator structure to be initialized

  DESCRIPTION
    Initialize partition set iterator to walk over interval in integer field
    space. That is, for "const1 <=? t.field <=? const2" interval, initialize 
    the iterator to return a set of [sub]partitions obtained with the
    following procedure:
      get partition id for t.field = const1,   return it
      get partition id for t.field = const1+1, return it
       ...                 t.field = const1+2, ...
       ...                           ...       ...
       ...                 t.field = const2    ...

  IMPLEMENTATION
    See get_partitions_in_range_iter for general description of interval
    analysis. We support walking over the following intervals: 
      "t.field IS NULL" 
      "c1 <=? t.field <=? c2", where c1 and c2 are finite. 
    Intervals with +inf/-inf, and [NULL, c1] interval can be processed but
    that is more tricky and I don't have time to do it right now.

    Additionally we have these requirements:
    * number of values in the interval must be less then number of
      [sub]partitions, and 
    * Number of values in the interval must be less then MAX_RANGE_TO_WALK.
    
    The rationale behind these requirements is that if they are not met
    we're likely to hit most of the partitions and traversing the interval
    will only add overhead. So it's better return "all partitions used" in
    that case.

  RETURN
    0 - No matching partitions, iterator not initialized
    1 - Some partitions would match, iterator intialized for traversing them
   -1 - All partitions would match, iterator not initialized
*/

int get_part_iter_for_interval_via_walking(partition_info *part_info,
                                           bool is_subpart,
                                           char *min_value, char *max_value,
                                           uint flags,
                                           PARTITION_ITERATOR *part_iter)
{
  Field *field;
  uint total_parts;
  partition_iter_func get_next_func;
  if (is_subpart)
  {
    field= part_info->subpart_field_array[0];
    total_parts= part_info->no_subparts;
    get_next_func=  get_next_subpartition_via_walking;
  }
  else
  {
    field= part_info->part_field_array[0];
    total_parts= part_info->no_parts;
    get_next_func=  get_next_partition_via_walking;
  }

  /* Handle the "t.field IS NULL" interval, it is a special case */
  if (field->real_maybe_null() && !(flags & (NO_MIN_RANGE | NO_MAX_RANGE)) &&
      *min_value && *max_value)
  {
    /* 
      We don't have a part_iter->get_next() function that would find which
      partition "t.field IS NULL" belongs to, so find partition that contains 
      NULL right here, and return an iterator over singleton set.
    */
    uint32 part_id;
    field->set_null();
    if (is_subpart)
    {
      part_id= part_info->get_subpartition_id(part_info);
      init_single_partition_iterator(part_id, part_iter);
      return 1; /* Ok, iterator initialized */
    }
    else
    {
      longlong dummy;
      int res= part_info->is_sub_partitioned() ?
                  part_info->get_part_partition_id(part_info, &part_id,
                                                   &dummy):
                  part_info->get_partition_id(part_info, &part_id, &dummy);
      if (!res)
      {
        init_single_partition_iterator(part_id, part_iter);
        return 1; /* Ok, iterator initialized */
      }
    }
    return 0; /* No partitions match */
  }

  if ((field->real_maybe_null() && 
       ((!(flags & NO_MIN_RANGE) && *min_value) ||  // NULL <? X
        (!(flags & NO_MAX_RANGE) && *max_value))) ||  // X <? NULL
      (flags & (NO_MIN_RANGE | NO_MAX_RANGE)))    // -inf at any bound
  {
    return -1; /* Can't handle this interval, have to use all partitions */
  }
  
  /* Get integers for left and right interval bound */
  longlong a, b;
  uint len= field->pack_length_in_rec();
  store_key_image_to_rec(field, min_value, len);
  a= field->val_int();
  
  store_key_image_to_rec(field, max_value, len);
  b= field->val_int();
  
  /* 
    Handle a special case where the distance between interval bounds is 
    exactly 4G-1. This interval is too big for range walking, and if it is an
    (x,y]-type interval then the following "b +=..." code will convert it to 
    an empty interval by "wrapping around" a + 4G-1 + 1 = a. 
  */
  if ((ulonglong)b - (ulonglong)a == ~0ULL)
    return -1;

  a += test(flags & NEAR_MIN);
  b += test(!(flags & NEAR_MAX));
  ulonglong n_values= b - a;
  
  if (n_values > total_parts || n_values > MAX_RANGE_TO_WALK)
    return -1;

  part_iter->field_vals.start= part_iter->field_vals.cur= a;
  part_iter->field_vals.end=   b;
  part_iter->part_info= part_info;
  part_iter->get_next=  get_next_func;
  return 1;
}


/*
  PARTITION_ITERATOR::get_next implementation: enumerate partitions in range

  SYNOPSIS
    get_next_partition_id_range()
      part_iter  Partition set iterator structure

  DESCRIPTION
    This is implementation of PARTITION_ITERATOR::get_next() that returns
    [sub]partition ids in [min_partition_id, max_partition_id] range.
    The function conforms to partition_iter_func type.

  RETURN
    partition id
    NOT_A_PARTITION_ID if there are no more partitions
*/

uint32 get_next_partition_id_range(PARTITION_ITERATOR* part_iter)
{
  if (part_iter->part_nums.cur == part_iter->part_nums.end)
  {
    part_iter->part_nums.cur= part_iter->part_nums.start;
    return NOT_A_PARTITION_ID;
  }
  else
    return part_iter->part_nums.cur++;
}


/*
  PARTITION_ITERATOR::get_next implementation for LIST partitioning

  SYNOPSIS
    get_next_partition_id_list()
      part_iter  Partition set iterator structure

  DESCRIPTION
    This implementation of PARTITION_ITERATOR::get_next() is special for 
    LIST partitioning: it enumerates partition ids in 
    part_info->list_array[i] where i runs over [min_idx, max_idx] interval.
    The function conforms to partition_iter_func type.

  RETURN 
    partition id
    NOT_A_PARTITION_ID if there are no more partitions
*/

uint32 get_next_partition_id_list(PARTITION_ITERATOR *part_iter)
{
  if (part_iter->part_nums.cur == part_iter->part_nums.end)
  {
    if (part_iter->ret_null_part)
    {
      part_iter->ret_null_part= FALSE;
      return part_iter->part_info->has_null_part_id;
    }
    part_iter->part_nums.cur= part_iter->part_nums.start;
    part_iter->ret_null_part= part_iter->ret_null_part_orig;
    return NOT_A_PARTITION_ID;
  }
  else
    return part_iter->part_info->list_array[part_iter->
                                            part_nums.cur++].partition_id;
}


/*
  PARTITION_ITERATOR::get_next implementation: walk over field-space interval

  SYNOPSIS
    get_next_partition_via_walking()
      part_iter  Partitioning iterator

  DESCRIPTION
    This implementation of PARTITION_ITERATOR::get_next() returns ids of
    partitions that contain records with partitioning field value within
    [start_val, end_val] interval.
    The function conforms to partition_iter_func type.

  RETURN 
    partition id
    NOT_A_PARTITION_ID if there are no more partitioning.
*/

static uint32 get_next_partition_via_walking(PARTITION_ITERATOR *part_iter)
{
  uint32 part_id;
  Field *field= part_iter->part_info->part_field_array[0];
  while (part_iter->field_vals.cur != part_iter->field_vals.end)
  {
    longlong dummy;
    field->store(part_iter->field_vals.cur++,
                 ((Field_num*)field)->unsigned_flag);
    if (part_iter->part_info->is_sub_partitioned() &&
        !part_iter->part_info->get_part_partition_id(part_iter->part_info,
                                                     &part_id, &dummy) ||
        !part_iter->part_info->get_partition_id(part_iter->part_info,
                                                &part_id, &dummy))
      return part_id;
  }
  part_iter->field_vals.cur= part_iter->field_vals.start;
  return NOT_A_PARTITION_ID;
}


/* Same as get_next_partition_via_walking, but for subpartitions */

static uint32 get_next_subpartition_via_walking(PARTITION_ITERATOR *part_iter)
{
  uint32 part_id;
  Field *field= part_iter->part_info->subpart_field_array[0];
  if (part_iter->field_vals.cur == part_iter->field_vals.end)
  {
    part_iter->field_vals.cur= part_iter->field_vals.start;
    return NOT_A_PARTITION_ID;
  }
  field->store(part_iter->field_vals.cur++, FALSE);
  return part_iter->part_info->get_subpartition_id(part_iter->part_info);
}


/*
  Create partition names

  SYNOPSIS
    create_partition_name()
    out:out                   Created partition name string
    in1                       First part
    in2                       Second part
    name_variant              Normal, temporary or renamed partition name

  RETURN VALUE
    NONE

  DESCRIPTION
    This method is used to calculate the partition name, service routine to
    the del_ren_cre_table method.
*/

void create_partition_name(char *out, const char *in1,
                           const char *in2, uint name_variant,
                           bool translate)
{
  char transl_part_name[FN_REFLEN];
  const char *transl_part;

  if (translate)
  {
    tablename_to_filename(in2, transl_part_name, FN_REFLEN);
    transl_part= transl_part_name;
  }
  else
    transl_part= in2;
  if (name_variant == NORMAL_PART_NAME)
    strxmov(out, in1, "#P#", transl_part, NullS);
  else if (name_variant == TEMP_PART_NAME)
    strxmov(out, in1, "#P#", transl_part, "#TMP#", NullS);
  else if (name_variant == RENAMED_PART_NAME)
    strxmov(out, in1, "#P#", transl_part, "#REN#", NullS);
}


/*
  Create subpartition name

  SYNOPSIS
    create_subpartition_name()
    out:out                   Created partition name string
    in1                       First part
    in2                       Second part
    in3                       Third part
    name_variant              Normal, temporary or renamed partition name

  RETURN VALUE
    NONE

  DESCRIPTION
  This method is used to calculate the subpartition name, service routine to
  the del_ren_cre_table method.
*/

void create_subpartition_name(char *out, const char *in1,
                              const char *in2, const char *in3,
                              uint name_variant)
{
  char transl_part_name[FN_REFLEN], transl_subpart_name[FN_REFLEN];

  tablename_to_filename(in2, transl_part_name, FN_REFLEN);
  tablename_to_filename(in3, transl_subpart_name, FN_REFLEN);
  if (name_variant == NORMAL_PART_NAME)
    strxmov(out, in1, "#P#", transl_part_name,
            "#SP#", transl_subpart_name, NullS);
  else if (name_variant == TEMP_PART_NAME)
    strxmov(out, in1, "#P#", transl_part_name,
            "#SP#", transl_subpart_name, "#TMP#", NullS);
  else if (name_variant == RENAMED_PART_NAME)
    strxmov(out, in1, "#P#", transl_part_name,
            "#SP#", transl_subpart_name, "#REN#", NullS);
}
#endif

