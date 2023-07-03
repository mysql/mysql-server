/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#include "mysql/harness/logging/eventlog_plugin.h"

#include "eventlog_rc/message.h"
#include "mysql/harness/logging/logger_plugin.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/logging/supported_logger_options.h"
#include "mysql/harness/plugin.h"

#include <Windows.h>
#include <cstdarg>

using mysql_harness::logging::LogLevel;
using mysql_harness::logging::LogTimestampPrecision;

constexpr const char *kRegistryPrefix =
    "SYSTEM\\CurrentControlSet\\services\\eventlog\\Application\\";

static WORD logger_to_eventlog_severity(LogLevel level) {
  switch (level) {
    case LogLevel::kFatal:
    case LogLevel::kError:
      return EVENTLOG_ERROR_TYPE;
    case LogLevel::kWarning:
      return EVENTLOG_WARNING_TYPE;
    default:
      assert(level == LogLevel::kSystem || level == LogLevel::kInfo ||
             level == LogLevel::kNote || level == LogLevel::kDebug);
      // there are no DEBUG, NOTE, or SYSTEM counterparts in the eventlog so we
      // go with Information for DEBUG, NOTE and SYSTEM too!
      return EVENTLOG_INFORMATION_TYPE;
  }
}

/**
 * Create a key in the Windows registry.
 *
 * We'll setup a "MySQL Router" key in the EventLog branch (RegCreateKey),
 * set our executable name (GetModuleFileName) as file-name
 * ("EventMessageFile"), then set the message types we expect to
 * be logging ("TypesSupported").
 * If the key does not exist, sufficient privileges will be required
 * to create and configure it.  If the key does exist, opening it
 * should be unprivileged; modifying will fail on insufficient
 * privileges, but that is non-fatal.
 *
 */

static void create_eventlog_registry_entry(
    const std::string &event_source_name) {
  HKEY hRegKey = NULL;
  TCHAR szPath[MAX_PATH];
  DWORD dwTypes;

  const std::string registryKey =
      std::string(kRegistryPrefix) + event_source_name;

  // Opens the event source registry key; creates it first if required.
  auto dwError =
      RegCreateKey(HKEY_LOCAL_MACHINE, registryKey.c_str(), &hRegKey);

  if (dwError != ERROR_SUCCESS) {
    if (dwError == ERROR_ACCESS_DENIED) {
      throw std::runtime_error(
          "Could not create or access the registry key needed "
          "for the " +
          event_source_name +
          " application\n"
          "to log to the Windows EventLog. Run the application "
          "with sufficient\n"
          "privileges once to create the key, add the key "
          "manually, or turn off\n"
          "event logging for that application.");
    }

    throw std::runtime_error(
        "Could not create the registry key needed "
        "for the " +
        event_source_name +
        " application\n"
        "Error: " +
        std::to_string(dwError));
  }

  // make sure to close the registry key no matter what error we enconter from
  // here
  std::shared_ptr<void> exit_guard(nullptr,
                                   [&](void *) { RegCloseKey(hRegKey); });

  /* Name of the PE module that contains the message resource */
  GetModuleFileName(nullptr, szPath, MAX_PATH);
  /* Register EventMessageFile (DLL/exec containing event identifiers) */
  dwError = RegSetValueEx(hRegKey, "EventMessageFile", 0, REG_EXPAND_SZ,
                          (PBYTE)szPath, (DWORD)(strlen(szPath) + 1));
  if ((dwError != ERROR_SUCCESS) && (dwError != ERROR_ACCESS_DENIED)) {
    throw std::runtime_error(
        "Could not create or access the registry key needed "
        "for the MySQL Router application\n"
        "Error: " +
        std::to_string(dwError));
  }

  /* Register supported event types */
  dwTypes =
      (EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE);

  dwError = RegSetValueEx(hRegKey, "TypesSupported", 0, REG_DWORD,
                          (LPBYTE)&dwTypes, sizeof dwTypes);

  if ((dwError != ERROR_SUCCESS) && (dwError != ERROR_ACCESS_DENIED)) {
    throw std::runtime_error(
        "Could not create or access the registry key needed "
        "for the MySQL Router application\n"
        "Error: " +
        std::to_string(dwError));
  }
}

EventlogHandler::EventlogHandler(
    bool format_messages, mysql_harness::logging::LogLevel level,
    bool create_registry_entries /*= true*/,
    const std::string event_source_name /*= kDefaultEventSourceName*/)
    : mysql_harness::logging::Handler(format_messages, level,
                                      LogTimestampPrecision::kSec),
      event_source_name_{event_source_name} {
  if (create_registry_entries)
    create_eventlog_registry_entry(event_source_name_);

  event_src_ = RegisterEventSourceA(NULL, event_source_name_.c_str());
  if (!event_src_) {
    throw std::runtime_error("Cannot create event log source, error: " +
                             std::to_string(GetLastError()));
  }
}

EventlogHandler::~EventlogHandler() { DeregisterEventSource(event_src_); }

void EventlogHandler::do_log(
    const mysql_harness::logging::Record &record) noexcept {
  LPCSTR strings[] = {event_source_name_.c_str(), record.domain.c_str(),
                      record.message.c_str()};

  const auto severity = logger_to_eventlog_severity(record.level);

  ReportEvent(event_src_, severity, /*category*/ 0,
              /*eventid*/ MSG_EVENTLOG, NULL, 3, 0, strings, NULL);
}

extern "C" {
mysql_harness::Plugin harness_plugin_eventlog = {
    mysql_harness::PLUGIN_ABI_VERSION,
    mysql_harness::ARCHITECTURE_DESCRIPTOR,
    "Logging using eventlog",
    VERSION_NUMBER(0, 0, 1),
    0,
    nullptr,  // Requires
    0,
    nullptr,  // Conflicts
    nullptr,  // init
    nullptr,  // deinit,
    nullptr,  // start,
    nullptr,  // stop
    false,    // declares_readiness
    logger_sink_supported_options.size(),
    logger_sink_supported_options.data(),
};
}
