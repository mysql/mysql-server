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

#ifndef ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_TEXT_TO_H_
#define ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_TEXT_TO_H_

#include <type_traits>

#include <my_rapidjson_size_t.h>
#include <rapidjson/document.h>
#include <rapidjson/memorystream.h>
#include <rapidjson/reader.h>

namespace helper {
namespace json {

/**
 * Convert text representation of json object/value to type defined by
 * 'Handler'.
 *
 * The 'Handler' must be a child class of `rapidjson::BaseReaderHandler`. The
 * child class must contain type/alias definition of returned result, under
 * "Result" alias and `get_result` method. For example:
 *
 *     class HandlerSomeExample
 *          : rapidjson::BaseReaderHandler<rapidjson::UTF8<>,HandlerSomeExample>
 * { public: using Result = std::string;
 *
 *         ...
 *      };
 */
template <typename Handler, typename Container>
bool text_to(Handler *handler, const Container &c) {
  static_assert(
      std::is_same<typename Container::value_type, char>::value ||
      std::is_same<typename Container::value_type, unsigned char>::value);
  rapidjson::MemoryStream memory_stream{
      reinterpret_cast<const char *>(&*c.begin()), c.size()};
  rapidjson::Reader read;

  return !read.Parse<Handler::k_parse_flags>(memory_stream, *handler).IsError();
}

template <typename Container>
bool text_to(rapidjson::Document *doc, const Container &c) {
  static_assert(
      std::is_same<typename Container::value_type, char>::value ||
      std::is_same<typename Container::value_type, unsigned char>::value);
  if (c.size() == 0) {
    return false;
  }
  doc->Parse(reinterpret_cast<const char *>(&*c.begin()), c.size());
  return !doc->HasParseError();
}

inline bool text_to(rapidjson::Document *doc, const std::string &str) {
  doc->Parse(str.c_str());
  return !doc->HasParseError();
}

inline bool text_to(rapidjson::Document *doc, const char *str) {
  doc->Parse(str);
  return !doc->HasParseError();
}

inline bool text_to(rapidjson::Document::Object *obj, const std::string &str) {
  rapidjson::Document doc;

  if (!text_to(&doc, str)) return false;
  if (!doc.IsObject()) return false;

  *obj = doc.GetObject();

  return true;
}

inline bool text_to(rapidjson::Value *val, const std::string &str) {
  rapidjson::Document doc;

  if (!text_to(&doc, str)) return false;
  if (!doc.IsObject()) return false;

  *val = doc.GetObject();

  return true;
}

/**
 * Convert text representation of json object/value to type defined by
 * 'Handler'.
 *
 * The 'Handler' must be a child class of `rapidjson::BaseReaderHandler`. The
 * child class must contain type/alias definition of returned result, under
 * "Result" alias and `get_result` method. For example:
 *
 *     class HandlerSomeExample
 *          : rapidjson::BaseReaderHandler<rapidjson::UTF8<>,HandlerSomeExample>
 * { public: using Result = std::string;
 *
 *         ...
 *      };
 */
template <typename Handler, typename Container>
typename Handler::Result text_to_handler(const Container &c) {
  Handler handler;

  text_to<Handler, Container>(&handler, c);

  return handler.get_result();
}

inline rapidjson::Document text_to_document(const std::string &str) {
  rapidjson::Document result;
  text_to(&result, str);
  return result;
}

}  // namespace json
}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_HELPER_JSON_TEXT_TO_H_
