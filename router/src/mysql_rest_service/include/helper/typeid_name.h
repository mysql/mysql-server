/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_HELPER_TYPEID_NAME_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_HELPER_TYPEID_NAME_H_

#ifdef HAVE_ABI_CXA_DEMANGLE
#include <cxxabi.h>
#endif  // HAVE_ABI_CXA_DEMANGLE

namespace helper {

template <typename Type>
std::string type_name() {
  auto name = typeid(Type).name();
#ifdef HAVE_ABI_CXA_DEMANGLE
  int status = 0;
  char *const readable_name =
      abi::__cxa_demangle(name, nullptr, nullptr, &status);
  if (status == 0) {
    std::string ret_string = readable_name;
    free(readable_name);
    return ret_string;
  }
#endif  // HAVE_ABI_CXA_DEMANGLE
  return name;
}

}  // namespace helper

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_INCLUDE_HELPER_TYPEID_NAME_H_ \
        */
