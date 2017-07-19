/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef X_TESTS_DRIVER_JSON_TO_ANY_HANDLER_H_
#define X_TESTS_DRIVER_JSON_TO_ANY_HANDLER_H_

#include <cstdint>
#include <stack>

#include "my_rapidjson_size_t.h"  // IWYU pragma: keep
#include "mysqlxclient/xmessage.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"


class Json_to_any_handler : public rapidjson::BaseReaderHandler<
                                   rapidjson::UTF8<>, Json_to_any_handler> {
 public:
  using Any    = ::Mysqlx::Datatypes::Any;
  using Scalar = ::Mysqlx::Datatypes::Scalar;

 public:
  explicit Json_to_any_handler(Any *any) { m_stack.push(any); }

  bool Key(const char *str, rapidjson::SizeType length, bool copy);
  bool Null();
  bool Bool(bool b);
  bool Int(int i);
  bool Uint(unsigned u);
  bool Int64(int64_t i);
  bool Uint64(uint64_t u);
  bool Double(double d, bool = false);
  bool String(const char *str, rapidjson::SizeType length, bool);
  bool StartObject();
  bool EndObject(rapidjson::SizeType member_count);
  bool StartArray();
  bool EndArray(rapidjson::SizeType element_count);

 private:
  Scalar *get_scalar(Scalar::Type scalar_type);
  std::stack<Any *> m_stack;
};

#endif  // X_TESTS_DRIVER_JSON_TO_ANY_HANDLER_H_
