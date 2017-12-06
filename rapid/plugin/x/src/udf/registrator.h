/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef PLUGIN_X_SRC_UDF_REGISTRATOR_H
#define PLUGIN_X_SRC_UDF_REGISTRATOR_H

#include <set>
#include <string>
#include "mysql/components/my_service.h"
#include "mysql/components/services/udf_registration.h"


namespace xpl {
namespace udf {

class Registrator {
 public:
  struct Record {
    const char* m_name;
    Item_result m_result;
    Udf_func_any m_func;
    Udf_func_init m_func_init;
    Udf_func_deinit m_func_deinit;
  };
  using Name_registry = std::set<std::string>;

  Registrator();
  ~Registrator();
  void registration(const Record& udf, Name_registry *udf_names);
  bool unregistration(const std::string &udf_name);
  void unregistration(Name_registry *udf_names);

 private:
  Registrator(const Registrator&) = delete;

  SERVICE_TYPE(registry) * m_registry;
  my_service<SERVICE_TYPE(udf_registration)> m_registrator;
};

}  // namespace udf
}  // namespace xpl

#endif  // PLUGIN_X_SRC_UDF_REGISTRATOR_H
