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

#ifndef MYSQL_COMPONENT_H
#define MYSQL_COMPONENT_H

#include <mysql/components/services/dynamic_loader.h>
#include <vector>
#include "my_metadata.h"

/**
  Wraps st_mysql_component_t component data conforming ABI into C++ object.
*/
class mysql_component : public my_metadata {
 public:
  mysql_component(mysql_component_t *component_data, my_string urn);

  const char *name_c_str() const;
  const char *urn_c_str() const;
  const my_string &get_urn() const;

  std::vector<const mysql_service_ref_t *> get_provided_services() const;
  std::vector<mysql_service_placeholder_ref_t *> get_required_services() const;

  const mysql_component_t *get_data() const;

 private:
  mysql_component_t *m_component_data;
  my_string m_urn;
};

#endif /* MYSQL_COMPONENT_H */
