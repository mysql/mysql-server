/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef _CORE_DUMPER_H_
#define _CORE_DUMPER_H_

#include "process_launcher.h"
#include "process_manager.h"

class CoreDumper : public ProcessManager {
 public:
  using pid_type = mysql_harness::ProcessLauncher::process_id_type;

  CoreDumper(std::string executable, pid_type pid)
      : executable_{std::move(executable)}, pid_{pid} {}

  stdx::expected<std::string, std::error_code> dump();

  stdx::expected<std::string, std::error_code> dump(
      const std::string &core_file_name);

 private:
  stdx::expected<std::string, std::error_code> gdb(
      const std::string &core_file_name);

  stdx::expected<std::string, std::error_code> lldb(
      const std::string &core_file_name);

  stdx::expected<std::string, std::error_code> cdb(
      const std::string &core_file_name);

  std::string executable_;
  pid_type pid_;
};

#endif
