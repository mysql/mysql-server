/* Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02111-1307  USA */

#ifndef MYSQL_SERVICE_IMPLEMENTATION_H
#define MYSQL_SERVICE_IMPLEMENTATION_H

#include "my_ref_counted.h"
#include "my_metadata.h"
#include <mysql/components/services/registry.h>

/** a Service implementation registry data */
class mysql_service_implementation : public my_ref_counted, public my_metadata
{
public:
  mysql_service_implementation(
    my_h_service interface, const char *full_name);
  mysql_service_implementation(mysql_service_implementation& other);

  const char* service_name_c_str() const;
  const char* name_c_str() const;
  my_h_service interface() const;

private:
  my_h_service m_interface;
  my_string m_service;
  my_string m_full_name;
};

#endif /* MYSQL_SERVICE_IMPLEMENTATION_H */
