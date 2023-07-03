/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _SQL_DATA_RESULT_H_
#define _SQL_DATA_RESULT_H_

#include "sql_data_context.h"


namespace xpl
{
class PFS_string;

class Sql_data_result
{
public:
  Sql_data_result(Sql_data_context &context);

  void disable_binlog();
  void restore_binlog();

  void query(const ngs::PFS_string &query);

  void get_next_field(long &value);
  void get_next_field(bool &value);
  void get_next_field(std::string &value);
  void get_next_field(const char * &value);
  void get_next_field(char * &value);

  bool next_row();
  long statement_warn_count();
  Buffering_command_delegate::Resultset::size_type size() const { return m_result_set.size(); }

  template<typename T> Sql_data_result &get(T &value)
  {
    get_next_field(value);
    return *this;
  }

private:
  typedef Callback_command_delegate::Field_value Field_value;
  typedef Buffering_command_delegate::Resultset Resultset;

  Field_value *get_value();
  Field_value &validate_field_index_no_null(const enum_field_types field_type);
  void         validate_field_index(const enum_field_types field_type) const;
  void         validate_field_index(const enum_field_types field_type1, const enum_field_types field_type2) const;
  void         validate_field_index_common() const;

  Resultset                                 m_result_set;
  Sql_data_context::Result_info             m_result_info;
  std::vector<Command_delegate::Field_type> m_field_types;
  std::size_t                               m_field_index;
  Resultset::iterator                       m_row_index;
  Sql_data_context                         &m_context;
};

} // namespace xpl

#endif // _SQL_DATA_RESULT_H_
