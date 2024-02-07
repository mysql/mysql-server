/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include <mysql/components/services/mysql_signal_handler.h>
#include <cassert>
#ifndef _WIN32
#include <signal.h>  // SIGSEGV, siginfo_t
#include <unistd.h>
#else
#include <Windows.h>  // GetStdHandle, STD_ERROR_HANDLE
#include <fileapi.h>  // WriteFile, SetFilePointer
struct siginfo_t;
#endif
#include <cstring>
#include <string>
#include <vector>
#include "mysql/components/component_implementation.h"

REQUIRES_SERVICE_PLACEHOLDER(my_signal_handler);

BEGIN_COMPONENT_REQUIRES(test_mysql_signal_handler)
REQUIRES_SERVICE(my_signal_handler), END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_PROVIDES(test_mysql_signal_handler)
END_COMPONENT_PROVIDES();

#ifdef _WIN32
size_t safe_write_stderr(const char *buf, size_t count) {
  DWORD bytes_written;
  SetFilePointer(GetStdHandle(STD_ERROR_HANDLE), 0, nullptr, FILE_END);
  WriteFile(GetStdHandle(STD_ERROR_HANDLE), buf, (DWORD)count, &bytes_written,
            nullptr);
  return bytes_written;
}
auto SIGSEGV = 11;
#else
size_t safe_write_stderr(const char *buf, size_t count) {
  return static_cast<size_t>(write(STDERR_FILENO, buf, count));
}
#endif

static auto test_fatal_signal_callback(int signum,
                                       siginfo_t *info [[maybe_unused]],
                                       void *ucontext [[maybe_unused]])
    -> void {
  assert(signum == SIGSEGV);
  if (signum != SIGSEGV) return;
  auto message = "Signal from the test_mysql_signal_handler component.\n";
  safe_write_stderr(message, strlen(message));
}

static mysql_service_status_t init() {
  return SERVICE_PLACEHOLDER(my_signal_handler)
      ->add(SIGSEGV, test_fatal_signal_callback);
}

static mysql_service_status_t deinit() {
  return SERVICE_PLACEHOLDER(my_signal_handler)
      ->remove(SIGSEGV, test_fatal_signal_callback);
}

BEGIN_COMPONENT_METADATA(test_mysql_signal_handler)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

DECLARE_COMPONENT(test_mysql_signal_handler, "mysql:test_mysql_signal_handler")
init, deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_mysql_signal_handler)
    END_DECLARE_LIBRARY_COMPONENTS
