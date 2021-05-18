/* Copyright (c) 2020, 2021, Oracle and/or its affiliates.

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

#include "sql/rpl_table_access.h"  // System_table_access
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
class Rpl_sys_table_access : public System_table_access {
 public:
  /**
    Construction.
    @param[in]  schema_name   Database where the table resides
    @param[in]  table_name    Table to be openned
    @param[in]  max_num_field Maximum number of fields
  */
  Rpl_sys_table_access(const std::string &schema_name,
                       const std::string &table_name, uint max_num_field);

  /**
    Destruction. All opened tables with the open_tables are closed during
    destruction if not already done in deinit().
  */
  ~Rpl_sys_table_access() override;

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

    @retval true  if there is error
    @retval false if there is no error
  */
  bool close(bool error);

  /**
    Get TABLE object created for the table access purposes.

    @return TABLE pointer.
  */
  TABLE *get_table() { return m_table; }

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
    Prepares before opening table.

    @param[in]  thd  Thread requesting to open the table
  */
  void before_open(THD *thd) override;

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

 protected:
  /* THD created for TableAccess object purpose. */
  THD *m_thd{nullptr};

  /* THD associated with the calling thread. */
  THD *m_current_thd{nullptr};

  /* The variable determine if table is opened or closed successfully. */
  bool m_error{false};

  /* Determine if index is deinitialized. */
  bool m_key_deinit{false};

  /* TABLE object */
  TABLE *m_table{nullptr};

  /* Save the lock info. */
  Open_tables_backup m_backup;

  std::string m_schema_name;
  std::string m_table_name;
  uint m_max_num_field;
};

#endif /* RPL_SYS_TABLE_ACCESS_INCLUDED */
