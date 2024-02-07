/*
   Copyright (c) 2010, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifdef _WIN32

#include "EventLogHandler.hpp"

#include <time.h>

#include "message.h"

EventLogHandler::EventLogHandler(const char *source_name)
    : LogHandler(),
      m_source_name(source_name),
      m_event_source(nullptr),
      m_level(Logger::LL_ERROR) {}

EventLogHandler::~EventLogHandler() { close(); }

static bool check_message_resource(void) {
  // Only do check once per binary
  static bool check_message_resource_done = false;
  if (check_message_resource_done) return true;
  check_message_resource_done = true;

  // Each program that want to log to Windows event log need to
  // have a message resource compiled in. Check that it's there
  // by resolving the message from current module(.exe)
  char *message_text;
  if (FormatMessage(
          FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE |
              FORMAT_MESSAGE_IGNORE_INSERTS,
          nullptr, MSG_EVENTLOG, 0, (LPTSTR)&message_text, 0, nullptr) != 0) {
    LocalFree(message_text);
    return true;
  }

  // Could not get message from own module, extract error
  // message from system and print it to help debugging
  DWORD last_err = GetLastError();
  fprintf(stderr,
          "This program does not seem to have the message resource "
          "required for logging to Windows event log, error: %lu ",
          last_err);
  if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_ALLOCATE_BUFFER |
                        FORMAT_MESSAGE_IGNORE_INSERTS,
                    nullptr, last_err, 0, (LPSTR)&message_text, 0, nullptr)) {
    fprintf(stderr, "message: '%s'\n", message_text);
    LocalFree(message_text);
  } else {
    fprintf(stderr, "message: <unknown>\n");
  }
  fflush(stderr);

  // The program have not been properly compiled, crash in debug mode
  assert(false);
  return false;
}

static bool setup_eventlogging(const char *source_name) {
  // Check that this binary have message resource compiled in
  if (!check_message_resource()) return false;

  char sub_key[MAX_PATH];
  BaseString::snprintf(
      sub_key, sizeof(sub_key),
      "SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\%s",
      source_name);

  // Create the event source registry key
  HKEY key_handle;
  LONG error = RegCreateKey(HKEY_LOCAL_MACHINE, sub_key, &key_handle);
  if (error != ERROR_SUCCESS) {
    // Could neither create or open key
    if (error == ERROR_ACCESS_DENIED) {
      fprintf(stderr,
              "WARNING: Could not create or access the registry key needed for "
              "the application\n"
              "to log to the Windows EventLog. Run the application with "
              "sufficient\n"
              "privileges once to create the key, or add the key manually, or "
              "turn off\n"
              "logging for that application. [HKLM] key '%s', error: %ld\n",
              sub_key, error);
    } else {
      fprintf(stderr,
              "WARNING: Could neither create or open key '%s', error: %ld\n",
              sub_key, error);
    }

    return false;
  }

  /* Get path of current module and use it as message resource  */
  char module_path[MAX_PATH];
  DWORD len = GetModuleFileName(nullptr, module_path, sizeof(module_path));
  if (len == 0 || len == sizeof(module_path)) {
    fprintf(stderr,
            "Could not extract path of module, module_len: %lu, error: %lu\n",
            len, GetLastError());
    RegCloseKey(key_handle);
    return false;
  }

  (void)RegSetValueEx(key_handle, "EventMessageFile", 0, REG_EXPAND_SZ,
                      (PBYTE)module_path, len + 1);

  /* Register supported event types */
  DWORD event_types =
      (EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE);
  (void)RegSetValueEx(key_handle, "TypesSupported", 0, REG_DWORD,
                      (PBYTE)&event_types, sizeof(event_types));

  RegCloseKey(key_handle);
  return true;
}

bool EventLogHandler::open() {
  if (!setup_eventlogging(m_source_name)) {
    fprintf(stderr, "Failed to setup event logging\n");
    return false;
  }

  m_event_source = RegisterEventSource(nullptr, m_source_name);
  if (!m_event_source) {
    fprintf(stderr, "Failed to register event source, error: %lu\n",
            GetLastError());
    return false;
  }
  return true;
}

bool EventLogHandler::close() {
  if (!is_open()) return true;

  (void)DeregisterEventSource(m_event_source);

  return true;
}

bool EventLogHandler::is_open() { return (m_event_source != nullptr); }

void EventLogHandler::writeHeader(const char *, Logger::LoggerLevel level,
                                  time_t) {
  m_level = level;
}

static bool write_event_log(HANDLE eventlog_handle, Logger::LoggerLevel level,
                            const char *msg) {
  WORD type;
  switch (level) {
    case Logger::LL_ON:
    case Logger::LL_DEBUG:
    case Logger::LL_INFO:
      type = EVENTLOG_INFORMATION_TYPE;
      break;

    case Logger::LL_WARNING:
      type = EVENTLOG_WARNING_TYPE;
      break;

    case Logger::LL_ERROR:
    case Logger::LL_ALERT:
    case Logger::LL_CRITICAL:
    case Logger::LL_ALL:
      type = EVENTLOG_ERROR_TYPE;
      break;
    default:
      return false;
  }

  if (!ReportEvent(eventlog_handle, type, 0, MSG_EVENTLOG, nullptr, 1, 0, &msg,
                   nullptr)) {
    return false;
  }

  return true;
}

void EventLogHandler::writeMessage(const char *msg) {
  if (!is_open()) return;

  if (!write_event_log(m_event_source, m_level, msg)) {
    fprintf(stderr, "Failed to report event to event log, error: %lu\n",
            GetLastError());
  }
}

void EventLogHandler::writeFooter() {}

bool EventLogHandler::setParam(const BaseString &, const BaseString &) {
  return false;
}

int EventLogHandler::printf(Logger::LoggerLevel level, const char *source_name,
                            const char *msg, ...) {
  if (setup_eventlogging(source_name)) {
    // Failed to setup event logging
    return -3;
  }

  char buf[MAX_LOG_MESSAGE_SIZE];
  va_list ap;
  va_start(ap, msg);
  int ret = vsnprintf_s(buf, sizeof(buf), _TRUNCATE, msg, ap);
  va_end(ap);

  HANDLE eventlog_handle = RegisterEventSource(nullptr, source_name);
  if (!eventlog_handle) {
    // Failed to open event log
    return -2;
  }

  if (!write_event_log(eventlog_handle, level, buf)) {
    // Failed to log, return error
    (void)DeregisterEventSource(eventlog_handle);
    return -1;
  }

  (void)DeregisterEventSource(eventlog_handle);

  // Ok, return length of the logged message
  return ret;
}

#endif
