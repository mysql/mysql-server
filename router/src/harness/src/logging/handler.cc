/*
  Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

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

/** @file
 * Module for implementing the Logger functionality.
 */

#include "mysql/harness/logging/handler.h"
#include "mysql/harness/logging/logging.h"

#include "common.h"
#include "mysql/harness/config_parser.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/plugin.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

using mysql_harness::Path;

#if defined(_MSC_VER) && defined(logger_EXPORTS)
/* We are building this library */
#define LOGGER_API __declspec(dllexport)
#else
#define LOGGER_API
#endif

using std::ofstream;
using std::ostringstream;

static const char *const level_str[] = {"FATAL", "ERROR", "WARNING", "INFO",
                                        "DEBUG"};

namespace mysql_harness {

namespace logging {

////////////////////////////////////////////////////////////////
// class Handler

Handler::Handler(bool format_messages, LogLevel level)
    : format_messages_(format_messages), level_(level) {}

// Log format is:
// <date> <time> <plugin> <level> [<thread>] <message>

std::string Handler::format(const Record &record) const {
  // Bypass formatting if disabled
  if (!format_messages_) return record.message;

  // Format the time (19 characters)
  char time_buf[20];
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S",
           localtime(&record.created));

  // Get the thread ID in a printable format
  std::stringstream ss;
  ss << std::hex << std::this_thread::get_id();

  // We ignore the return value from snprintf, which means that the
  // output is truncated if the total length exceeds the buffer size.
  char buffer[512];
  snprintf(buffer, sizeof(buffer), "%-19s %s %s [%s] %s", time_buf,
           record.domain.c_str(), level_str[static_cast<int>(record.level)],
           ss.str().c_str(), record.message.c_str());

  // Note: This copies the buffer into an std::string
  return buffer;
}

void Handler::handle(const Record &record) { do_log(record); }

// satisfy ODR
constexpr const char *StreamHandler::kDefaultName;

////////////////////////////////////////////////////////////////
// class StreamHandler

StreamHandler::StreamHandler(std::ostream &out, bool format_messages,
                             LogLevel level)
    : Handler(format_messages, level), stream_(out) {}

void StreamHandler::do_log(const Record &record) {
  std::lock_guard<std::mutex> lock(stream_mutex_);
  stream_ << format(record) << std::endl;
}

////////////////////////////////////////////////////////////////
// class FileHandler

FileHandler::FileHandler(const Path &path, bool format_messages, LogLevel level)
    : StreamHandler(fstream_, format_messages, level),
      fstream_(path.str(), ofstream::app) {
  if (fstream_.fail()) {
    // get the last-error early as with VS2015 it has been seen
    // that something in std::system_error() called SetLastError(0)
    auto last_error =
#ifdef _WIN32
        GetLastError()
#else
        errno
#endif
        ;

    if (path.exists()) {
      throw std::system_error(
          last_error, std::system_category(),
          "File exists, but cannot open for writing " + path.str());
    } else {
      throw std::system_error(
          last_error, std::system_category(),
          "Cannot create file in directory " + path.dirname().str());
    }
  }
}

// satisfy ODR
constexpr const char *FileHandler::kDefaultName;

FileHandler::~FileHandler() {}

}  // namespace logging

}  // namespace mysql_harness
