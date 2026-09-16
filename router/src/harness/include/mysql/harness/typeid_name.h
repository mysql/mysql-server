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

#ifndef MYSQL_HARNESS_TYPEID_NAME_INCLUDED
#define MYSQL_HARNESS_TYPEID_NAME_INCLUDED

#include <cstdlib>
#include <memory>
#include <string>
#include <typeinfo>

#ifdef HAVE_ABI_CXA_DEMANGLE
#include <cxxabi.h>
#endif  // HAVE_ABI_CXA_DEMANGLE

namespace mysql_harness {

inline std::string typeid_name(const std::type_info &ti) {
#ifdef HAVE_ABI_CXA_DEMANGLE
  int status = 0;
  std::unique_ptr<char, decltype(&std::free)> demangled{
      abi::__cxa_demangle(ti.name(), nullptr, nullptr, &status), &std::free};

  if (status == 0 && demangled != nullptr) {
    return demangled.get();
  }
#endif  // HAVE_ABI_CXA_DEMANGLE

  return ti.name();
}

template <class T>
std::string typeid_name(const T &v) {
  return typeid_name(typeid(v));
}

template <class T>
std::string typeid_name() {
  return typeid_name(typeid(T));
}

}  // namespace mysql_harness

#endif  // MYSQL_HARNESS_TYPEID_NAME_INCLUDED
