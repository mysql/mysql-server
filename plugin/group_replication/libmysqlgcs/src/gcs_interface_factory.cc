/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#include <stddef.h>
#include <algorithm>
#include <string>
#ifdef _WIN32
#include <iterator>
#endif

#include "plugin/group_replication/libmysqlgcs/include/mysql/gcs/gcs_interface.h"
#include "plugin/group_replication/libmysqlgcs/src/bindings/xcom/gcs_xcom_interface.h"

Gcs_interface *Gcs_interface_factory::get_interface_implementation(
    enum_available_interfaces binding) {
  Gcs_interface *retval = nullptr;
  switch (binding) {
    case XCOM:
      retval = Gcs_xcom_interface::get_interface();
      break;
    default:
      break;
  }

  return retval;
}

Gcs_interface *Gcs_interface_factory::get_interface_implementation(
    const std::string &binding) {
  enum_available_interfaces binding_translation =
      Gcs_interface_factory::from_string(binding);

  return Gcs_interface_factory::get_interface_implementation(
      binding_translation);
}

void Gcs_interface_factory::cleanup(const std::string &binding) {
  enum_available_interfaces binding_translation =
      Gcs_interface_factory::from_string(binding);

  Gcs_interface_factory::cleanup(binding_translation);
}

void Gcs_interface_factory::cleanup(enum_available_interfaces binding) {
  switch (binding) {
    case XCOM:
      Gcs_xcom_interface::cleanup();
      break;
    default:
      break;
  }
}

void Gcs_interface_factory::cleanup_thread_communication_resources(
    const std::string &binding) {
  enum_available_interfaces binding_translation =
      Gcs_interface_factory::from_string(binding);

  Gcs_interface_factory::cleanup_thread_communication_resources(
      binding_translation);
}

void Gcs_interface_factory::cleanup_thread_communication_resources(
    enum_available_interfaces binding) {
  switch (binding) {
    case XCOM:
      Gcs_xcom_interface::cleanup_thread_ssl_resources();
      break;
    default:
      break;
  }
}

enum_available_interfaces Gcs_interface_factory::from_string(
    const std::string &binding) {
  enum_available_interfaces retval = NONE;

  std::string binding_to_lower;
  binding_to_lower.clear();
  std::transform(binding.begin(), binding.end(),
                 std::back_inserter(binding_to_lower), ::tolower);

  if (binding_to_lower.compare("xcom") == 0) retval = XCOM;

  return retval;
}
