/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_RAPID_JSON_TO_MAP_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_RAPID_JSON_TO_MAP_H_

#include <string>

#include <my_rapidjson_size_t.h>
#include <rapidjson/reader.h>

namespace helper {
namespace json {

/**
 * This class is a adapter for Reader from RapidJson.
 *
 * This class adapts std::map of strings (keys and values are strings)
 * to be destination of text document conversion done be `rapidjson::Reader`.
 * There are some constrains to what is converted:
 *    * values from top level document are inserted into the map,
 *    * value must be a simple type, sub objects, or arrays are ignored.
 */
class RapidReaderHandlerToMapOfSimpleValues
    : rapidjson::BaseReaderHandler<rapidjson::UTF8<>,
                                   RapidReaderHandlerToMapOfSimpleValues> {
 public:
  using Map = std::map<std::string, std::string>;
  using Result = Map;

  const Map &get_result() const { return result_; }

 public:  // template overwrites methods from `rapidjson::BaseReaderHandler`
  bool Null() { return String("null", 4, false); }

  bool Bool(bool value) {
    if (value) return String("true", 4, false);

    return String("false", 5, false);
  }

  bool String(const Ch *ch, rapidjson::SizeType size, bool) {
    if (level_ != 1) return true;

    result_[key_] = std::string(ch, size);

    return static_cast<Override &>(*this).Default();
  }

  // Ignoring following methods because, parser should be configured to call
  // "RawNumber" method.
  bool Int(int) { return true; }
  bool Uint(unsigned) { return true; }
  bool Int64(int64_t) { return true; }
  bool Uint64(uint64_t) { return true; }
  bool Double(double) { return true; }

  /// enabled via kParseNumbersAsStringsFlag, string is not null-terminated (use
  /// length)
  bool RawNumber(const Ch *str, rapidjson::SizeType len, bool copy) {
    return String(str, len, copy);
  }

  bool StartObject() {
    ++level_;
    return true;
  }

  bool EndObject(rapidjson::SizeType) {
    --level_;
    return true;
  }

  bool Key(const Ch *str, rapidjson::SizeType len, bool) {
    if (level_ != 1) return true;
    key_.assign(str, len);
    return true;
  }

  // Ignore arrays
  bool StartArray() {
    ++level_;
    return true;
  }

  bool EndArray(rapidjson::SizeType) {
    --level_;
    return true;
  }

 private:
  std::string key_;
  Map result_;
  int level_{0};
};

template <typename SubHandler>
class ExtractSubObjectHandler
    : rapidjson::BaseReaderHandler<rapidjson::UTF8<>,
                                   ExtractSubObjectHandler<SubHandler>> {
  using Base =
      rapidjson::BaseReaderHandler<rapidjson::UTF8<>,
                                   ExtractSubObjectHandler<SubHandler>>;
  using Ch = typename Base::Ch;

 public:
  using Result = typename SubHandler::Result;

  ExtractSubObjectHandler(const std::string &key, SubHandler &sub_handler)
      : key_{key}, sub_handler_{sub_handler} {}

  bool Null() {
    if (targer_) return sub_handler_.Null();

    return true;
  }

  bool Bool(bool value) {
    if (targer_) return sub_handler_.Bool(value);

    return true;
  }

  bool String(const Ch *ch, rapidjson::SizeType size, bool b) {
    if (targer_) return sub_handler_.String(ch, size, b);

    return true;
  }

  bool Int(int v) {
    if (targer_) return sub_handler_.Int(v);

    return true;
  }

  bool Uint(unsigned v) {
    if (targer_) return sub_handler_.Uint(v);

    return true;
  }

  bool Int64(int64_t v) {
    if (targer_) return sub_handler_.Int64(v);

    return true;
  }

  bool Uint64(uint64_t v) {
    if (targer_) return sub_handler_.Uint64(v);

    return true;
  }

  bool Double(double v) {
    if (targer_) return sub_handler_.Double(v);

    return true;
  }

  /// enabled via kParseNumbersAsStringsFlag, string is not null-terminated (use
  /// length)
  bool RawNumber(const Ch *str, rapidjson::SizeType len, bool copy) {
    if (targer_) return sub_handler_.RawNumber(str, len, copy);

    return true;
  }

  bool StartObject() {
    if (targer_) sub_handler_.StartObject();
    ++level_;
    return true;
  }

  bool EndObject(rapidjson::SizeType size) {
    if (targer_) sub_handler_.EndObject(size);
    --level_;
    return true;
  }

  bool Key(const Ch *str, rapidjson::SizeType len, bool b) {
    if (targer_) sub_handler_.Key(str, len, b);
    if (level_ != 1) return true;
    targer_ = key_ == std::string(str, len);
    return true;
  }

  // Ignore arrays
  bool StartArray() {
    if (targer_) sub_handler_.StartArray();
    ++level_;
    return true;
  }

  bool EndArray(rapidjson::SizeType size) {
    if (targer_) sub_handler_.EndArray(size);
    --level_;
    return true;
  }

  Result get_result() { return sub_handler_.get_result(); }

 private:
  std::string key_;
  SubHandler &sub_handler_;
  int level_{0};
  bool targer_{false};
};

}  // namespace json
}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_RAPID_JSON_TO_MAP_H_
