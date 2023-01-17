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

#include "core_dumper.h"

#include <fstream>
#include <regex>
#include <string>

#include "core_finder.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/string_utils.h"    // split_string
#include "mysql/harness/utility/string.h"  // join
#include "process_launcher.h"
#include "process_manager.h"

namespace {
std::string find_executable_path(const std::string &name) {
  std::string path(getenv("PATH"));

#ifdef _WIN32
  const char path_sep = ';';
#else
  const char path_sep = ':';
#endif

  for (auto subpath : mysql_harness::split_string(path, path_sep)) {
    // the path can end with the separator so the last value can be ""
    if (!subpath.empty()) {
      auto fn = mysql_harness::Path(subpath).join(name);
      if (fn.exists()) return fn.str();
    }
  }

  return {};
}

}  // namespace

stdx::expected<std::string, std::error_code> CoreDumper::dump() {
  return dump(CoreFinder(executable_, pid_).core_name());
}

stdx::expected<std::string, std::error_code> CoreDumper::dump(
    const std::string &core_file_name) {
  if (core_file_name.empty() || !mysql_harness::Path(core_file_name).exists()) {
    return stdx::make_unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }

  return cdb(core_file_name)  // windows
      .or_else([this, core_file_name](std::error_code) {
        return lldb(core_file_name);  // linux, macosx
      })
      .or_else([this, core_file_name](std::error_code) {
        return gdb(core_file_name);  // linux, solaris
      });
}

stdx::expected<std::string, std::error_code> CoreDumper::gdb(
    const std::string &core_file_name) {
  std::string debugger_path = find_executable_path("gdb");
  if (debugger_path.empty()) {
    return stdx::make_unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }

  TempDirectory dir;
  auto cmds_filename = dir.file("cmds");
  std::ofstream(cmds_filename) << R"(thread apply all bt
quit)";

  try {
    auto &debugger_proc =
        spawner(debugger_path)
            .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
            .expected_exit_code(EXIT_SUCCESS)
            .spawn({
                executable_,           //
                "-c", core_file_name,  //
                "-x", cmds_filename,   //
                "--batch",             //
            });

    try {
      auto exit_status_res = debugger_proc.native_wait_for_exit();
      if (!(exit_status_res == ExitStatus{EXIT_SUCCESS})) {
        std::cerr << "getting core-dump failed: " << exit_status_res << "\n";
      }
    } catch (const std::exception &e) {  // timeout
      std::cerr << "getting core-dump failed: " << e.what() << "\n";
    }

    return debugger_proc.get_full_output();
  } catch (const std::exception &e) {
    std::cerr << "getting stacktrace with " << debugger_path
              << " failed: " << e.what() << "\n";
    return stdx::make_unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }
}

// for lldb (macosx, linux)
//
// bt all
stdx::expected<std::string, std::error_code> CoreDumper::lldb(
    const std::string &core_file_name) {
  std::string debugger_path = find_executable_path("lldb");
  if (debugger_path.empty()) {
    return stdx::make_unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }

  try {
    auto &debugger_proc =
        spawner(debugger_path)
            .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
            .expected_exit_code(EXIT_SUCCESS)
            .spawn({executable_,               //
                    "--core", core_file_name,  //
                    "--one-line", "bt all",    //
                    "--batch"});

    try {
      auto exit_status_res = debugger_proc.native_wait_for_exit();
      if (!(exit_status_res == ExitStatus{EXIT_SUCCESS})) {
        std::cerr << "getting core-dump failed: " << exit_status_res << "\n";
      }
    } catch (const std::exception &e) {  // timeout
      std::cerr << "getting core-dump failed: " << e.what() << "\n";
    }

    return debugger_proc.get_full_output();
  } catch (const std::exception &e) {
    std::cerr << "getting stacktrace with " << debugger_path
              << " failed: " << e.what() << "\n";
    return stdx::make_unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }
}

stdx::expected<std::string, std::error_code> CoreDumper::cdb(
    const std::string &core_file_name [[maybe_unused]]) {
#ifndef _WIN32
  return stdx::make_unexpected(
      make_error_code(std::errc::no_such_file_or_directory));
#endif

  auto env = [](const char *name) -> std::optional<std::string> {
    auto e = getenv(name);
    if (e != nullptr) return std::string(e);

    return std::nullopt;
  };

  std::string debugger_path = find_executable_path("cdb.exe");
  if (debugger_path.empty()) {
    // Check if the debugger is in:
    //
    // ENV{WindowsSdkDir}/Debuggers/{arch}/
    std::string windows_sdk_dir =
        env("WindowsSdkDir").value_or("C:/Program Files (x86)/Windows Kits/10");

    std::string arch = env("PROCESSOR_ARCHITECTURE").value_or("");

    if (!windows_sdk_dir.empty()) {
      if (arch == "AMD64") {
        const auto p = mysql_harness::Path(windows_sdk_dir)
                           .join("Debuggers")
                           .join("x64")
                           .join("cdb.exe");

        if (p.exists()) {
          debugger_path = p.str();
        }
      } else {
        std::cerr << __LINE__ << ": " << __func__ << ": unknown arch '" << arch
                  << "'\n";
      }
    }

    if (debugger_path.empty()) {
      // still not found
      return stdx::make_unexpected(
          make_error_code(std::errc::no_such_file_or_directory));
    }
  }

  std::string image_path;
  // build the image path
  try {
    const std::string cmds = mysql_harness::join(
        std::initializer_list<std::string>{
            "lmv",  // list loaded modules (verbose)
            "q",    // quit
        },
        ";");

    auto &debugger_proc =
        spawner(debugger_path)
            .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
            .expected_exit_code(EXIT_SUCCESS)
            .spawn({
                "-z", core_file_name,  //
                "-c", cmds,            //
            });

    try {
      auto exit_status_res = debugger_proc.native_wait_for_exit();
      if (!(exit_status_res == ExitStatus{EXIT_SUCCESS})) {
        std::cerr << "getting core-dump failed: " << exit_status_res << "\n";
      }
    } catch (const std::exception &e) {  // timeout
      std::cerr << "getting core-dump failed: " << e.what() << "\n";
    }

    // parse for "Image path: "
    //
    // strip the file-part from the image-path to get the image-directory

    std::regex image_path_regex(R"(Image path: (.+)\\[^\\]+)");

    std::istringstream iss(debugger_proc.get_full_output());
    std::string line;
    std::smatch image_path_match;
    while (std::getline(iss, line)) {
      if (std::regex_search(line, image_path_match, image_path_regex)) {
        image_path += ";" + std::string(image_path_match[1].first,
                                        image_path_match[1].second);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "getting stacktrace with " << debugger_path
              << " failed: " << e.what() << "\n";
  }

  image_path += ";.";  // ... and the current directory.
  std::string symbol_path = image_path;

  auto nt_symbol_path = env("_NT_SYMBOL_PATH").value_or("");
  if (!nt_symbol_path.empty()) {
    symbol_path += ";" + nt_symbol_path;
  }

  TempDirectory dir;
  auto cmds_filename = dir.file("cmds");
  std::ofstream(cmds_filename) << R"(
!sym prompts off; * disable authentication for the symbol server
.echo; .echo ## Current Exception; .echo;          !analyze -v;
.echo; .echo ## Exception context; .echo;          .ecxr;
.echo; .echo ## Local Variables per thread; .echo; !for_each_frame dv /t;
.echo; .echo ## Stacks per thread; .echo;          !uniqstack -p;
)";

  try {
    auto &debugger_proc =
        spawner(debugger_path)
            .wait_for_sync_point(ProcessManager::Spawner::SyncPoint::NONE)
            .expected_exit_code(EXIT_SUCCESS)
            .spawn({
                "-z", core_file_name,  //
                "-c",
                "$$<" + cmds_filename + ";q",  // run script from command-file
                "-i", image_path,              //
                "-y", symbol_path,             //
                "-t", "0",                     // print no errors
                "-lines",                      // source line info
            });

    try {
      auto exit_status_res = debugger_proc.native_wait_for_exit();
      if (!(exit_status_res == ExitStatus{EXIT_SUCCESS})) {
        std::cerr << "getting core-dump failed: " << exit_status_res << "\n";
      }
    } catch (const std::exception &e) {  // timeout
      std::cerr << "getting core-dump failed: " << e.what() << "\n";
    }

    std::istringstream iss(debugger_proc.get_full_output());
    std::ostringstream oss;
    std::string line;
    while (std::getline(iss, line)) {
      if (std::regex_search(line, std::regex(R"(^\*)")) ||  // comments
          std::regex_search(line, std::regex("^NatVis script unloaded from"))) {
        continue;
      }

      oss << line << "\n";
    }

    return oss.str();
  } catch (const std::exception &e) {
    std::cerr << "getting stacktrace with " << debugger_path
              << " failed: " << e.what() << "\n";
    return stdx::make_unexpected(
        make_error_code(std::errc::no_such_file_or_directory));
  }
}
