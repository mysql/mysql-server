/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef SRC_ROUTER_INCLUDE_MYSQLROUTER_WINDOWS_SERVICE_OPERATIONS_H_
#define SRC_ROUTER_INCLUDE_MYSQLROUTER_WINDOWS_SERVICE_OPERATIONS_H_

#include "mysql/harness/arg_handler.h"

enum class ServiceOperation { None, Install, InstallManual, Remove, Start };

struct ServiceConfOptions {
  std::string config_file;
  ServiceOperation operation{ServiceOperation::None};
  std::string service_name;
  std::string service_display_name;
};

/** @brief Adds a Windows service related options to the command line arguments
 * handler
 *
 * @param arg_handler arg handler object in which the function will register the
 * Windows service related parameters
 * @param conf_opts pointer to the object where the option handler will store
   the selected option parameters (service operation, name), not stored if
   nullptr passsed
 *
 */
void add_service_options(CmdArgHandler &arg_handler,
                         ServiceConfOptions *conf_opts = nullptr);

#endif  // SRC_ROUTER_INCLUDE_MYSQLROUTER_WINDOWS_SERVICE_OPERATIONS_H_
