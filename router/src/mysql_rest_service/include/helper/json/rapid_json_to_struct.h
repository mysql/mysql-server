/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_RAPID_JSON_TO_STRUCT_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_RAPID_JSON_TO_STRUCT_H_

#include <list>
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
template <typename UserResult>
class RapidReaderHandlerToStruct
    : public rapidjson::BaseReaderHandler<
          rapidjson::UTF8<>, RapidReaderHandlerToStruct<UserResult>> {
 public:
  constexpr static rapidjson::ParseFlag k_parse_flags{
      rapidjson::kParseNumbersAsStringsFlag};

  using Handler = RapidReaderHandlerToStruct<UserResult>;
  using Parent =
      rapidjson::BaseReaderHandler<rapidjson::UTF8<>,
                                   RapidReaderHandlerToStruct<UserResult>>;
  using Ch = typename Parent::Ch;
  RapidReaderHandlerToStruct(const std::string &separator = ".")
      : separator_{separator} {}
  virtual ~RapidReaderHandlerToStruct() = default;

  using Result = UserResult;
  const UserResult &get_result() { return result_; }

  virtual bool on_new_value() {
    ++values_processed_;
    auto &pk = get_parent_key();
    if (pk.is_array) {
      key_.name = std::to_string(pk.array_idx++);
      key_.level = level_;

      return true;
    }
    return false;
  }

 public:  // template overwrites methods from `rapidjson::BaseReaderHandler`
  virtual bool Null() {
    on_new_value();
    return true;
  }

  virtual bool Bool(bool) {
    on_new_value();
    return true;
  }

  virtual bool String(const Ch *, rapidjson::SizeType, bool) {
    on_new_value();
    return true;
  }

  virtual bool Int(int) {
    on_new_value();
    return true;
  }

  virtual bool Uint(unsigned) {
    on_new_value();
    return true;
  }

  virtual bool Int64(int64_t) {
    on_new_value();
    return true;
  }

  virtual bool Uint64(uint64_t) {
    on_new_value();
    return true;
  }

  virtual bool Double(double) {
    on_new_value();
    return true;
  }

  virtual void empty_object() {}

  virtual void empty_array() {}

  /// enabled via kParseNumbersAsStringsFlag, string is not null-terminated (use
  /// length)
  virtual bool RawNumber(const Ch *, rapidjson::SizeType, bool) {
    on_new_value();
    return true;
  }

  bool StartObject() {
    on_new_value();
    auto k = key_;
    k.processed = values_processed_;
    keys_.push_back(k);
    ++level_;
    return true;
  }

  bool EndObject(rapidjson::SizeType) {
    --level_;
    if (!keys_.empty()) {
      auto &b = keys_.back();
      if (level_ == b.level) {
        if (b.processed == values_processed_) empty_object();
        keys_.pop_back();
      }
    }

    return true;
  }

  bool Key(const Ch *str, rapidjson::SizeType len, bool) {
    key_.name.assign(str, len);
    key_.level = level_;
    return true;
  }

  // Ignore arrays
  bool StartArray() {
    on_new_value();
    auto k = key_;
    k.is_array = true;
    k.array_idx = 1;
    k.processed = values_processed_;
    keys_.push_back(k);

    ++level_;
    ++arrays_;
    return true;
  }

  bool EndArray(rapidjson::SizeType) {
    --level_;
    --arrays_;
    if (!keys_.empty()) {
      auto &b = keys_.back();
      if (level_ == b.level) {
        if (b.processed == values_processed_) empty_array();
        keys_.pop_back();
      }
    }
    return true;
  }

 protected:
  UserResult result_;

  bool is_object_path() { return level_ > 0 && arrays_ == 0; }
  bool is_array_value() { return arrays_ > 0; }
  int get_level() const { return level_; }

  std::string get_current_key() const {
    std::string result;
    for (const auto &key : keys_) {
      // Skip first elemnt if its an array
      if (key.name.empty()) continue;

      result += key.name + separator_;
    }
    return result + key_.name;
  }

  const std::string separator_;

  struct KeyValue {
    std::string name;
    bool is_array{false};
    int array_idx{0};
    int level{0};
    bool leaf{false};
    uint64_t processed{0};
  };

  std::list<KeyValue> get_keys() const {
    auto keys = keys_;
    auto key = key_;
    key.leaf = true;
    keys.push_back(key);
    return keys;
  }

 private:
  KeyValue &get_parent_key() {
    if (keys_.empty()) {
      static KeyValue k_empty;
      return k_empty;
    }

    return keys_.back();
  }

  uint64_t values_processed_{0};
  std::list<KeyValue> keys_;
  KeyValue key_;
  int level_{0};
  int arrays_{0};
};

}  // namespace json
}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_RAPID_JSON_TO_STRUCT_H_
