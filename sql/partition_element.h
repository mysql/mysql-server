/* Copyright (C) 2000,200666666 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

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

/**
 * An enum and a struct to handle partitioning and subpartitioning.
 */
enum partition_type {
  NOT_A_PARTITION= 0,
  RANGE_PARTITION,
  HASH_PARTITION,
  LIST_PARTITION
};

enum partition_state {
  PART_NORMAL= 0,
  PART_IS_DROPPED= 1,
  PART_TO_BE_DROPPED= 2,
  PART_TO_BE_ADDED= 3,
  PART_TO_BE_REORGED= 4,
  PART_REORGED_DROPPED= 5,
  PART_CHANGED= 6,
  PART_IS_CHANGED= 7,
  PART_IS_ADDED= 8
};

/*
  This struct is used to contain the value of an element
  in the VALUES IN struct. It needs to keep knowledge of
  whether it is a signed/unsigned value and whether it is
  NULL or not.
*/

typedef struct p_elem_val
{
  longlong value;
  bool null_value;
  bool unsigned_flag;
} part_elem_value;

class partition_element :public Sql_alloc {
public:
  List<partition_element> subpartitions;
  List<part_elem_value> list_val_list;
  ulonglong part_max_rows;
  ulonglong part_min_rows;
  longlong range_value;
  char *partition_name;
  char *tablespace_name;
  char* part_comment;
  char* data_file_name;
  char* index_file_name;
  handlerton *engine_type;
  enum partition_state part_state;
  uint16 nodegroup_id;
  bool has_null_value;
  bool signed_flag;/* Indicate whether this partition uses signed constants */
  bool max_value;  /* Indicate whether this partition uses MAXVALUE */

  partition_element()
  : part_max_rows(0), part_min_rows(0), range_value(0),
    partition_name(NULL), tablespace_name(NULL), part_comment(NULL),
    data_file_name(NULL), index_file_name(NULL),
    engine_type(NULL), part_state(PART_NORMAL),
    nodegroup_id(UNDEF_NODEGROUP), has_null_value(FALSE),
    signed_flag(FALSE), max_value(FALSE)
  {
    subpartitions.empty();
    list_val_list.empty();
  }
  ~partition_element() {}
};
