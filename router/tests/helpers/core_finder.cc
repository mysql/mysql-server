/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
#include "core_finder.h"

#include <fstream>
#include <sstream>
#include <string>

#if defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

/*
 * Find a core-file after a program crashed.
 *
 * Windows
 * -------
 *
 * Looks for '{executable}.{pid}.dmp'
 *
 * MacOS
 * -----
 *
 * Looks for '/cores/core.{pid}'
 *
 * That directory may not be writable by normal users and core-dumps discarded.
 *
 * Linux
 * -----
 *
 * checks /proc/sys/kernel/core_pattern and /proc/sys/kernel/core_uses_pid.
 *
 * if core_pattern is "core" or from "apport" it expects "core"
 * in the workdir of the executable. If core_uses_pid is 1,
 * it expects "core.{pid}".
 *
 * FreeBSD
 * -------
 *
 * Looks for "core.{pid}" in the current directory.
 *
 * Solaris
 * -------
 *
 * Looks for "core" in the current directory.
 *
 * Possible Extensions
 * -------------------
 *
 * On Linux coredumps may be handled by systemd-coredump.
 *
 * curedumpctl may be used to get a stacktrace.
 *
 *   https://www.freedesktop.org/software/systemd/man/coredumpctl.html
 *
 * $ coredumpctl debug ${PID} --debug-arguments="-batch -ex ..."
 *
 * On FreeBSD and MacOS, cores are placed in the location specified by:
 *
 * $ sysctl kern.corefile
 *
 * MacOS
 * : /cores/core.%P
 *
 * FreeBSD
 * : %P.core
 *
 * On Solaris, coreadm may be queried for the core-file-pattern:
 *
 *     coreadm {pid}
 *
 * Limitations
 * -----------
 *
 * On MacOS cores are only generated if the executable as the entitlement to
 * dump cores:
 *
 *     com.apple.security.get-task-allow bool true
 *
 * which needs to be part of the signature of the executable:
 *
 *     codesign -s - -f --entitlements core-dump-entitlements.plist
 *     {executable}
 */

#ifdef __linux__
namespace {
// check if the core-files use a pid.
stdx::expected<bool, std::error_code> core_uses_pid() {
  std::ifstream ifs("/proc/sys/kernel/core_uses_pid");

  if (!ifs.good()) {
    return stdx::make_unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }

  std::string core_uses_pid;
  std::getline(ifs, core_uses_pid);

  // not supported yet.
  if (core_uses_pid == "0") {
    return false;
  } else if (core_uses_pid == "1") {
    return true;
  } else {
    return stdx::make_unexpected(make_error_code(std::errc::invalid_argument));
  }
}
}  // namespace
#endif

std::string CoreFinder::core_name() const {
#ifdef _WIN32
  // {executable}.{pid}.dmp
  std::string module_name;
  module_name.resize(MAX_PATH);

  _splitpath_s(executable_.c_str(),                     //
               NULL, 0,                                 // drive
               NULL, 0,                                 // dir
               module_name.data(), module_name.size(),  // name
               nullptr, 0                               // ext
  );

  size_t nul_pos = module_name.find('\0');
  if (nul_pos != std::string::npos) {
    module_name.resize(nul_pos);  // truncate to nul-char
  }

  return module_name + "." + std::to_string(pid_) + ".dmp";
#elif defined(__linux__)
  std::string core_file_name("core");
  // see "man core" on Linux
  //
  // /proc/sys/kernel/core_pattern (default: core)
  // /proc/sys/kernel/core_uses_pid

  {
    mysql_harness::Path core_pattern_path("/proc/sys/kernel/core_pattern");
    if (core_pattern_path.exists()) {
      std::ifstream ifs(core_pattern_path.str());

      std::string core_pattern;
      std::getline(ifs, core_pattern);

      auto starts_with = [](std::string_view a, std::string_view needle) {
        return a.substr(0, needle.size()) == needle;
      };

      if (core_pattern == "core" ||
          starts_with(core_pattern, "|/usr/share/apport/apport")) {
        // if core-pattern is "core", core-uses-pid may append a PID to the
        // filename.
        //
        // apport also writes a core file with .{PID} [if core-uses-pid]
      } else if (core_pattern == "") {
        core_file_name.clear();  // empty is ok ... if it gets an PID appended.
      } else {
        // location of core-file is unknown.
        return {};
      }
    }
  }

  auto core_uses_pid_res = core_uses_pid();
  if (!core_uses_pid_res) return {};

  if (*core_uses_pid_res) {
    core_file_name += '.';
    core_file_name += std::to_string(pid_);
  }

  return core_file_name;
#elif defined(__APPLE__) || defined(__FreeBSD__)
  std::string core_pattern;
  core_pattern.resize(256);
  size_t core_pattern_size = core_pattern.size();
  if (0 != sysctlbyname("kern.corefile", core_pattern.data(),
                        &core_pattern_size, nullptr, 0)) {
    return {};
  }
  core_pattern.resize(core_pattern_size);

  // handle the most common forms.
  if (core_pattern == "/cores/core.%P") {  // MacOS default
    return "/cores/core." + std::to_string(pid_);
  } else if (core_pattern == "core.%P") {
    return "core." + std::to_string(pid_);
  } else if (core_pattern == "%P.core") {  // FreeBSD default
    return std::to_string(pid_) + ".core";
  }
  // fallthrough
#endif

  return "core";
}
