/*
 * Copyright (c) 2019, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_INTERFACE_ADMIN_COMMAND_ARGUMENTS_H_
#define PLUGIN_X_SRC_INTERFACE_ADMIN_COMMAND_ARGUMENTS_H_

#include <string>
#include <vector>

#include "plugin/x/src/ngs/error_code.h"
#include "plugin/x/src/ngs/protocol/protocol_protobuf.h"

namespace xpl {
namespace iface {

class Admin_command_arguments {
 public:
  using Argument_list = std::vector<std::string>;
  using List = ::google::protobuf::RepeatedPtrField<::Mysqlx::Datatypes::Any>;
  using Any = ::Mysqlx::Datatypes::Any;
  using Object = ::Mysqlx::Datatypes::Object;
  using Argument_name_list = std::vector<std::string>;

  static const char *const k_placeholder;
  enum class Appearance_type { k_obligatory, k_optional };

  Admin_command_arguments() = default;
  virtual ~Admin_command_arguments() = default;
  Admin_command_arguments(const Admin_command_arguments &) = default;
  Admin_command_arguments(Admin_command_arguments &&) = default;
  Admin_command_arguments &operator=(const Admin_command_arguments &) = default;
  Admin_command_arguments &operator=(Admin_command_arguments &&) = default;

  virtual Admin_command_arguments &string_arg(
      const Argument_name_list &name, std::string *ret_value,
      const Appearance_type appearance) = 0;
  virtual Admin_command_arguments &string_list(
      const Argument_name_list &name, std::vector<std::string> *ret_value,
      const Appearance_type appearance) = 0;
  virtual Admin_command_arguments &sint_arg(
      const Argument_name_list &name, int64_t *ret_value,
      const Appearance_type appearance) = 0;
  virtual Admin_command_arguments &uint_arg(
      const Argument_name_list &name, uint64_t *ret_value,
      const Appearance_type appearance) = 0;
  virtual Admin_command_arguments &bool_arg(
      const Argument_name_list &name, bool *ret_value,
      const Appearance_type appearance) = 0;
  virtual Admin_command_arguments &docpath_arg(
      const Argument_name_list &name, std::string *ret_value,
      const Appearance_type appearance) = 0;
  virtual Admin_command_arguments &object_list(
      const Argument_name_list &name,
      std::vector<Admin_command_arguments *> *ret_value,
      const Appearance_type appearance,
      unsigned expected_members_count = 3) = 0;

  virtual Admin_command_arguments &any_arg(
      const Argument_name_list &name, Any *ret_value,
      const Appearance_type appearance) = 0;

  virtual Admin_command_arguments &object_arg(
      const Argument_name_list &name, Object *ret_value,
      const Appearance_type appearance) = 0;

  virtual const ngs::Error_code &end() = 0;
  virtual const ngs::Error_code &error() const = 0;

  virtual uint32_t size() const = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_ADMIN_COMMAND_ARGUMENTS_H_
