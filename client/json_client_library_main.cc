/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
#include <utility>

#include "field_types.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql_time.h"
#include "sql-common/json_binary.h"
#include "sql-common/json_dom.h"
#include "sql-common/json_error_handler.h"
#include "sql-common/json_path.h"
#include "sql-common/my_decimal.h"
#include "sql_string.h"
#include "template_utils.h"

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

    // Test call to get_free_space.
    size_t free_space;
    v.get_free_space(CoutSerializationErrorHandler(), &free_space);
    std::cout << "5.1. free space: " << free_space << std::endl;

    std::string std_string;
    if (v.to_pretty_std_string(&std_string, CoutDefaultDepthHandler))
      std::cout << "ERRROR!!" << std::endl;
    std::cout << "5.2. val[" << std_string << "]" << std::endl;
  }

  // Test if contains_wr is exposed
  {
    const std::string json_1{"[{\"key\":123},146]"};
    Json_dom_ptr dom1(Json_dom::parse(
        json_1.c_str(), json_1.length(),
        [](const char *, size_t) { assert(false); }, [] { assert(false); }));
    Json_wrapper json_wrapper = Json_wrapper(std::move(dom1));

    const std::string json_2{"{\"key\":123}"};
    Json_dom_ptr dom2(Json_dom::parse(
        json_2.c_str(), json_2.length(),
        [](const char *, size_t) { assert(false); }, [] { assert(false); }));
    Json_wrapper json_containee = Json_wrapper(std::move(dom2));

    bool result;
    if (json_wrapper_contains(json_wrapper, json_containee, &result)) {
      assert(false);
    } else {
      std::cout << "6.1. it is: " << std::boolalpha << result
                << " that json_1 contains json_2\n";
    }
    if (json_wrapper_contains(json_containee, json_wrapper, &result)) {
      assert(false);
    } else {
      std::cout << "6.2. it is: " << std::boolalpha << result
                << " that json_2 contains json_1\n";
    }
  }

  {
    const std::string json{"[\"Alice2\", 225, 165.5, 155.2, \"female\"]"};
    Json_dom_ptr dom(Json_dom::parse(
        json.c_str(), json.length(),
        [](const char *, size_t) { assert(false); }, [] { assert(false); }));
    Json_array *arr = down_cast<Json_array *>(dom.get());

    Json_dom_ptr dom1(Json_dom::parse(
        "1", 1, [](const char *, size_t) { assert(false); },
        [] { assert(false); }));
    Json_dom_ptr dom2(Json_dom::parse(
        "155.2", 5, [](const char *, size_t) { assert(false); },
        [] { assert(false); }));

    // Sort array and use binary search to lookup values
    arr->sort();
    bool result = arr->binary_search(dom1.get());
    std::cout << "7.1. it is: " << std::boolalpha << result
              << " that array contains 1\n";

    result = arr->binary_search(dom2.get());
    std::cout << "7.2. it is: " << std::boolalpha << result
              << " that array contains 155.2\n";
  }

  // Make sure json_type_name() is visible.
  {
    std::cout << "8.1. Type of empty wrapper is "
              << json_type_name(Json_wrapper{}) << ".\n";
    {
      Json_array array;
      std::cout << "8.2. Type of array is "
                << json_type_name(Json_wrapper{&array, /*alias=*/true})
                << ".\n";
    }
    {
      Json_string string;
      std::cout << "8.3. Type of string is "
                << json_type_name(Json_wrapper{&string, /*alias=*/true})
                << ".\n";
    }
    {
      Json_opaque blob{MYSQL_TYPE_BLOB};
      std::cout << "8.4. Type of blob is "
                << json_type_name(Json_wrapper{&blob, /*alias=*/true}) << ".\n";
    }
  }

  // Make sure coerce_* functions are visible.
  {
    {
      Json_wrapper jw = Json_wrapper(
          create_dom_ptr<Json_string>("2015-01-15 23:24:25.000000"));
      MYSQL_TIME ltime;
      bool res = jw.coerce_date(
          [](const char *, int) { std::cout << "9.1. ERROR \n"; },
          [](MYSQL_TIME_STATUS &) { std::cout << "9.1. checking \n"; }, &ltime);
      std::cout << "9.1. 2015-01-15 23:24:25.000000 is" << (res ? " NOT " : " ")
                << "a valid DATE \n";
    }
    {
      Json_wrapper jw = Json_wrapper(
          create_dom_ptr<Json_string>("2015-99-15 23:24:25.000000"));
      MYSQL_TIME ltime;
      bool res = jw.coerce_date(
          [](const char *, int) { std::cout << "9.2. ERROR \n"; },
          [](MYSQL_TIME_STATUS &) { std::cout << "9.2. checking \n"; }, &ltime);
      std::cout << "9.2. 2015-99-15 23:24:25.000000 is" << (res ? " NOT " : " ")
                << "a valid DATE \n";
    }
    {
      Json_wrapper jw = Json_wrapper(
          create_dom_ptr<Json_string>("2015-99-15 23:24:25.000000"));
      double res = jw.coerce_real(
          [](const char *, int) { std::cout << "9.3. ERROR \n"; });
      std::cout << "9.3. 2015-99-15 23:24:25.000000 coerced to DOUBLE is "
                << res << "\n";
    }
    {
      Json_wrapper jw =
          Json_wrapper(create_dom_ptr<Json_string>("1988.9999999"));
      my_decimal dec;
      jw.coerce_decimal([](const char *, int) { std::cout << "9.4. ERROR \n"; },
                        &dec);
      int len = 128;
      const std::unique_ptr<char[]> buff(new char[len]{'\0'});
      decimal2string(&dec, buff.get(), &len);
      std::cout << "9.4. 1988.9999999 coerced to DECIMAL is " << buff << "\n";
    }
    {
      Json_wrapper jw = Json_wrapper(create_dom_ptr<Json_string>("1988"));
      double res = jw.coerce_int(
          [](const char *, int) { std::cout << "9.5. ERROR \n"; });
      std::cout << "9.5. 1988 coerced to INTEGER is " << res << "\n";
    }
    {
      Json_wrapper jw = Json_wrapper(
          create_dom_ptr<Json_string>("2015-01-15 23:24:25.000000"));
      MYSQL_TIME ltime;
      bool res = jw.coerce_time(
          [](const char *, int) { std::cout << "9.6. ERROR \n"; },
          [](MYSQL_TIME_STATUS &) { std::cout << "9.6. checking \n"; }, &ltime);
      std::cout << "9.6. 2023-12-11 09:23:00.360900 is" << (res ? " NOT " : " ")
                << "a valid TIME \n";
    }
  }

  return 0;
}
