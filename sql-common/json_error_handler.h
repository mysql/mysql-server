#ifndef JSON_ERROR_HANDLER_INCLUDED
#define JSON_ERROR_HANDLER_INCLUDED

/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <cstdlib>
#include <functional>

class THD;
struct CHARSET_INFO;
struct MYSQL_TIME_STATUS;

using JsonParseErrorHandler =
    std::function<void(const char *parse_err, size_t err_offset)>;
using JsonErrorHandler = std::function<void()>;
using JsonCoercionHandler =
    std::function<void(const char *target_type, int error_code)>;
using JsonCoercionDeprecatedHandler =
    std::function<void(MYSQL_TIME_STATUS &status)>;
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

/**
  Error handler to be used when parsing JSON schemas and validating JSON
  objects using a JSON schema.
*/
class JsonSchemaErrorHandler {
 public:
  virtual ~JsonSchemaErrorHandler() = default;
  /// Called when an invalid JSON value is encountered.
  virtual void InvalidJsonText(size_t arg_no, const char *wrong_string,
                               size_t offset) const = 0;
  /// Called if the provided JSON is not a JSON object.
  virtual void InvalidJsonType() const = 0;
  /// Called if a std exception is thrown
  virtual void HandleStdExceptions() const = 0;
  /// Called if a schema reference is encountered in the JSON document as
  /// MySQL does not support such constructs.
  virtual void NotSupported() const = 0;
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

/**
  The default error handler to be used when parsing JSON schemas and
  validating JSON objects using a JSON schema inside the MySQL server.
*/
class JsonSchemaDefaultErrorHandler final : public JsonSchemaErrorHandler {
 public:
  explicit JsonSchemaDefaultErrorHandler(const char *function_name)
      : m_calling_function_name(function_name) {}
  void InvalidJsonText(size_t arg_no, const char *wrong_string,
                       size_t offset) const override;
  void InvalidJsonType() const override;
  void HandleStdExceptions() const override;
  void NotSupported() const override;

 private:
  /// Used for error reporting and holds the name of the function.
  const char *m_calling_function_name;
};

/// Callback function called when a coercion error occurs. It reports the
/// error as ERROR
class JsonCoercionErrorHandler {
 public:
  explicit JsonCoercionErrorHandler(const char *msgnam) : m_msgnam(msgnam) {}
  void operator()(const char *target_type, int error_code) const;

 private:
  /// The name of the field/expression being coerced to be used
  /// in error message if conversion failed.
  const char *m_msgnam;
};

/// Callback function called when a coercion error occurs. It reports the
/// error as WARNING
class JsonCoercionWarnHandler {
 public:
  explicit JsonCoercionWarnHandler(const char *msgnam) : m_msgnam(msgnam) {}
  void operator()(const char *target_type, int error_code) const;

 private:
  /// The name of the field/expression being coerced to be used
  /// in error message if conversion failed.
  const char *m_msgnam;
};

/// Callback function that checks if MYSQL_TIME_STATUS contains a deprecation
/// warning. If it does, it issues the warning and resets the status indication.
class JsonCoercionDeprecatedDefaultHandler {
 public:
  void operator()(MYSQL_TIME_STATUS &status) const;
};

#endif  // MYSQL_SERVER
#endif  // JSON_ERROR_HANDLER_INCLUDED
