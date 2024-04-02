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

#ifndef ROUTER_TESTS_MRS_CLIENT_JSON_JSON_COPY_POINTERS_H_
#define ROUTER_TESTS_MRS_CLIENT_JSON_JSON_COPY_POINTERS_H_

#include <iterator>
#include <string>
#include <vector>

#include "helper/json/rapid_json_to_struct.h"
#include "json/custom_pointer.h"
#include "json/json_container.h"

namespace json {

class JsonCopyPointers : public helper::json::RapidReaderHandlerToStruct<bool> {
 public:
  using CustomPointers = std::vector<CustomPointer>;
  using Pointers = std::vector<std::string>;
  using Value = rapidjson::Value;
  using StringRefType = Value::StringRefType;
  constexpr static rapidjson::ParseFlag k_parse_flags{rapidjson::kParseNoFlags};

  JsonCopyPointers(const Pointers &pointers = {}, const bool exclusive = false)
      : exclusive_{exclusive} {
    std::copy(pointers.begin(), pointers.end(), std::back_inserter(pointers_));
  }

  bool RawNumber(const Ch *, rapidjson::SizeType, bool) override {
    assert(false &&
           "Configure parser, to call Int, UInt...., versions of this method.");
    return true;
  }

  bool Null() override {
    Handler::Null();
    copy(rapidjson::Type::kNullType);
    return true;
  }

  bool Bool(bool v) override {
    Handler::Bool(v);

    copy(v ? rapidjson::Type::kTrueType : rapidjson::Type::kFalseType);
    return true;
  }

  virtual bool Int(int v) override {
    Handler::Int(v);
    copy(v);
    return true;
  }

  virtual bool Uint(unsigned v) override {
    Handler::Uint(v);
    copy(v);
    return true;
  }

  virtual bool Int64(int64_t v) override {
    Handler::Int64(v);
    copy(v);
    return true;
  }

  virtual bool Uint64(uint64_t v) override {
    Handler::Uint64(v);
    copy(v);
    return true;
  }

  virtual bool Double(double v) override {
    Handler::Double(v);
    copy(v);
    return true;
  }

  bool String(const Ch *str, rapidjson::SizeType len, bool b) override {
    StringRefType sv{str, len};
    Handler::String(str, len, b);
    copy(sv);
    return true;
  }

  void empty_object() override {
    Handler::empty_object();
    copy(rapidjson::Type::kObjectType);
  }

  void empty_array() override {
    Handler::empty_array();
    copy(rapidjson::Type::kArrayType);
  }

  rapidjson::Document &get_document() { return doc_; }

  std::vector<std::string> get_not_matched_pointers() {
    std::vector<std::string> result;
    for (auto &p : pointers_) {
      if (!p.is_marked()) {
        result.push_back(p.get_name());
      }
    }

    return result;
  }

 private:
  template <typename K>
  bool match(const K &k) {
    for (auto &ptr : pointers_) {
      auto k_it = k.begin();
      bool all_ok{true};
      bool non_empty{false};
      for (auto &entry : ptr) {
        auto it = k_it++;
        while (it != k.end() && it->name.empty()) {
          it = k_it++;
        }
        if (it == k.end()) break;
        if (!entry.matches(it->name)) {
          all_ok = false;
          break;
        }
        non_empty = true;
      }

      if (non_empty && all_ok) {
        ptr.mark();
        return true;
      }
    }
    return false;
  }

  template <typename V>
  void copy(const V &v) {
    const auto &keys = get_keys();
    if (exclusive_ ^ !match(keys)) return;

    std::string path;
    jc_.cursor_reset();
    for (auto &k : keys) {
      if (!k.name.empty()) {
        path += k.name + separator_;
      }
      jc_.cursor_move(path, k.name, k.is_array, k.leaf);
    }

    Value jv{v};
    jc_.cursor_set_value(jv);
  }

  bool exclusive_;
  rapidjson::Document doc_;
  JsonContainer jc_{&doc_};
  CustomPointers pointers_;
};

}  // namespace json

#endif  // ROUTER_TESTS_MRS_CLIENT_JSON_JSON_COPY_POINTERS_H_
