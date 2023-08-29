/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
  Json_dom::seek, Json_dom::parse, json_binary::parse_binary,
  json_binary::serialize functions are visible and can be called.
 */

#include <cassert>
#include <cstring>
#include <iostream>
#include <memory>
#include <new>
#include <string>
#include <string_view>

#include "field_types.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql_time.h"
#include "sql-common/json_binary.h"
#include "sql-common/json_dom.h"
#include "sql-common/json_error_handler.h"
#include "sql-common/json_path.h"
#include "sql-common/my_decimal.h"
#include "sql_string.h"

namespace {
void CoutDefaultDepthHandler() { std::cout << "Doc too deep"; }

class CoutSerializationErrorHandler : public JsonSerializationErrorHandler {
 public:
  void KeyTooBig() const override { std::cout << "Key too big"; }
  void ValueTooBig() const override { std::cout << "Value too big"; }
  void TooDeep() const override { CoutDefaultDepthHandler(); }
  void InvalidJson() const override { std::cout << "Invalid JSON"; }
  void InternalError(const char *message) const override {
    std::cout << "Internal error: " << message;
  }
  bool CheckStack() const override {
    std::cout << "Checking stack\n";
    return false;
  }
};

}  // namespace

int main() {
  Json_object o;
  for (size_t i = 0; i < 1000; ++i) {
    o.add_alias(std::to_string(i),
                new (std::nothrow)
                    Json_string(std::string("key_") + std::to_string(i)));
  }

  // Make sure Json_wrapper::seek is visible
  Json_wrapper_vector hits(PSI_NOT_INSTRUMENTED);
  const bool need_only_one{false};
  const char *json_path = R"($**."512")";
  Json_path path(PSI_NOT_INSTRUMENTED);
  size_t bad_index;
  parse_path(std::strlen(json_path), json_path, &path, &bad_index);

  Json_wrapper wr(&o);
  wr.set_alias();

  StringBuffer<20000> buf;
  if (wr.to_binary(CoutSerializationErrorHandler(), &buf)) {
    std::cout << "error";
  } else {
    const json_binary::Value v(
        json_binary::parse_binary(buf.ptr(), buf.length()));
    std::string std_string;
    v.to_pretty_std_string(&std_string, CoutDefaultDepthHandler);
    std::cout << "1. Binary_val[" << std_string << "]" << std::endl;
  }

  wr.seek(path, path.leg_count(), &hits, true, need_only_one);

  for (auto &hit : hits) {
    String buffer;
    hit.to_string(&buffer, false, nullptr, [] { assert(false); });
    std::cout << std::string_view{buffer.ptr(), buffer.length()} << std::endl;
  }

  // Make sure Json_dom::parse is visible and error handling works
  {
    const std::string json{"[{\"key\":123},146]"};
    Json_dom_ptr dom(Json_dom::parse(
        json.c_str(), json.length(),
        [](const char *, size_t) { assert(false); }, [] { assert(false); }));
    if (dom != nullptr) std::cout << "1. success" << std::endl;
  }

  {
    const std::string json{
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
    const std::string json{"[&&"};
    Json_dom_ptr dom(Json_dom::parse(
        json.c_str(), json.length(),
        [](const char *err_mesg, size_t err_code) {
          std::cout << "3. parse_error [" << err_code << "]: " << err_mesg
                    << std::endl;
        },
        [] { assert(false); }));
    if (dom != nullptr) assert(false);
  }

  {
    my_decimal m;
    double2my_decimal(0, 3.14000000001, &m);
    const Json_decimal jd(m);
    if (json_binary::serialize(&jd, CoutSerializationErrorHandler(), &buf))
      std::cout << "ERRROR!!" << std::endl;

    json_binary::Value v = json_binary::parse_binary(buf.ptr(), buf.length());
    std::string std_string;
    if (v.to_std_string(&std_string, CoutDefaultDepthHandler))
      std::cout << "ERRROR!!" << std::endl;
    std::cout << "4. val[" << std_string << "]" << std::endl;
  }

  {
    /* DATETIME scalar */
    MYSQL_TIME dt;
    std::memset(&dt, 0, sizeof dt);
    dt.year = 1988;
    dt.month = 12;
    dt.day = 15;
    dt.time_type = MYSQL_TIMESTAMP_DATE;
    const Json_datetime jd(dt, MYSQL_TYPE_DATETIME);

    if (json_binary::serialize(&jd, CoutSerializationErrorHandler(), &buf))
      std::cout << "ERRROR!!" << std::endl;

    json_binary::Value v = json_binary::parse_binary(buf.ptr(), buf.length());
    std::string std_string;
    if (v.to_pretty_std_string(&std_string, CoutDefaultDepthHandler))
      std::cout << "ERRROR!!" << std::endl;
    std::cout << "5. val[" << std_string << "]" << std::endl;
  }

  return 0;
}
