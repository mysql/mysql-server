/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_SRC_HELPER_GET_SYSTEM_VARIABLE_H_
#define PLUGIN_X_SRC_HELPER_GET_SYSTEM_VARIABLE_H_

#include <string>

#include "plugin/x/src/interface/sql_session.h"
#include "plugin/x/src/ngs/error_code.h"
#include "plugin/x/src/sql_data_result.h"
#include "plugin/x/src/xpl_log.h"

namespace xpl {

template <typename T>
void get_system_variable(iface::Sql_session *da, const std::string &variable,
                         T *value) {
  Sql_data_result result(da);
  try {
    result.query(("SELECT @@" + variable).c_str());
    if (result.size() != 1) {
      log_error(ER_XPLUGIN_FAILED_TO_GET_SYS_VAR, variable.c_str());
      *value = T();
      return;
    }
    result.get(value);
  } catch (const ngs::Error_code &) {
    log_error(ER_XPLUGIN_FAILED_TO_GET_SYS_VAR, variable.c_str());
    *value = T();
  }
}

template <typename T>
T get_system_variable(iface::Sql_session *da, const std::string &variable) {
  T value = T();
  get_system_variable(da, variable, &value);
  return value;
}

}  // namespace xpl

#endif  // PLUGIN_X_SRC_HELPER_GET_SYSTEM_VARIABLE_H_
