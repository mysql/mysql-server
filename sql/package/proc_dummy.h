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

#ifndef SQL_PACKAGE_PROC_DUMMY_INCLUDED
#define SQL_PACKAGE_PROC_DUMMY_INCLUDED

/**
  Dummy proc definition.

  It was used to demostrate how to define a native procedure,
  so it only take effect on DBUG mode.
*/
namespace im {

/* The schema of dummy and dummy_2 proc */
extern const LEX_CSTRING PROC_DUMMY_SCHEMA;

/**
  Dummy proc sql command class.
*/
class Sql_cmd_proc_dummy : public Sql_cmd_admin_proc {
 public:
  explicit Sql_cmd_proc_dummy(THD *thd, mem_root_deque<Item *> *list,
                              const Proc *proc)
      : Sql_cmd_admin_proc(thd, list, proc) {}

  /**
    Implementation of Proc execution body.

    @param[in]    THD           Thread context

    @retval       true          Failure
    @retval       false         Success
  */
  virtual bool pc_execute(THD *thd) override;
};

/**
  Dummy proc definition.
*/
class Proc_dummy : public Proc {
  typedef Sql_cmd_proc_dummy Sql_cmd_type;

 public:
  explicit Proc_dummy(PSI_memory_key key) : Proc(key) {
    m_result_type = Result_type::RESULT_OK;
  }

  virtual ~Proc_dummy() {}

  static Proc *instance();

  virtual Sql_cmd *evoke_cmd(THD *thd,
                             mem_root_deque<Item *> *list) const override;

  virtual const std::string str() const override {
    return std::string("dummy");
  }

  virtual const std::string qname() const override {
    std::stringstream ss;
    ss << PROC_DUMMY_SCHEMA.str << "." << str();
    return ss.str();
  }
};

/**
  Dummy proc sql command class.
*/
class Sql_cmd_proc_dummy_2 : public Sql_cmd_admin_proc {
 public:
  explicit Sql_cmd_proc_dummy_2(THD *thd, mem_root_deque<Item *> *list,
                                const Proc *proc)
      : Sql_cmd_admin_proc(thd, list, proc) {}

  /**
    Implementation of Proc execution body.

    @param[in]    THD           Thread context

    @retval       true          Failure
    @retval       false         Success
  */
  virtual bool pc_execute(THD *thd) override;

  /**
    Send the result set.
  */
  virtual void send_result(THD *thd, bool error) override;
};

/**
  Dummy_2 proc definition.
*/
class Proc_dummy_2 : public Proc {
 public:
  typedef Sql_cmd_proc_dummy_2 Sql_cmd_type;

  /**
    call dummy_2(id bigint, name varchar(100));
     1) id : MYSQL_TYPE_LONGLONG
     2) name : MYSQL_TYPE_VARCHAR
  */
  enum enum_parameter { PARAMETER_ID_ID = 0, PARAMETER_NAME_ID = 1 };

  /**
    dummy_2 result columns list
  */
  enum enum_column { COLUMN_NAME = 0, COLUMN_ID };

  static constexpr const char *column_name = "NAME";
  static constexpr const char *column_id = "ID";

 public:
  explicit Proc_dummy_2(PSI_memory_key key) : Proc(key) {
    m_result_type = Result_type::RESULT_SET;

    /* Parameter definition. */
    m_parameters.assign_at(PARAMETER_ID_ID, MYSQL_TYPE_LONGLONG);
    m_parameters.assign_at(PARAMETER_NAME_ID, MYSQL_TYPE_VARCHAR);

    /* Column definition. */

    Column_element element;

    /* Column name */
    element = {MYSQL_TYPE_VARCHAR, column_name, strlen(column_name), 256};
    m_columns.assign_at(COLUMN_NAME, element);

    /* Column id */
    element = {MYSQL_TYPE_LONGLONG, column_id, strlen(column_id), 0};
    m_columns.assign_at(COLUMN_ID, element);
  }

  virtual ~Proc_dummy_2() {}

  static Proc *instance();

  virtual Sql_cmd *evoke_cmd(THD *thd,
                             mem_root_deque<Item *> *list) const override;

  virtual const std::string str() const override {
    return std::string("dummy_2");
  }

  virtual const std::string qname() const override {
    std::stringstream ss;
    ss << PROC_DUMMY_SCHEMA.str << "." << str();
    return ss.str();
  }
};
} /* namespace im */

#endif
