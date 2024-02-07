// Copyright (c) 2000, 2024, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

/// @file safe_kill_win.cc
///
/// Utility program used to signal a safe_process it's time to shutdown
///
/// Usage:
///   safe_kill <pid>

#include <signal.h>
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, const char **argv) {
  // Ignore any signals
  signal(SIGINT, SIG_IGN);
  signal(SIGBREAK, SIG_IGN);
  signal(SIGTERM, SIG_IGN);

  if (argc != 3) {
    std::fprintf(stderr, "safe_kill <pid>\n");
    std::exit(2);
  }

  const DWORD pid = std::atoi(argv[1]);

  if (std::strcmp(argv[2], "mysqltest") == 0) {
    char event_name[64];
    std::sprintf(event_name, "mysqltest[%lu]stacktrace", pid);

    HANDLE stacktrace_request_event =
        OpenEvent(EVENT_MODIFY_STATE, FALSE, event_name);

    if (stacktrace_request_event == nullptr) {
      // Failed to open timeout event
      HANDLE mysqltest_process =
          OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pid);

      // Check if the process is alive.
      if (mysqltest_process)
        std::fprintf(
            stderr, "Failed to open stacktrace_request_event %s, error = %lu\n",
            event_name, GetLastError());
    } else {
      if (SetEvent(stacktrace_request_event) == 0) {
        // Failed to set timeout event
        std::fprintf(stderr, "Failed to set event %s.\n", event_name);
        std::exit(3);
      }

      // A small delay of 4 seconds after setting timeout_event to allow the
      // stack printing to be completed before setting the shutdown_event.
      Sleep(4000);
      CloseHandle(stacktrace_request_event);
    }
  }

  char safe_process_name[32] = {0};
  std::sprintf(safe_process_name, "safe_process[%lu]", pid);

  int retry_open_event = 2;

  // Open the event to signal
  HANDLE shutdown_event;
  while ((shutdown_event = OpenEvent(EVENT_MODIFY_STATE, FALSE,
                                     safe_process_name)) == nullptr) {
    // Check if the process is alive, otherwise there is really
    // no sense to retry the open of the event.
    HANDLE process =
        OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, pid);

    if (!process) {
      // Already died
      std::exit(1);
    }

    DWORD exit_code;
    if (!GetExitCodeProcess(process, &exit_code)) {
      std::fprintf(stderr, "GetExitCodeProcess failed, pid= %lu, err= %lu\n",
                   pid, GetLastError());
      std::exit(1);
    }

    if (exit_code != STILL_ACTIVE) {
      // Already died
      CloseHandle(process);
      std::exit(2);
    }

    CloseHandle(process);

    if (retry_open_event--)
      Sleep(100);
    else {
      std::fprintf(stderr, "Failed to open shutdown_event '%s', error: %lu\n",
                   safe_process_name, GetLastError());
      std::exit(3);
    }
  }

  if (SetEvent(shutdown_event) == 0) {
    std::fprintf(stderr, "Failed to signal shutdown_event '%s', error: %lu\n",
                 safe_process_name, GetLastError());
    CloseHandle(shutdown_event);
    std::exit(4);
  }

  CloseHandle(shutdown_event);
  std::exit(0);
}
