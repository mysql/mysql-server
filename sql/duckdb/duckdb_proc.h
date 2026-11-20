/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#pragma once

#include <string>

#include "sql/package/proc.h"

namespace myduck {

extern const LEX_CSTRING DUCKDB_PROC_SCHEMA;

/**
  Execute sql by duckdb

  1) Uniform schema: dbms_duckdb
*/
class Duckdb_proc_base : public im::Proc, public im::Disable_copy_base {
 public:
  explicit Duckdb_proc_base(PSI_memory_key key) : im::Proc(key) {}

  const std::string qname() const override {
    return std::string(DUCKDB_PROC_SCHEMA.str, DUCKDB_PROC_SCHEMA.length) +
           "." + str();
  }
};

class Sql_cmd_duckdb_proc_base : public im::Sql_cmd_admin_proc {
 public:
  explicit Sql_cmd_duckdb_proc_base(THD *thd, mem_root_deque<Item *> *list,
                                    const im::Proc *proc)
      : im::Sql_cmd_admin_proc(thd, list, proc) {}
};

class Sql_cmd_duckdb_query : public Sql_cmd_duckdb_proc_base {
 public:
  explicit Sql_cmd_duckdb_query(THD *thd, mem_root_deque<Item *> *list,
                                const im::Proc *proc)
      : Sql_cmd_duckdb_proc_base(thd, list, proc) {}

  /**
    Implementation of Proc execution body.

    @param[in]    THD           Thread context

    @retval       true          Failure
    @retval       false         Success
  */
  bool pc_execute(THD *thd) override;

  void send_result(THD *thd, bool error) override;
};

class Duckdb_proc_query : public Duckdb_proc_base {
  using Sql_cmd_type = Sql_cmd_duckdb_query;

  /** All the parameters */
  enum enum_parameter { QUERY_PARAM_SQL = 0, QUERY_PARAM_LAST };

  enum_field_types get_field_type(enum_parameter param) {
    switch (param) {
      case QUERY_PARAM_SQL:
        return MYSQL_TYPE_VARCHAR;
      default:
        assert(0);
    }
    return MYSQL_TYPE_VARCHAR;
  }

  enum enum_column { COLUMN_RESULT = 0, COLUMN_LAST };

 public:
  explicit Duckdb_proc_query(PSI_memory_key key = 0) : Duckdb_proc_base(key) {
    /* Init parameters */
    for (size_t i = QUERY_PARAM_SQL; i < QUERY_PARAM_LAST; i++) {
      m_parameters.assign_at(
          i, get_field_type(static_cast<enum enum_parameter>(i)));
    }

    /* Result set protocol packet */
    m_result_type = Result_type::RESULT_SET;
    Column_element elements[COLUMN_LAST] = {
        {MYSQL_TYPE_VARCHAR, STRING_WITH_LEN("RESULT"), 1024}};

    for (size_t i = 0; i < COLUMN_LAST; i++) {
      m_columns.assign_at(i, elements[i]);
    }
  }

  /**
    Singleton instance for query()
  */
  static im::Proc *instance();

  /**
    Evoke the sql_cmd object for query() proc.
  */
  Sql_cmd *evoke_cmd(THD *thd, mem_root_deque<Item *> *list) const override;

  ~Duckdb_proc_query() override = default;

  /** Proc name */
  const std::string str() const override { return "query"; }
};

}  // namespace myduck
