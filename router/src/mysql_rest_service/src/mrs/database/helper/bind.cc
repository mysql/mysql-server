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

#include "mrs/database/helper/bind.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>

#include "helper/json/rapid_json_iterator.h"
#include "helper/json/text_to.h"
#include "helper/json/to_sqlstring.h"
#include "helper/mysql_numeric_value.h"
#include "mrs/http/error.h"

namespace mrs {
namespace database {

using DataTypeInText = ::helper::DataTypeInText;

void MysqlBind::fill_null_as_inout(DataType data_type) {
  auto bind = allocate_bind_buffer(data_type);
  lengths_.emplace_back(new unsigned long);
  bind->length = lengths_.back().get();
  *bind->length = 0;

  nulls_.emplace_back(new bool);
  bind->is_null = nulls_.back().get();
  *bind->is_null = true;
}

void MysqlBind::fill_mysql_bind_for_out(DataType data_type) {
  auto bind = allocate_bind_buffer(data_type);
  auto length = new unsigned long;
  lengths_.emplace_back(length);
  bind->length = length;
  *bind->length = 0;
}

void MysqlBind::fill_mysql_bind_inout_vector(const rapidjson::Value &value) {
  if (!value.IsArray())
    throw http::Error(HttpStatusCode::BadRequest,
                      "Expecting json-array for vector parameter");
  auto array = value.GetArray();
  for (const auto &element : helper::json::array_iterator(array)) {
    if (!element.IsNumber())
      throw http::Error(HttpStatusCode::BadRequest,
                        "Expecting that all elements of json-array are numbers "
                        "for vector parameter");
  }

  auto bind = allocate_bind_buffer(DataType::VECTOR);
  auto float_buffer = reinterpret_cast<float *>(bind->buffer);
  auto float_number_of_elements = bind->buffer_length / sizeof(float);

  if (array.Size() > float_number_of_elements) {
    using namespace std::string_literals;
    throw http::Error(
        HttpStatusCode::BadRequest,
        "Too many elements for vector parameter, internal buffer allows for "s +
            std::to_string(float_number_of_elements) + " elements");
  }

  for (const auto &element : helper::json::array_iterator(array)) {
    *(float_buffer++) = element.GetFloat();
  }

  bind->length =
      lengths_.emplace_back(new unsigned long(array.Size() * sizeof(float)))
          .get();
}

void MysqlBind::fill_mysql_bind_inout_vector(const std::string &value) {
  auto json = helper::json::text_to_document(value);

  if (json.HasParseError()) {
    throw http::Error(HttpStatusCode::BadRequest,
                      "Invalid json-value used for vector parameter");
  }

  fill_mysql_bind_inout_vector(json);
}

void MysqlBind::fill_mysql_bind_impl(const std::string &value,
                                     DataType data_type) {
  if (data_type == DataType::BOOLEAN) {
    using namespace std::string_literals;
    bool value_bool = false;
    static const std::string k_false{"\0", 1};
    static const std::string k_true{"\1", 1};

    auto value_type = ::helper::get_type_inside_text(value);
    switch (value_type) {
      case DataTypeInText::kDataInteger:
        value_bool = std::atoi(value.c_str()) > 0;
        break;
      case DataTypeInText::kDataString: {
        if (value == "true") {
          value_bool = 1;
        } else if (value == "false") {
          value_bool = 0;
        } else
          throw http::Error(HttpStatusCode::BadRequest,
                            "Not allowed value:"s + value +
                                ", for one of boolean parameters");

      } break;

      default:
        throw http::Error(
            HttpStatusCode::BadRequest,
            "Not allowed value:"s + value + ", for one of parameters");
    }
    allocate_for_blob(value_bool ? k_true : k_false);
    return;
  }

  // Transfer other types as strings.
  allocate_for_string(value);
}

enum_field_types MysqlBind::to_mysql_type(DataType pdt) {
  switch (pdt) {
    case DataType::UNKNOWN: {
      assert(false &&
             "This should not happen, DB object should disable "
             "fields/parameters that are unknown.");
      throw std::runtime_error("Unsupported MySQL type.");
    }
    case DataType::BINARY:
      return MYSQL_TYPE_BLOB;
    case DataType::GEOMETRY:
      return MYSQL_TYPE_GEOMETRY;
    case DataType::JSON:
      [[fallthrough]];
    case DataType::STRING:
      return MYSQL_TYPE_STRING;
    case DataType::INTEGER:
      return MYSQL_TYPE_LONGLONG;
    case DataType::DOUBLE:
      return MYSQL_TYPE_DOUBLE;
    case DataType::BOOLEAN:
      return MYSQL_TYPE_TINY_BLOB;
    case DataType::VECTOR:
      return MYSQL_TYPE_VECTOR;

    default:
      return MYSQL_TYPE_NULL;
  }
}

bool MysqlBind::is_null(const std::string &) { return false; }

bool MysqlBind::is_null(const rapidjson::Value &value) {
  return value.IsNull();
}

const std::string &MysqlBind::to_string(const std::string &value) {
  return value;
}

std::string MysqlBind::to_string(const rapidjson::Value &value) {
  std::stringstream stream;
  ::helper::json::to_stream(stream, value, "1", "0");
  return stream.str();
}

MYSQL_BIND *MysqlBind::allocate_for_blob(const std::string &value) {
  auto result = allocate_for(value);
  result->buffer_type = MYSQL_TYPE_BLOB;
  return result;
}
MYSQL_BIND *MysqlBind::allocate_for_string(const std::string &value) {
  return allocate_for(value);
}

MYSQL_BIND *MysqlBind::allocate_for(const std::string &value) {
  auto bind = allocate_bind_buffer(DataType::STRING);
  if (value.length() + 1 > bind->buffer_length) {
    using namespace std::string_literals;
    throw http::Error(
        HttpStatusCode::BadRequest,
        "'in-out' parameter is too long, the internal buffer is "s +
            std::to_string(bind->buffer_length) + " bytes long.");
  }
  memcpy(reinterpret_cast<char *>(bind->buffer), value.c_str(), value.length());
  auto length = new unsigned long;
  lengths_.emplace_back(length);
  bind->length = length;
  *bind->length = value.length();

  return bind;
}

static std::unique_ptr<char[]> allocate_buffer(MYSQL_BIND *bind, size_t size) {
  std::unique_ptr<char[]> result;
  result.reset(new char[bind->buffer_length = size]);
  return result;
}

MYSQL_BIND *MysqlBind::allocate_bind_buffer(DataType data_type) {
  auto bind = &parameters.emplace_back();

  // Please note that the var-string maximums size is 2^16-1 characters.
  // We are not allowing LARGETEXT, here. It would be too much for the
  // network transfer.
  const size_t k_var_string_maximum_size = 0xFFFF;  // 2^16-1
  const size_t k_tiny_blob_maximum_size = 255;
  const size_t k_vector_maximum_size = 16383 * sizeof(float);
  const size_t k_blob_maximum_size = 0xFFFF;  // 2^16-1

  std::unique_ptr<char[]> &buffer = buffers_.emplace_back();
  bind->buffer_type = to_mysql_type(data_type);

  switch (bind->buffer_type) {
    case MYSQL_TYPE_STRING:
      buffer = allocate_buffer(bind, k_var_string_maximum_size);
      break;

    case MYSQL_TYPE_LONGLONG:
      buffer = allocate_buffer(bind, sizeof(uint64_t));
      break;

    case MYSQL_TYPE_DOUBLE:
      buffer = allocate_buffer(bind, sizeof(double));
      break;

    case MYSQL_TYPE_TINY_BLOB:
      buffer = allocate_buffer(bind, k_tiny_blob_maximum_size);
      break;

    case MYSQL_TYPE_LONG:
      buffer = allocate_buffer(bind, sizeof(uint32_t));
      break;

    case MYSQL_TYPE_TIMESTAMP:
      buffer = allocate_buffer(bind, k_var_string_maximum_size);
      break;

    case MYSQL_TYPE_VECTOR:
      buffer = allocate_buffer(bind, k_vector_maximum_size);
      // Server desn't accept vector type as parameter.
      bind->buffer_type = MYSQL_TYPE_VAR_STRING;
      break;

    case MYSQL_TYPE_GEOMETRY:
      // Geometry is a BLOB type under the hood, BLOB requirement is 64k
      buffer = allocate_buffer(bind, k_blob_maximum_size);
      bind->buffer_type = MYSQL_TYPE_BLOB;
      break;

    case MYSQL_TYPE_BLOB:
      buffer = allocate_buffer(bind, k_blob_maximum_size);
      break;

    default:
      fprintf(stderr, "bind->buffer_type:%i\n", (int)bind->buffer_type);
      assert(nullptr && "should not happen");
  }

  bind->buffer = buffer.get();

  return bind;
}

}  // namespace database
}  // namespace mrs
