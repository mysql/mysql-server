/*
 * Copyright (c) 2019, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_GET_DETAILED_VALIDATION_ERROR_H_
#define PLUGIN_X_SRC_GET_DETAILED_VALIDATION_ERROR_H_

#include <string>

#include "plugin/x/src/query_string_builder.h"
#include "plugin/x/src/sql_data_result.h"
#include "plugin/x/src/xpl_error.h"

namespace xpl {

inline ngs::Error_code get_detailed_validation_error(
    iface::Sql_session &data_context) {
  Sql_data_result sql_result(&data_context);
  sql_result.query(
      "GET DIAGNOSTICS CONDITION 1 @$internal_validation_error_message = "
      "MESSAGE_TEXT;");
  sql_result.query("SELECT @$internal_validation_error_message");

  std::string error_text;
  sql_result.get(&error_text);

  return ngs::Error(ER_X_DOCUMENT_DOESNT_MATCH_EXPECTED_SCHEMA,
                    "Document is not valid according to the schema assigned to "
                    "collection. %s",
                    error_text.c_str());
}

}  // namespace xpl

#endif  // PLUGIN_X_SRC_GET_DETAILED_VALIDATION_ERROR_H_
