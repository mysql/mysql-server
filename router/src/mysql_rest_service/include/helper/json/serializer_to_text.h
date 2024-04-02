/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_SERIALIZERTOTEXT_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_SERIALIZERTOTEXT_H_

#include <sstream>

#include "my_rapidjson_size_t.h"

#include <rapidjson/document.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/writer.h>

#include "mrs/database/entry/universal_id.h"

#include "helper/mysql_column_types.h"
#include "helper/optional.h"

namespace helper {
namespace json {

class SerializerToText {
 public:
  class Object {
   public:
    Object(Object &&other) { *this = std::move(other); }

    Object(SerializerToText *serializer = nullptr) : serializer_{serializer} {
      initialize();
    }

    ~Object() { finalize(); }

    SerializerToText *operator->() { return serializer_; }
    bool is_usable() const { return !finalized_; }

    Object &operator=(Object &&other) {
      finalize();
      serializer_ = other.serializer_;
      finalized_ = other.finalized_;
      other.finalized_ = true;
      return *this;
    }

   private:
    void initialize() {
      if (!serializer_) return;
      serializer_->writer_.StartObject();
      finalized_ = false;
    }

    void finalize() {
      if (!serializer_) return;
      if (!finalized_) serializer_->writer_.EndObject();
      finalized_ = true;
    }

    SerializerToText *serializer_{nullptr};
    bool finalized_{true};
  };

  class Array {
   public:
    Array(Array &&other) { *this = std::move(other); }
    Array(SerializerToText *serializer = nullptr) : serializer_{serializer} {
      initialize();
    }
    ~Array() { finalize(); }

    SerializerToText *operator->() { return serializer_; }
    SerializerToText &operator*() { return *serializer_; }
    bool is_usable() const { return finalized_; }

    Array &operator=(Array &&other) {
      finalize();
      serializer_ = other.serializer_;
      finalized_ = other.finalized_;
      other.finalized_ = true;
      return *this;
    }

    template <typename Arr>
    Array &add(const Arr &arr) {
      auto it = std::begin(arr);
      while (it != std::end(arr)) {
        *serializer_ << *it;
        ++it;
      }

      return *this;
    }

   private:
    void initialize() {
      if (!serializer_) return;
      serializer_->writer_.StartArray();
      finalized_ = false;
    }

    void finalize() {
      if (!serializer_) return;
      if (!finalized_) serializer_->writer_.EndArray();
      finalized_ = true;
    }

    SerializerToText *serializer_{nullptr};
    bool finalized_{false};
  };

 public:
  explicit SerializerToText(const bool bigint_encode_as_string = false)
      : bigint_encode_as_string_{bigint_encode_as_string} {}

  std::string get_result() {
    writer_.Flush();
    return value_.str();
  }

  Object add_object() { return Object(this); }
  Array add_array() { return Array(this); }

  SerializerToText &operator<<(const char *value) {
    add_value(value, JsonType::kString);
    return *this;
  }

  SerializerToText &operator<<(const std::string &value) {
    add_value(value.c_str(), value.length(), JsonType::kString);
    return *this;
  }

  SerializerToText &operator<<(const int value) {
    writer_.Int(value);
    return *this;
  }

  SerializerToText &operator<<(const unsigned int value) {
    writer_.Uint(value);
    return *this;
  }

  SerializerToText &operator<<(const uint64_t value) {
    if (!bigint_encode_as_string_) {
      writer_.Uint64(value);
    } else {
      auto str = std::to_string(value);
      writer_.String(str.c_str(), str.length());
    }
    return *this;
  }

  SerializerToText &operator<<(const int64_t value) {
    if (!bigint_encode_as_string_) {
      writer_.Int64(value);
    } else {
      auto str = std::to_string(value);
      writer_.String(str.c_str(), str.length());
    }
    return *this;

    return *this;
  }

  SerializerToText &operator<<(const bool value) {
    writer_.Bool(value);
    return *this;
  }

  SerializerToText &add_value(const char *value,
                              JsonType ct = JsonType::kString) {
    return add_value(value, value ? strlen(value) : 0, ct);
  }

  SerializerToText &add_value(const char *value, uint32_t length, JsonType ct) {
    if (!value) {
      writer_.Null();
      return *this;
    }

    switch (ct) {
      case JsonType::kJson:
      case JsonType::kBool: {
        writer_.RawValue(value, length, rapidjson::kObjectType);
      } break;

      case JsonType::kNull:
        writer_.Null();
        break;

      case JsonType::kNumeric:
        // This functions should take into account `bigint_encode_as_string_`.
        writer_.RawValue(value, length, rapidjson::kNumberType);
        break;

      case JsonType::kBlob:
      case JsonType::kString:
        writer_.String(value, length);
        break;
    }

    return *this;
  }

  SerializerToText &add_value(const rapidjson::Value &value) {
    rapidjson::StringBuffer json_buf;
    {
      rapidjson::Writer<rapidjson::StringBuffer> json_writer(json_buf);

      value.Accept(json_writer);
    }

    return add_value(json_buf.GetString(), json_buf.GetLength(),
                     JsonType::kJson);
  }

  void flush() { writer_.Flush(); }

  Array member_add_array(const char *key) {
    writer_.Key(key);
    return Array(this);
  }

  Object member_add_object(const char *key) {
    writer_.Key(key);
    return Object(this);
  }

  template <typename Str1, typename Str2>
  SerializerToText &member_add_value(const Str1 &key, const Str2 &value,
                                     JsonType ct) {
    add_member_impl(get_raw(key), get_raw(value), ct);
    return *this;
  }

  template <typename Str1, typename Value>
  SerializerToText &member_add_value(const Str1 &key, Value &&value) {
    add_member_impl(get_raw(key), std::forward<Value>(value));
    return *this;
  }

  template <typename Str1>
  SerializerToText &member_add_value(const Str1 &key, const char *str,
                                     uint32_t len) {
    writer_.Key(key);
    add_value(str, len, helper::JsonType::kString);
    return *this;
  }

  template <typename Str1>
  SerializerToText &member_add_value(const Str1 &key, const char *str,
                                     uint32_t len, JsonType ct) {
    writer_.Key(key);
    add_value(str, len, ct);
    return *this;
  }

 private:
  const char *get_raw(const char *value) { return value; }
  const char *get_raw(const std::string &value) { return value.c_str(); }
  const char *get_raw(const bool value) { return value ? "1" : "0"; }

  template <typename Value>
  void add_member_impl(const char *key, Value &&value) {
    writer_.Key(key);
    *this << (std::forward<Value>(value));
  }

  template <typename Value>
  void add_member_impl(const char *key, helper::Optional<Value> &value) {
    if (value) {
      writer_.Key(key);
      *this << (value.value());
    }
  }

  template <typename Value>
  void add_member_impl(const char *key, const helper::Optional<Value> &value) {
    if (value) {
      writer_.Key(key);
      *this << (value.value());
    }
  }

  void add_member_impl(const char *key, const char *value, JsonType ct) {
    writer_.Key(key);
    add_value(value, ct);
  }

  bool bigint_encode_as_string_{false};
  std::stringstream value_;
  rapidjson::OStreamWrapper ostream_{value_};
  rapidjson::Writer<rapidjson::OStreamWrapper> writer_{ostream_};
};

}  // namespace json
}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_SERIALIZERTOTEXT_H_
