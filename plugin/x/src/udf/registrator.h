/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef PLUGIN_X_SRC_UDF_REGISTRATOR_H_
#define PLUGIN_X_SRC_UDF_REGISTRATOR_H_

#include <set>
#include <string>
#include "mysql/components/my_service.h"
#include "mysql/components/services/udf_registration.h"

namespace xpl {
namespace udf {

class Registrator {
 public:
  struct Record {
    const char *m_name;
    Item_result m_result;
    Udf_func_any m_func;
    Udf_func_init m_func_init;
    Udf_func_deinit m_func_deinit;
  };
  using Name_registry = std::set<std::string>;

  Registrator();
  ~Registrator();
  void registration(const Record &udf, Name_registry *udf_names);
  bool unregistration(const std::string &udf_name);
  void unregistration(Name_registry *udf_names);

 private:
  Registrator(const Registrator &) = delete;

  SERVICE_TYPE(registry) * m_registry;
  my_service<SERVICE_TYPE(udf_registration)> m_registrator;
};

}  // namespace udf
}  // namespace xpl

#endif  // PLUGIN_X_SRC_UDF_REGISTRATOR_H_
