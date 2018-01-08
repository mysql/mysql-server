/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef _SQL_DATA_RESULT_H_
#define _SQL_DATA_RESULT_H_

#include <initializer_list>

#include "plugin/x/ngs/include/ngs/interface/sql_session_interface.h"
#include "plugin/x/src/xpl_resultset.h"

namespace xpl {
class PFS_string;

class Sql_data_result {
 public:
  Sql_data_result(ngs::Sql_session_interface &context);

  void disable_binlog();
  void restore_binlog();

  void query(const ngs::PFS_string &query);

  void get_next_field(long &value);
  void get_next_field(bool &value);
  void get_next_field(std::string &value);
  void get_next_field(const char *&value);
  void get_next_field(char *&value);

  bool next_row();
  long statement_warn_count() { return m_resultset.get_info().num_warnings; }
  Collect_resultset::Row_list::size_type size() const {
    return m_resultset.get_row_list().size();
  }

  template <typename T>
  Sql_data_result &get(T &value) {
    get_next_field(value);
    return *this;
  }

  template <typename T>
  T get() {
    T value;
    get_next_field(value);
    return value;
  }

  Sql_data_result &skip() {
    ++m_field_index;
    return *this;
  }

 private:
  typedef Collect_resultset::Field Field_value;

  Field_value *get_value() { return (*m_row_index).fields[m_field_index++]; }
  Field_value &validate_field_index_no_null(
      std::initializer_list<enum_field_types> field_types);
  void validate_field_index(std::initializer_list<enum_field_types> field_types)
      const;

  Collect_resultset m_resultset;
  std::size_t m_field_index;
  Collect_resultset::Row_list::const_iterator m_row_index;
  ngs::Sql_session_interface &m_context;
};

}  // namespace xpl

#endif  // _SQL_DATA_RESULT_H_
