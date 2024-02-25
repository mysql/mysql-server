/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include "mysqlrouter/windows/service_operations.h"
#include "mysqlrouter/windows/router_service_names.h"

#ifdef _WIN32

void add_service_options(CmdArgHandler &arg_handler,
                         ServiceConfOptions &conf_opts) {
  using OptionNames = CmdOption::OptionNames;

  arg_handler.add_option(
      OptionNames({"--install-service"}),
      "Install " + std::string(kServiceProductName) +
          " as a Windows service which starts "
          "automatically at system boot (<SERVICE_NAME>='" +
          std::string(kDefaultServiceName) + "' if not provided)",
      CmdOptionValueReq::optional, "SERVICE_NAME",
      [&](const std::string &name) {
        conf_opts.operation = ServiceOperation::Install;
        conf_opts.service_name = name.empty() ? kDefaultServiceName : name;
        conf_opts.service_display_name =
            name.empty() ? kDefaultServiceDisplayName : name;
      });

  arg_handler.add_option(
      OptionNames({"--install-service-manual"}),
      "Install " + std::string(kServiceProductName) +
          " as a Windows service which needs to be started manually "
          "(<SERVICE_NAME>='" +
          std::string(kDefaultServiceName) + "' if not provided)",
      CmdOptionValueReq::optional, "SERVICE_NAME",
      [&](const std::string &name) {
        conf_opts.operation = ServiceOperation::InstallManual;
        conf_opts.service_name = name.empty() ? kDefaultServiceName : name;
        conf_opts.service_display_name =
            name.empty() ? kDefaultServiceDisplayName : name;
      });

  arg_handler.add_option(
      OptionNames({"--remove-service"}),
      "Remove " + std::string(kServiceProductName) +
          " from a Windows services (<SERVICE_NAME>='" +
          std::string(kDefaultServiceName) + "' if not provided)",
      CmdOptionValueReq::optional, "SERVICE_NAME",
      [&](const std::string &name) {
        conf_opts.operation = ServiceOperation::Remove;
        conf_opts.service_name = name.empty() ? kDefaultServiceName : name;
        conf_opts.service_display_name =
            name.empty() ? kDefaultServiceDisplayName : name;
      });

  arg_handler.add_option(
      OptionNames({"--service"}),
      "Used internally by the service manager when " +
          std::string(kServiceProductName) +
          " is started as a Windows service. Fails when called from the "
          "command line. (<SERVICE_NAME>='" +
          std::string(kDefaultServiceName) + "' if not provided)",
      CmdOptionValueReq::optional, "SERVICE_NAME",
      [&](const std::string &name) {
        conf_opts.operation = ServiceOperation::Start;
        conf_opts.service_name = name.empty() ? kDefaultServiceName : name;
        conf_opts.service_display_name =
            name.empty() ? kDefaultServiceDisplayName : name;
      });
}
#endif
