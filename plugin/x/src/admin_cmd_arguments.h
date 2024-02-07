/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_SRC_ADMIN_CMD_ARGUMENTS_H_
#define PLUGIN_X_SRC_ADMIN_CMD_ARGUMENTS_H_

#include <memory>
#include <string>
#include <vector>

#include "plugin/x/src/interface/admin_command_arguments.h"
#include "plugin/x/src/ngs/error_code.h"
#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"

namespace xpl {

class Admin_command_arguments_object : public iface::Admin_command_arguments {
 public:
  using Object = ::Mysqlx::Datatypes::Object;
  explicit Admin_command_arguments_object(const List &args);
  explicit Admin_command_arguments_object(const Object &obj);

  Admin_command_arguments_object &string_arg(
      const Argument_name_list &name, std::string *ret_value,
      const Appearance_type appearance) override;
  Admin_command_arguments_object &string_list(
      const Argument_name_list &name, std::vector<std::string> *ret_value,
      const Appearance_type appearance) override;
  Admin_command_arguments_object &sint_arg(
      const Argument_name_list &name, int64_t *ret_value,
      const Appearance_type appearance) override;
  Admin_command_arguments_object &uint_arg(
      const Argument_name_list &name, uint64_t *ret_value,
      const Appearance_type appearance) override;
  Admin_command_arguments_object &bool_arg(
      const Argument_name_list &name, bool *ret_value,
      const Appearance_type appearance) override;
  Admin_command_arguments_object &docpath_arg(
      const Argument_name_list &name, std::string *ret_value,
      const Appearance_type appearance) override;
  Admin_command_arguments_object &object_list(
      const Argument_name_list &name,
      std::vector<iface::Admin_command_arguments *> *ret_value,
      const Appearance_type appearance,
      unsigned expected_members_count) override;

  Admin_command_arguments_object &any_arg(
      const Argument_name_list &name, Any *ret_value,
      const Appearance_type appearance) override;

  Admin_command_arguments_object &object_arg(
      const Argument_name_list &name, Object *ret_value,
      const Appearance_type appearance) override;

  std::vector<std::string> get_obj_keys() const;

  const ngs::Error_code &end() override;
  const ngs::Error_code &error() const override { return m_error; }
  void set_path(const std::string &path) { m_path = path; }

  uint32_t size() const override { return m_object.fld().size(); }

 private:
  using Object_field_list =
      ::google::protobuf::RepeatedPtrField<Object::ObjectField>;

  template <typename H>
  void get_scalar_arg(const Argument_name_list &name,
                      const Appearance_type appearance, H *handler);
  template <typename H>
  void get_scalar_value(const Any &value, H *handler);
  const Object::ObjectField *get_object_field(const Argument_name_list &name,
                                              const Appearance_type appearance);
  void set_number_args_error(const std::string &name);
  void set_arg_value_error(const std::string &name);
  void set_arg_type_error(const std::string &name,
                          const std::string &expected_type);

  Admin_command_arguments_object *add_sub_object(const Object &object,
                                                 const std::string &path);
  std::string get_path(const std::string &name) const {
    return m_path.empty() ? name : m_path + "." + name;
  }

  const bool m_args_empty;
  const bool m_is_object;
  const Object &m_object;
  ngs::Error_code m_error;
  int32_t m_args_consumed{0};
  std::vector<std::shared_ptr<Admin_command_arguments_object>> m_sub_objects;
  std::string m_path;
  std::vector<std::string> m_allowed_keys;
};

#define DOC_MEMBER_REGEX                                                     \
  R"(\\$((\\*{2})?(\\[([[:digit:]]+|\\*)\\]|\\.([[:alpha:]_\\$][[:alnum:]_)" \
  R"(\\$]*|\\*|\\".*\\"|`.*`)))*)"

}  // namespace xpl

#endif  // PLUGIN_X_SRC_ADMIN_CMD_ARGUMENTS_H_
