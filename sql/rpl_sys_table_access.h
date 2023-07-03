/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RPL_SYS_TABLE_ACCESS_INCLUDED
#define RPL_SYS_TABLE_ACCESS_INCLUDED

#include "sql/field.h"
#include "sql/sql_class.h"
#include "sql/table.h"
#include "thr_lock.h"

class Json_wrapper;
class THD;
struct TABLE;

/**
  @class Rpl_sys_table_access

  The class is used to simplify table data access. It creates new thread/session
  context (THD) and open table on class object creation, and destroys thread and
  closes all open thread tables on class object destruction.
*/
class Rpl_sys_table_access {
 public:
  /**
    Construction.
    @param[in]  schema_name   Database where the table resides
    @param[in]  table_name    Table to be opened
    @param[in]  max_num_field Maximum number of fields
  */
  Rpl_sys_table_access(const std::string &schema_name,
                       const std::string &table_name, uint max_num_field);

  /**
    Destruction. All opened tables with the open_tables are closed during
    destruction if not already done in deinit().
  */
  ~Rpl_sys_table_access();

  /**
    Creates new thread/session context (THD) and open's table on class object
    creation.

    @param[in]  lock_type     How to lock the table

    @retval true  if there is error
    @retval false if there is no error
  */
  bool open(enum thr_lock_type lock_type);

  /**
    All opened tables with the open_tables are closed and removes
    THD created in close().

    @param[in]  error         State that there was a error on the table
                              operations
    @param[in]  ignore_global_read_lock
                              State that the global_read_lock must be
                              ignored

    @retval true  if there is error
    @retval false if there is no error
  */
  bool close(bool error, bool ignore_global_read_lock = false);

  /**
    Get TABLE object created for the table access purposes.

    @return TABLE pointer.
  */
  TABLE *get_table();

  /**
    Set error.
  */
  void set_error() { m_error = true; }

  /**
    Verify if error is set.

    @retval true  if there is error
    @retval false if there is no error
  */
  bool get_error() { return m_error; }

  /**
    Stores provided string to table's field.

    @param[in]  field        Field class object
    @param[in]  fld          The std::string value to be saved.
    @param[in]  cs           Charset info

    @retval true   Error
    @retval false  Success
  */
  bool store_field(Field *field, std::string fld,
                   CHARSET_INFO *cs = &my_charset_bin);

  /**
    Stores provided integer to table's field.

    @param[in]  field        Field class object
    @param[in]  fld          The long long value to be saved.
    @param[in]  unsigned_val If value is unsigned.

    @retval true   Error
    @retval false  Success
  */
  bool store_field(Field *field, long long fld, bool unsigned_val = true);

  /**
    Stores provided Json to table's field.

    @param[in]  field        Field class object
    @param[in]  wrapper      The Json_wrapper class object value

    @retval true   Error
    @retval false  Success
  */
  bool store_field(Field *field, const Json_wrapper &wrapper);

  /**
    Retrieves string field from provided table's field.

    @param[in]  field        Field class object
    @param[in]  fld          The std::string value to be retrieved.
    @param[in]  cs           Charset info

    @retval true   Error
    @retval false  Success
  */
  bool get_field(Field *field, std::string &fld,
                 CHARSET_INFO *cs = &my_charset_bin);

  /**
    Retrieves long long integer field from provided table's field.

    @param[in]  field        Field class object
    @param[in]  fld          The uint value to be retrieved.

    @retval true   Error
    @retval false  Success
  */
  bool get_field(Field *field, uint &fld);

  /**
    Retrieves Json field from provided table's field.

    @param[in]  field        Field class object
    @param[in]  wrapper      The retrieved field would be saved in
                             Json_wrapper format.

    @retval true   Error
    @retval false  Success
  */
  bool get_field(Field *field, Json_wrapper &wrapper);

  std::string get_field_error_msg(std::string field_name) const;

  static void handler_write_row_func(Rpl_sys_table_access &table_op,
                                     bool &err_val, std::string &err_msg,
                                     uint table_index = 0,
                                     key_part_map keypart_map = HA_WHOLE_KEY);

  static void handler_delete_row_func(Rpl_sys_table_access &table_op,
                                      bool &err_val, std::string &err_msg,
                                      uint table_index = 0,
                                      key_part_map keypart_map = HA_WHOLE_KEY);

  template <class F, class... Ts, std::size_t... Is>
  static void for_each_in_tuple(std::tuple<Ts...> &tuple, F func,
                                std::index_sequence<Is...>) {
    using tuple_list = int[255];
    (void)tuple_list{0, ((void)func(Is, std::get<Is>(tuple)), 0)...};
  }

  template <class F, class... Ts>
  static void for_each_in_tuple(std::tuple<Ts...> &tuple, F func) {
    for_each_in_tuple(tuple, func, std::make_index_sequence<sizeof...(Ts)>());
  }

  template <class F, class... Ts, std::size_t... Is>
  static void for_each_in_tuple(const std::tuple<Ts...> &tuple, F func,
                                std::index_sequence<Is...>) {
    using tuple_list = int[255];
    (void)tuple_list{0, ((void)func(Is, std::get<Is>(tuple)), 0)...};
  }

  template <class F, class... Ts>
  static void for_each_in_tuple(const std::tuple<Ts...> &tuple, F func) {
    for_each_in_tuple(tuple, func, std::make_index_sequence<sizeof...(Ts)>());
  }

  /**
    Delete all rows on `m_schema_name.m_table_name`.

    @retval true  if there is error
    @retval false if there is no error
  */
  bool delete_all_rows();

  /**
    Return the version stored on `m_schema_version_name.m_table_version_name`
    for the `m_schema_name.m_table_name` table.

    @retval 0  if there is error
    @retval >0 if there is no error
  */
  ulonglong get_version();

  /**
    Increment the version stored on `m_schema_version_name.m_table_version_name`
    for the `m_schema_name.m_table_name` table.

    @retval true  if there is error
    @retval false if there is no error
  */
  bool increment_version();

  /**
    Update the version stored on `m_schema_version_name.m_table_version_name`
    for the `m_schema_name.m_table_name` table.

    @param[in]  version  the version value

    @retval true  if there is error
    @retval false if there is no error
  */
  bool update_version(ulonglong version);

  /**
    Delete the version stored on `m_schema_version_name.m_table_version_name`
    for the `m_schema_name.m_table_name` table.

    @retval true  if there is error
    @retval false if there is no error
  */
  bool delete_version();

  /**
    Get database name of table accessed.

    @return database name.
  */
  std::string get_db_name() { return m_schema_name; }

  /**
    Get table name of table accessed.

    @return table name.
  */
  std::string get_table_name() { return m_table_name; }

 private:
  /* THD created for TableAccess object purpose. */
  THD *m_thd{nullptr};

  /* THD associated with the calling thread. */
  THD *m_current_thd{nullptr};

  /* The variable determine if table is opened or closed successfully. */
  bool m_error{false};

  /* Table_ref object */
  Table_ref *m_table_list{nullptr};
  enum thr_lock_type m_lock_type;

  std::string m_schema_name;
  std::string m_table_name;
  uint m_max_num_field;

  const std::string m_schema_version_name{"mysql"};
  const std::string m_table_version_name{
      "replication_group_configuration_version"};
  const uint m_table_data_index = 0;
  const uint m_table_version_index = 1;
  const uint m_table_list_size = 2;
};

#endif /* RPL_SYS_TABLE_ACCESS_INCLUDED */
