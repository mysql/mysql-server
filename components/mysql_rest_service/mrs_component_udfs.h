/*
  Copyright (c) 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MRS_COMPONENT_UDF_MANAGER_H
#define MRS_COMPONENT_UDF_MANAGER_H

#include <cstring>
#include <set>
#include <string>

#include "mysql/components/services/udf_registration.h"

namespace mrs_component {

// UDF layout:
struct UDF {
  UDF() = default;

  const char *name = nullptr;
  enum Item_result return_type;
  Udf_func_any function = nullptr;
  Udf_func_init init_function = nullptr;
  Udf_func_deinit deinit_function = nullptr;

  // operator< to allow comparisons for std::set
  bool operator<(const UDF &other) const {
    return std::strcmp(name, other.name) < 0;
  }
};

/**
  Owns and manages the UDFs provided by the MRS component.
*/
class UDFs {
 public:
  UDFs() = default;
  ~UDFs() = default;
  UDFs(const UDFs &) = delete;
  UDFs &operator=(const UDFs &) = delete;
  UDFs(UDFs &&) = delete;
  UDFs &operator=(UDFs &&) = delete;

  int initialize_all();
  int register_all();
  int unregister_all();

  static void throw_udf_error(const char *udf_name, const char *error_message);

  // UDF callbacks
  static bool mysql_rest_service_restart_init(UDF_INIT *initid, UDF_ARGS *args,
                                              char *message);
  static void mysql_rest_service_restart_deinit(UDF_INIT *initid
                                                [[maybe_unused]]);
  static long long mysql_rest_service_restart(UDF_INIT *, UDF_ARGS *args,
                                              unsigned char *,
                                              unsigned char *error);

  static bool mysql_rest_service_stop_init(UDF_INIT *initid, UDF_ARGS *args,
                                           char *message);
  static void mysql_rest_service_stop_deinit(UDF_INIT *initid [[maybe_unused]]);
  static long long mysql_rest_service_stop(UDF_INIT *, UDF_ARGS *args,
                                           unsigned char *,
                                           unsigned char *error);

  static bool mysql_rest_service_start_init(UDF_INIT *initid, UDF_ARGS *args,
                                            char *message);
  static void mysql_rest_service_start_deinit(UDF_INIT *initid
                                              [[maybe_unused]]);
  static long long mysql_rest_service_start(UDF_INIT *, UDF_ARGS *args,
                                            unsigned char *,
                                            unsigned char *error);

 private:
  std::set<UDF> m_udf_list;
  std::set<std::string> m_registered_udfs;
};

}  // namespace mrs_component

#endif  // MRS_COMPONENT_UDF_MANAGER_H
