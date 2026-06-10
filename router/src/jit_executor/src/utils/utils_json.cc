/*
 * Copyright (c) 2015, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "utils/utils_json.h"
// #include "mysqlshdk/libs/utils/utils_json.h"

#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <cstring>

// #include "mysqlshdk/include/scripting/types.h"
#include "mysqlrouter/jit_executor_value.h"
// #include "mysqlshdk/libs/utils/dtoa.h"
#include "mysql/strings/dtoa.h"
// #include "mysqlshdk/libs/utils/utils_encoding.h"
#include "utils/utils_encoding.h"
//  #include "mysqlshdk/libs/utils/utils_string.h"
#include "native_wrappers/polyglot_object_bridge.h"
#include "polyglot_wrappers/types_polyglot.h"
#include "utils/utils_string.h"

namespace shcore {

namespace {

size_t fmt_double(double d, char *buffer, size_t buffer_len) {
  return my_gcvt(d, MY_GCVT_ARG_DOUBLE, buffer_len, buffer, NULL);
}

/**
 * Raw JSON wrapper with custom conversion of the Double value using
 * the my_gcvt function ported from the server, which generates a string
 * value with the most number of significat digits and thus precision.
 */
template <typename T>
class My_writer : public rapidjson::Writer<T> {
 public:
  explicit My_writer(T &outstream) : rapidjson::Writer<T>(outstream) {}

  bool Double(double data) {
    char buffer[32];
    size_t len = fmt_double(data, buffer, sizeof(buffer));

    rapidjson::Writer<T>::Prefix(rapidjson::kNumberType);
    return rapidjson::Writer<T>::WriteRawValue(buffer, len);
  }
};

/**
 * Pretty JSON wrapper with custom conversion of the Double value using
 * the my_gcvt function ported from the server, which generates a string
 * value with the most number of significat digits and thus precision.
 */
template <typename T>
class My_pretty_writer : public rapidjson::PrettyWriter<T> {
 public:
  explicit My_pretty_writer(T &outstream)
      : rapidjson::PrettyWriter<T>(outstream) {}

  bool Double(double data) {
    char buffer[32];
    size_t len = fmt_double(data, buffer, sizeof(buffer));

    rapidjson::PrettyWriter<T>::PrettyPrefix(rapidjson::kNumberType);
    return rapidjson::PrettyWriter<T>::WriteRawValue(buffer, len);
  }
};

class String_stream {
 public:
  using Ch = std::string::value_type;

  void Put(Ch c) { data += c; }
  void Flush() {}

  std::string data;
};

}  // namespace

class JSON_dumper::Writer_base {
 public:
  virtual ~Writer_base() = default;

  virtual void start_array() = 0;
  virtual void end_array() = 0;
  virtual void start_object() = 0;
  virtual void end_object() = 0;
  virtual void append_null() = 0;
  virtual void append_bool(bool data) = 0;
  virtual void append_int(int data) = 0;
  virtual void append_int64(int64_t data) = 0;
  virtual void append_uint(unsigned int data) = 0;
  virtual void append_uint64(uint64_t data) = 0;
  virtual void append_string(std::string_view data) = 0;
  virtual void append_float(double data) = 0;
  virtual void append_document(const rapidjson::Document &document) = 0;

  virtual const std::string &str() const = 0;
};

namespace {

template <template <class> class Writer>
class Stream_writer : public JSON_dumper::Writer_base {
 public:
  Stream_writer() : m_writer(m_data) {}

  ~Stream_writer() override = default;

  void start_array() override { m_writer.StartArray(); }

  void end_array() override { m_writer.EndArray(); }

  void start_object() override { m_writer.StartObject(); }

  void end_object() override { m_writer.EndObject(); }

  void append_null() override { m_writer.Null(); }

  void append_bool(bool data) override { m_writer.Bool(data); }

  void append_int(int data) override { m_writer.Int(data); }

  void append_int64(int64_t data) override { m_writer.Int64(data); }

  void append_uint(unsigned int data) override { m_writer.Uint(data); }

  void append_uint64(uint64_t data) override { m_writer.Uint64(data); }

  void append_string(std::string_view data) override {
    m_writer.String(data.data(), unsigned(data.length()));
  }

  void append_float(double data) override { m_writer.Double(data); }

  void append_document(const rapidjson::Document &document) override {
    document.Accept(m_writer);
  }

  const std::string &str() const override { return m_data.data; }

 private:
  String_stream m_data;
  Writer<String_stream> m_writer;
};

class Raw_writer : public Stream_writer<My_writer> {};

class Pretty_writer : public Stream_writer<My_pretty_writer> {};

}  // namespace

JSON_dumper::JSON_dumper(bool pprint, size_t binary_limit)
    : _binary_limit(binary_limit) {
  if (pprint)
    _writer = std::make_unique<Pretty_writer>();
  else
    _writer = std::make_unique<Raw_writer>();
}

JSON_dumper::~JSON_dumper() = default;

void JSON_dumper::start_array() {
  _deep_level++;
  _writer->start_array();
}
void JSON_dumper::end_array() {
  _deep_level--;
  _writer->end_array();
}
void JSON_dumper::start_object() {
  _deep_level++;
  _writer->start_object();
}
void JSON_dumper::end_object() {
  _deep_level--;
  _writer->end_object();
}

void JSON_dumper::append_value(const Value &value) {
  switch (value.get_type()) {
    case Undefined:
      // TODO: Decide what to do on undefineds
      break;
    case Null:
      _writer->append_null();
      break;
    case Bool:
      _writer->append_bool(value.as_bool());
      break;
    case Integer:
      _writer->append_int64(value.as_int());
      break;
    case UInteger:
      _writer->append_uint64(value.as_uint());
      break;
    case Float:
      _writer->append_float(value.as_double());
      break;
    case String:
      _writer->append_string(value.get_string());
      break;
    case Object: {
      auto object = value.as_object();
      if (object)
        object->append_json(*this);
      else
        _writer->append_null();
    } break;
    case ObjectBridge: {
      auto object = value.as_object_bridge();
      if (object)
        object->append_json(*this);
      else
        _writer->append_null();
    } break;
    case Array: {
      Value::Array_type_ref array = value.as_array();

      if (array) {
        Value::Array_type::const_iterator index, end = array->end();

        _writer->start_array();

        for (index = array->begin(); index != end; ++index)
          append_value(*index);

        _writer->end_array();
      } else {
        _writer->append_null();
      }
    } break;
    case Map: {
      Value::Map_type_ref map = value.as_map();

      if (map) {
        Value::Map_type::const_iterator index, end = map->end();

        _writer->start_object();

        for (index = map->begin(); index != end; ++index) {
          append_value(index->first, index->second);
        }

        _writer->end_object();
      } else {
        _writer->append_null();
      }
    } break;
    // case Function:
    //   value.as_function()->append_json(this);
    //   break;
    case Binary: {
      std::string encoded;
      auto raw_value = value.get_string();
      size_t max_length = raw_value.size();
      if (_binary_limit > 0) {
        // At most binary-limit + 1 bytes should be sent, when the extra byte
        // is sent, it will be an indicator for the consumer of the data that
        // a truncation happened
        max_length = std::min(raw_value.size(), _binary_limit + 1);
      }
      encode_base64(static_cast<const unsigned char *>(
                        static_cast<const void *>(raw_value.c_str())),
                    max_length, &encoded);
      _writer->append_string(encoded);
      break;
    }
  }
}

void JSON_dumper::append_value(std::string_view key, const Value &value) {
  if (value.get_type() != Value_type::Undefined) {
    _writer->append_string(key);
    append_value(value);
  }
}

void JSON_dumper::append(const Value &value) { append_value(value); }

void JSON_dumper::append(std::string_view key, const Value &value) {
  append_value(key, value);
}

void JSON_dumper::append(const Dictionary_t &value) {
  append_value(Value(value));
}

void JSON_dumper::append(std::string_view key, const Dictionary_t &value) {
  append_value(key, Value(value));
}

void JSON_dumper::append(const Array_t &value) { append_value(Value(value)); }

void JSON_dumper::append(std::string_view key, const Array_t &value) {
  append_value(key, Value(value));
}

void JSON_dumper::append_null() const { _writer->append_null(); }

void JSON_dumper::append_null(std::string_view key) const {
  _writer->append_string(key);
  _writer->append_null();
}

void JSON_dumper::append_bool(bool data) const { _writer->append_bool(data); }

void JSON_dumper::append_bool(std::string_view key, bool data) const {
  _writer->append_string(key);
  _writer->append_bool(data);
}

void JSON_dumper::append(bool data) const { append_bool(data); }

void JSON_dumper::append(std::string_view key, bool data) const {
  append_bool(key, data);
}

void JSON_dumper::append_int(int data) const { _writer->append_int(data); }

void JSON_dumper::append_int(std::string_view key, int data) const {
  _writer->append_string(key);
  _writer->append_int(data);
}

void JSON_dumper::append(int data) const { append_int(data); }

void JSON_dumper::append(std::string_view key, int data) const {
  append_int(key, data);
}

void JSON_dumper::append_uint(unsigned int data) const {
  _writer->append_uint(data);
}

void JSON_dumper::append_uint(std::string_view key, unsigned int data) const {
  _writer->append_string(key);
  _writer->append_uint(data);
}

void JSON_dumper::append(unsigned int data) const { append_uint(data); }

void JSON_dumper::append(std::string_view key, unsigned int data) const {
  append_uint(key, data);
}

void JSON_dumper::append_int64(int64_t data) const {
  _writer->append_int64(data);
}

void JSON_dumper::append_int64(std::string_view key, int64_t data) const {
  _writer->append_string(key);
  _writer->append_int64(data);
}

void JSON_dumper::append_uint64(uint64_t data) const {
  _writer->append_uint64(data);
}

void JSON_dumper::append_uint64(std::string_view key, uint64_t data) const {
  _writer->append_string(key);
  _writer->append_uint64(data);
}

void JSON_dumper::append(long int data) const {
  if constexpr (sizeof(decltype(data)) == 8) {
    append_int64(data);
  } else {
    append_int(data);
  }
}

void JSON_dumper::append(std::string_view key, long int data) const {
  if constexpr (sizeof(decltype(data)) == 8) {
    append_int64(key, data);
  } else {
    append_int(key, data);
  }
}

void JSON_dumper::append(unsigned long int data) const {
  if constexpr (sizeof(decltype(data)) == 8) {
    append_uint64(data);
  } else {
    append_uint(data);
  }
}

void JSON_dumper::append(std::string_view key, unsigned long int data) const {
  if constexpr (sizeof(decltype(data)) == 8) {
    append_uint64(key, data);
  } else {
    append_uint(key, data);
  }
}

void JSON_dumper::append(long long int data) const { append_int64(data); }

void JSON_dumper::append(std::string_view key, long long int data) const {
  append_int64(key, data);
}

void JSON_dumper::append(unsigned long long int data) const {
  append_uint64(data);
}

void JSON_dumper::append(std::string_view key,
                         unsigned long long int data) const {
  append_uint64(key, data);
}

void JSON_dumper::append_string(std::string_view data) const {
  _writer->append_string(data);
}

void JSON_dumper::append_string(std::string_view key,
                                std::string_view data) const {
  _writer->append_string(key);
  _writer->append_string(data);
}

void JSON_dumper::append(std::string_view data) const { append_string(data); }

void JSON_dumper::append(std::string_view key, std::string_view data) const {
  append_string(key, data);
}

void JSON_dumper::append_float(double data) const {
  _writer->append_float(data);
}

void JSON_dumper::append_float(std::string_view key, double data) const {
  _writer->append_string(key);
  _writer->append_float(data);
}

void JSON_dumper::append(double data) const { append_float(data); }

void JSON_dumper::append(std::string_view key, double data) const {
  append_float(key, data);
}

void JSON_dumper::append_json(const std::string &data) const {
  rapidjson::Document document;
  document.Parse(data.c_str());
  _writer->append_document(document);
}

const std::string &JSON_dumper::str() const { return _writer->str(); }

namespace json {

namespace {

auto missing_value(const char *name) {
  return std::runtime_error{str_format(
      "JSON object should contain a '%s' key with a string value", name)};
}

auto missing_uint_value(const char *name) {
  return std::runtime_error{str_format(
      "JSON object should contain a '%s' key with an unsigned integer value",
      name)};
}

}  // namespace

JSON parse(std::string_view json) {
  rapidjson::Document doc;

  doc.Parse(json.data(), json.length());

  return doc;
}

JSON parse_object_or_throw(std::string_view json) {
  auto doc = parse(json);

  if (doc.HasParseError()) {
    throw std::runtime_error{std::string("failed to parse JSON: ") +
                             rapidjson::GetParseError_En(doc.GetParseError())};
  }

  if (!doc.IsObject()) {
    throw std::runtime_error{"expected a JSON object"};
  }

  return doc;
}

std::string required(const JSON &json, const char *name, bool allow_empty) {
  const auto it = json.FindMember(name);

  if (json.MemberEnd() == it || !it->value.IsString() ||
      (!allow_empty && !it->value.GetStringLength())) {
    throw missing_value(name);
  }

  return {it->value.GetString(), it->value.GetStringLength()};
}

uint64_t required_uint(const JSON &json, const char *name) {
  const auto it = json.FindMember(name);

  if (json.MemberEnd() == it || !it->value.IsUint64()) {
    throw missing_uint_value(name);
  }

  return it->value.GetUint64();
}

std::optional<std::string> optional(const JSON &json, const char *name,
                                    bool allow_empty) {
  const auto it = json.FindMember(name);

  if (json.MemberEnd() == it) {
    return {};
  }

  if (!it->value.IsString() || (!allow_empty && !it->value.GetStringLength())) {
    throw missing_value(name);
  }

  return std::string{it->value.GetString(), it->value.GetStringLength()};
}

std::optional<uint64_t> optional_uint(const JSON &json, const char *name) {
  const auto it = json.FindMember(name);

  if (json.MemberEnd() == it) {
    return {};
  }

  if (!it->value.IsString()) {
    throw missing_uint_value(name);
  }

  return it->value.GetUint64();
}

std::string to_string(const JSON &json) {
  String_stream stream;
  My_writer<String_stream> writer{stream};
  json.Accept(writer);
  return stream.data;
}

std::string to_pretty_string(const JSON &json) {
  String_stream stream;
  My_pretty_writer<String_stream> writer{stream};
  json.Accept(writer);
  return stream.data;
}

}  // namespace json

}  // namespace shcore
