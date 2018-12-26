/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/json_utils.h"

#include <cctype>

#include "my_rapidjson_size_t.h"  // IWYU pragma: keep

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include "include/my_dbug.h"
#include "plugin/x/src/xpl_error.h"
#include "plugin/x/src/xpl_regex.h"

namespace xpl {

#if 0
bool validate_json_string(const char *s, size_t length) {
  const char *c = s;
  const char *end = s + length;

  if (*c == '"') {  // string literal
    ++c;
    while (c < end) {
      if (iscntrl(*c) || *c == 0) {
        return false;
      }
      if (*c == '\\') {
        if (c >= end - 1) return false;
        /* The allowed escape codes:
         \"
         \\
         \/
         \b
         \f
         \n
         \r
         \t
         \u four-hex-digits
         */
        ++c;
        switch (*c) {
          case '"':
          case '\\':
          case '/':
          case 'b':
          case 'f':
          case 'n':
          case 'r':
          case 't':
            ++c;
            break;
          case 'u':
            if (c < end - 5 && isxdigit(c[1]) && isxdigit(c[2]) &&
                isxdigit(c[3]) && isxdigit(c[4]))
              c += 5;
            else
              return false;
            break;
          default:
            return false;
        }
      } else if (*c == '"') {
        break;
      } else {
        ++c;
      }
    }
    if (*c != '"') return false;
    ++c;
  } else {
    return false;
  }
  return true;
}

bool validate_json_string(const std::string &s) {
  return validate_json_string(s.data(), s.length());
}

ngs::Error_code validate_json_document_path(const std::string &s) {
  if (s.empty()) return ngs::Error(ER_X_BAD_DOC_PATH, "Empty document path");

  return ngs::Error_code();
}
#endif

std::string quote_json(const std::string &s) {
  std::string out;
  size_t i, end = s.length();

  out.reserve(s.length() * 2 + 1);

  out.push_back('"');

  for (i = 0; i < end; ++i) {
    switch (s[i]) {
      case '"':
        out.append("\\\"");
        break;

      case '\\':
        out.append("\\\\");
        break;

      case '/':
        out.append("\\/");
        break;

      case '\b':
        out.append("\\b");
        break;

      case '\f':
        out.append("\\f");
        break;

      case '\n':
        out.append("\\n");
        break;

      case '\r':
        out.append("\\r");
        break;

      case '\t':
        out.append("\\t");
        break;

      default:
        out.push_back(s[i]);
        break;
    }
  }
  out.push_back('"');
  return out;
}

std::string quote_json_if_needed(const std::string &s) {
  size_t i, end = s.length();

  if (isalpha(s[0]) || s[0] == '_') {
    for (i = 1; i < end && (isdigit(s[i]) || isalpha(s[i]) || s[i] == '_');
         i++) {
    }
    if (i == end) return s;
  }
  return quote_json(s);
}

namespace {
class Json_string_handler
    : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>,
                                          Json_string_handler> {
 public:
  bool Key(const char *str, rapidjson::SizeType /*length*/, bool /*copy*/) {
    return !(m_level == 1 && std::strcmp("_id", str) == 0);
  }
  bool StartObject() {
    ++m_level;
    return true;
  }
  bool EndObject(rapidjson::SizeType) {
    --m_level;
    return true;
  }

 private:
  uint32_t m_level{0};
};

}  // namespace

bool is_id_in_json(const std::string &s) {
  Json_string_handler handler;
  rapidjson::Reader reader;
  rapidjson::StringStream ss(s.c_str());
  return reader.Parse(ss, handler).IsError();
}

}  // namespace xpl
