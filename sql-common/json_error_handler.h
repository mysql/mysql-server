#ifndef JSON_ERROR_HANDLER_INCLUDED
#define JSON_ERROR_HANDLER_INCLUDED

/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

class THD;

using JsonParseErrorHandler =
    std::function<void(const char *parse_err, size_t err_offset)>;
using JsonErrorHandler = std::function<void()>;

/**
  Error handler for the functions that serialize a JSON value in the JSON binary
  storage format. The member functions are called when an error occurs, and they
  should report the error the way the caller has specified. When called from the
  server, my_error() should be called to signal the error. The subclass
  JsonSerializationDefaultErrorHandler, which calls my_error(), should be used
  when called from server code.
*/
class JsonSerializationErrorHandler {
 public:
  virtual ~JsonSerializationErrorHandler() = default;

  /// Called when a JSON object contains a member with a name that is longer
  /// than supported by the JSON binary format.
  virtual void KeyTooBig() const = 0;

  /// Called when a JSON document is too big to be stored in the JSON binary
  /// format.
  virtual void ValueTooBig() const = 0;

  /// Called when a JSON document has more nesting levels than supported.
  virtual void TooDeep() const = 0;

  /// Called when an invalid JSON value is encountered.
  virtual void InvalidJson() const = 0;

  /// Called when an internal error occurs.
  virtual void InternalError(const char *message) const = 0;

  /// Check if the stack is about to be exhausted, and report the error.
  /// @return true if the stack is about to be exhausted, false otherwise.
  virtual bool CheckStack() const = 0;
};

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

void JsonDepthErrorHandler();

/**
  Error handler to be used when serializing JSON binary values in server code.
  Uses my_error(), so it cannot be used in code outside of the server.
*/
class JsonSerializationDefaultErrorHandler final
    : public JsonSerializationErrorHandler {
 public:
  explicit JsonSerializationDefaultErrorHandler(const THD *thd) : m_thd(thd) {}
  void KeyTooBig() const override;
  void ValueTooBig() const override;
  void TooDeep() const override;
  void InvalidJson() const override;
  void InternalError(const char *message) const override;
  bool CheckStack() const override;

 private:
  const THD *m_thd;
};

#endif  // MYSQL_SERVER
#endif  // JSON_ERROR_HANDLER_INCLUDED
