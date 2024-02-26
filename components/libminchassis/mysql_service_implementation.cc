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

#include "mysql_service_implementation.h"

mysql_service_implementation::mysql_service_implementation(
    my_h_service interface, const char *full_name)
    : my_ref_counted(),
      my_metadata(),
      m_interface(interface),
      m_full_name(full_name) {
  my_string::size_type first_dot;

  first_dot = m_full_name.find_first_of('.');

  if (m_full_name.length() < 3 || /* dot and at least 1 char for each part */
      first_dot < 1 || first_dot >= m_full_name.length() - 1 ||
      first_dot != m_full_name.find_last_of('.')) {
    /* Set interface to NULL, this should be interpreted as failure to
      construct the object instance. */
    m_interface = {};
    return;
  }

  m_service = my_string(m_full_name.begin(), m_full_name.begin() + first_dot);
}

mysql_service_implementation::mysql_service_implementation(
    mysql_service_implementation &)

    = default;

/**
  Gets service name that is implemented by this service implementation.

  @return Pointer to service name. Pointer is valid till service name is not
    changed or service implementation is unregistered.
*/
const char *mysql_service_implementation::service_name_c_str() const {
  return m_service.c_str();
}

/**
  Gets fully qualified name of this service implementation.

  @return Pointer to service name. Pointer is valid till service name is not
    changed or service implementation is unregistered.
*/
const char *mysql_service_implementation::name_c_str() const {
  return m_full_name.c_str();
}

/**
  Gets pointer to interface structure with method pointers.

  @return Pointer to interface structure.
*/
my_h_service mysql_service_implementation::interface() const {
  return m_interface;
}
