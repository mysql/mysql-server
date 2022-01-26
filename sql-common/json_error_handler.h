#ifndef JSON_ERROR_HANDLER_INCLUDED
#define JSON_ERROR_HANDLER_INCLUDED

/* Copyright (c) 2022, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <cstdlib>
#include <functional>

using JsonParseErrorHandler =
    std::function<void(const char *parse_err, size_t err_offset)>;
using JsonDocumentDepthHandler = std::function<void()>;

#ifdef MYSQL_SERVER

class JsonParseDefaultErrorHandler {
 public:
  JsonParseDefaultErrorHandler(const char *func_name, int arg_idx)
      : m_func_name(func_name), m_arg_idx(arg_idx) {}

  void operator()(const char *parse_err, size_t err_offset) const;

 private:
  const char *m_func_name;
  const int m_arg_idx;
};

void JsonDocumentDefaultDepthHandler();

#endif  // MYSQL_SERVER
#endif  // JSON_ERROR_HANDLER_INCLUDED
