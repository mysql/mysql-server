/* Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef RPL_INFO_TABLE_H
#define RPL_INFO_TABLE_H

#include "my_global.h"
#include "mysql/mysql_lex_string.h"  // LEX_STRING
#include "rpl_info_handler.h"        // Rpl_info_handler
#include "table.h"                   // TABLE

class Rpl_info_table_access;
typedef struct st_mysql_lex_string LEX_STRING;


/**
  Methods to find information in a table:
  
  FIND_SCAN does a index scan and stops at n-th occurrence.
  
  FIND_KEY retrieves the index entry previous populated at
  values if there is any.
*/
enum enum_find_method { FIND_SCAN, FIND_KEY };

class Rpl_info_table : public Rpl_info_handler
{
  friend class Rpl_info_factory;

public:
  virtual ~Rpl_info_table();

private:
  /**
    This property identifies the name of the schema where a
    replication table is created.
  */
  LEX_STRING str_schema;

  /**
    This property identifies the name of a replication
    table.
  */
  LEX_STRING str_table;

  /**
    This property represents a description of the repository.
    Speciffically, "schema"."table".
  */
  char *description;

  /**
    This property represents the amount of fields in the repository
    primary key.
  */
  uint m_n_pk_fields;

  /**
    This property identifies the indexes of the primary keys fields
    in the table.
  */
  const uint *m_pk_field_indexes;

  /**
    This is a pointer to a class that facilitates manipulation
    of replication tables.
  */
  Rpl_info_table_access *access;

  /**
    Identifies if a table is transactional or non-transactional.
    This is used to provide a crash-safe behaviour.
  */
  bool is_transactional;

  int do_init_info();
  int do_init_info(uint instance);
  int do_init_info(enum_find_method method, uint instance);
  enum_return_check do_check_info();
  enum_return_check do_check_info(uint instance);
  void do_end_info();
  int do_flush_info(const bool force);
  int do_remove_info();
  int do_clean_info();
  /**
    Returns the number of entries in the table identified by:
    param_schema.param_table.

    @param[in]  nparam           Number of fields in the table.
    @param[in]  param_schema     Table's schema.
    @param[in]  param_table      Table's name.
    @param[out] counter          Number of entries found.

    @retval false Success
    @retval true  Error
  */
  static bool do_count_info(uint nparam, const char* param_schema,
                            const char* param_table,  uint* counter);
  static int do_reset_info(uint nparam, const char* param_schema,
                           const char *param_table,
                           const char *channel_name, uint channel_idx);
  int do_prepare_info_for_read();
  int do_prepare_info_for_write();

  bool do_set_info(const int pos, const char *value);
  bool do_set_info(const int pos, const uchar *value,
                   const size_t size);
  bool do_set_info(const int pos, const int value);
  bool do_set_info(const int pos, const ulong value);
  bool do_set_info(const int pos, const float value);
  bool do_set_info(const int pos, const Server_ids *value);
  bool do_get_info(const int pos, char *value, const size_t size,
                   const char *default_value);
  bool do_get_info(const int pos, uchar *value, const size_t size,
                   const uchar *default_value);
  bool do_get_info(const int pos, int *value,
                   const int default_value);
  bool do_get_info(const int pos, ulong *value,
                   const ulong default_value);
  bool do_get_info(const int pos, float *value,
                   const float default_value);
  bool do_get_info(const int pos, Server_ids *value,
                   const Server_ids *default_value);
  char* do_get_description_info();

  bool do_is_transactional();
  bool do_update_is_transactional();
  uint do_get_rpl_info_type();

  /**
    Verify if the table primary key fields are at the expected (column)
    position.

    @param table The table handle where the verification will be done.

    @return false if the table primary key fields are fine.
    @return true  if problems were found with table primary key fields.
  */
  bool verify_table_primary_key_fields(TABLE* table);

  Rpl_info_table(uint nparam,
                 const char* param_schema,
                 const char *param_table,
                 const uint param_n_pk_fields= 0,
                 const uint *param_pk_field_indexes= NULL);

  Rpl_info_table(const Rpl_info_table& info);
  Rpl_info_table& operator=(const Rpl_info_table& info);
};
#endif /* RPL_INFO_TABLE_H */
