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

#include "sql/package/proc.h"

class sys_var;
class THD;

namespace im {

class Sql_cmd_show_native_procedure : public Sql_cmd_admin_proc {
 public:
  explicit Sql_cmd_show_native_procedure(THD *thd, mem_root_deque<Item *> *list,
                                         const Proc *proc)
      : Sql_cmd_admin_proc(thd, list, proc) {}
  /**
    Implementation of Proc execution body.

    @param[in]    THD           Thread context

    @retval       true          Failure
    @retval       false         Success
  */
  virtual bool pc_execute(THD *thd) override;

  /* Override default send_result */
  virtual void send_result(THD *thd, bool error) override;
};

class Show_native_procedure_proc : public Proc, public Disable_copy_base {
  using Sql_cmd_type = Sql_cmd_show_native_procedure;

  enum enum_column {
    COLUMN_SCHEMA_NAME = 0,
    COLUMN_ELEMENT_NAME,
    PROC_TYPE,
    PARAMETERS,
    COLUMN_LAST
  };

 public:
  explicit Show_native_procedure_proc(PSI_memory_key key = 0) : Proc(key) {
    m_result_type = Result_type::RESULT_SET;
    Column_element elements[COLUMN_LAST] = {
        {MYSQL_TYPE_VARCHAR, STRING_WITH_LEN("SCHEMA_NAME"), 128},
        {MYSQL_TYPE_VARCHAR, STRING_WITH_LEN("PROC_NAME"), 128},
        {MYSQL_TYPE_VARCHAR, STRING_WITH_LEN("PROC_TYPE"), 128},
        {MYSQL_TYPE_VARCHAR, STRING_WITH_LEN("PARAMETERS"), 1024}};
    for (size_t i = 0; i < COLUMN_LAST; i++) {
      m_columns.assign_at(i, elements[i]);
    }
  }

  /* Singleton instance */
  static Proc *instance();

  /**
    Evoke the sql_cmd object for proc.
  */
  virtual Sql_cmd *evoke_cmd(THD *thd,
                             mem_root_deque<Item *> *list) const override;

  virtual ~Show_native_procedure_proc() {}

  /* Proc name */
  virtual const std::string str() const override {
    return std::string("show_native_procedure");
  }

  virtual const std::string qname() const override {
    std::stringstream ss;
    ss << im::ADMIN_PROC_SCHEMA.str << "." << str();
    return ss.str();
  }
};

} /* namespace im */
