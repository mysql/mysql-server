/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

/*
  This is a test executable which verifies that Json_wrapper::seek,
  Json_dom::seek and Json_dom::parse functions are visible and can be called.
 */
#include <cstring>
#include <iostream>
#include <string>

#include "sql-common/json_dom.h"
#include "sql-common/json_path.h"

#include "sql_string.h"

int main() {
  Json_object o;
  for (size_t i = 0; i < 1000; ++i) {
    o.add_alias(std::to_string(i),
                new (std::nothrow)
                    Json_string(std::string("key_") + std::to_string(i)));
  }

  // Make sure Json_wrapper::seek is visible
  Json_wrapper_vector hits(PSI_NOT_INSTRUMENTED);
  bool need_only_one{false};
  const char *json_path = R"($**."512")";
  Json_path path(PSI_NOT_INSTRUMENTED);
  size_t bad_index;
  parse_path(std::strlen(json_path), json_path, &path, &bad_index,
             [] { assert(false); });

  Json_wrapper wr(&o);
  wr.set_alias();
  wr.seek(path, path.leg_count(), &hits, true, need_only_one);

  for (auto &hit : hits) {
    String buffer;
    hit.to_string(&buffer, false, nullptr, [] { assert(false); });
    std::cout << std::string_view{buffer.ptr(), buffer.length()} << std::endl;
  }

  // Make sure Json_dom::parse is visible and error handling works
  {
    std::string json{"[{\"key\":123},146]"};
    Json_dom_ptr dom(Json_dom::parse(
        json.c_str(), json.length(),
        [](const char *, size_t) { assert(false); }, [] { assert(false); }));
    if (dom != nullptr) std::cout << "1. success" << std::endl;
  }

  {
    std::string json{
        "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["
        R"([[[[[[[[[[[[[[[[[[[[[[[[[[[[[[{"key":123}]]]]]]]]]]]]]]]]]]]]]]]]]])"
        "]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]"
        "]]]]"};
    Json_dom_ptr dom(Json_dom::parse(
        json.c_str(), json.length(),
        [](const char *err_mesg, size_t err_code) {
          std::cout << "2. parse_error [" << err_code << "]: " << err_mesg
                    << std::endl;
        },
        [] { std::cout << "2. depth_error" << std::endl; }));
    if (dom != nullptr) assert(false);
  }

  {
    std::string json{"[&&"};
    Json_dom_ptr dom(Json_dom::parse(
        json.c_str(), json.length(),
        [](const char *err_mesg, size_t err_code) {
          std::cout << "3. parse_error [" << err_code << "]: " << err_mesg
                    << std::endl;
        },
        [] { assert(false); }));
    if (dom != nullptr) assert(false);
  }

  return 0;
}
