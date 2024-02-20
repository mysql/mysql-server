/* Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

#include "sql-common/json_error_handler.h"

#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld_error.h"
#include "sql/check_stack.h"
#include "sql/derror.h"
#include "sql/sql_class.h"
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/sql_exception_handler.h"

void JsonParseDefaultErrorHandler::operator()(const char *parse_err,
                                              size_t err_offset) const {
  my_error(ER_INVALID_JSON_TEXT_IN_PARAM, MYF(0), m_arg_idx + 1, m_func_name,
           parse_err, err_offset, "");
}

void JsonDepthErrorHandler() { my_error(ER_JSON_DOCUMENT_TOO_DEEP, MYF(0)); }

void JsonSerializationDefaultErrorHandler::KeyTooBig() const {
  my_error(ER_JSON_KEY_TOO_BIG, MYF(0));
}

void JsonSerializationDefaultErrorHandler::ValueTooBig() const {
  my_error(ER_JSON_VALUE_TOO_BIG, MYF(0));
}

void JsonSerializationDefaultErrorHandler::TooDeep() const {
  JsonDepthErrorHandler();
}

void JsonSerializationDefaultErrorHandler::InvalidJson() const {
  my_error(ER_INVALID_JSON_BINARY_DATA, MYF(0));
}

void JsonSerializationDefaultErrorHandler::InternalError(
    const char *message) const {
  my_error(ER_INTERNAL_ERROR, MYF(0), message);
}

bool JsonSerializationDefaultErrorHandler::CheckStack() const {
  return check_stack_overrun(m_thd, STACK_MIN_SIZE, nullptr);
}

void JsonCoercionErrorHandler::operator()(const char *target_type,
                                          int error_code) const {
  my_error(error_code, MYF(0), target_type, "", m_msgnam,
           current_thd->get_stmt_da()->current_row_for_condition());
}

void JsonCoercionWarnHandler::operator()(const char *target_type,
                                         int error_code) const {
  /*
   One argument is no longer used (the empty string), but kept to avoid
   changing error message format.
  */
  push_warning_printf(current_thd, Sql_condition::SL_WARNING, error_code,
                      ER_THD_NONCONST(current_thd, error_code), target_type, "",
                      m_msgnam,
                      current_thd->get_stmt_da()->current_row_for_condition());
}

void JsonCoercionDeprecatedDefaultHandler::operator()(
    MYSQL_TIME_STATUS &status) const {
  check_deprecated_datetime_format(current_thd, &my_charset_utf8mb4_bin,
                                   status);
}
void JsonSchemaDefaultErrorHandler::InvalidJsonText(size_t arg_no,
                                                    const char *wrong_string,
                                                    size_t offset) const {
  my_error(ER_INVALID_JSON_TEXT_IN_PARAM, MYF(0), arg_no,
           m_calling_function_name, wrong_string, offset, "");
}

void JsonSchemaDefaultErrorHandler::NotSupported() const {
  my_error(ER_NOT_SUPPORTED_YET, MYF(0), "references in JSON Schema");
}

void JsonSchemaDefaultErrorHandler::HandleStdExceptions() const {
  handle_std_exception(m_calling_function_name);
}

void JsonSchemaDefaultErrorHandler::InvalidJsonType() const {
  my_error(ER_INVALID_JSON_TYPE, MYF(0), 1, m_calling_function_name, "object");
}