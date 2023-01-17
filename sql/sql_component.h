/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#ifndef _sql_component_h
#define _sql_component_h

#include <string>
#include <vector>

#include "lex_string.h"
#include "my_sqlcommand.h"
#include "sql/mem_root_array.h"
#include "sql/sql_cmd.h"

class THD;
class PT_item_list;
class String;
struct PT_install_component_set_element;

/**
   This class implements the INSTALL COMPONENT statement.
*/

class Sql_cmd_install_component : public Sql_cmd {
 public:
  Sql_cmd_install_component(const Mem_root_array_YY<LEX_STRING> &urns,
                            List<PT_install_component_set_element> *set_exprs)
      : m_urns(urns), m_set_exprs(set_exprs) {}

  enum_sql_command sql_command_code() const override {
    return SQLCOM_INSTALL_COMPONENT;
  }

  /**
    Install a new component by loading it by dynamic loader service.

    @param thd  Thread context

    @returns false if success, true otherwise
  */
  bool execute(THD *thd) override;

  char **m_arg_list{nullptr};
  int m_arg_list_size{0};

 private:
  const Mem_root_array_YY<LEX_STRING> m_urns;
  List<PT_install_component_set_element> *m_set_exprs;
};

/**
   This class implements the UNINSTALL COMPONENT statement.
*/

class Sql_cmd_uninstall_component : public Sql_cmd {
 public:
  Sql_cmd_uninstall_component(const Mem_root_array_YY<LEX_STRING> &urns)
      : m_urns(urns) {}

  enum_sql_command sql_command_code() const override {
    return SQLCOM_UNINSTALL_COMPONENT;
  }

  /**
    Uninstall a plugin by unloading it in the dynamic loader service.

    @param thd  Thread context

    @returns false if success, true otherwise
  */
  bool execute(THD *thd) override;

 private:
  const Mem_root_array_YY<LEX_STRING> m_urns;
};

/**
  This class implements component loading through manifest file
*/
class Deployed_components final {
 public:
  explicit Deployed_components(const std::string program_name,
                               const std::string instance_path);
  ~Deployed_components();
  bool valid() const { return valid_; }
  bool components_loaded() const { return loaded_; }

 private:
  void get_next_component(std::string &components_list,
                          std::string &one_component);
  bool load();
  bool unload();
  bool make_urns(std::vector<const char *> &urns);

 private:
  std::string program_name_;
  std::string instance_path_;
  std::string components_;
  std::string last_error_;
  bool valid_;
  bool loaded_;
};
#endif
