/*
  Copyright (c) 2015, 2024, Oracle and/or its affiliates.

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

/** @file
 * Module for implementing the Logger functionality.
 */
#include "mysql/harness/logging/handler.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <iostream>  // cerr
#include <sstream>
#include <string>
#include <thread>
#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

#include "mysql/harness/config_parser.h"
#include "mysql/harness/filesystem.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"
#include "mysql/harness/utility/string.h"  // string_format

using mysql_harness::Path;

using namespace std::chrono_literals;

static const char *const level_str[] = {"FATAL", "SYSTEM", "ERROR", "WARNING",
                                        "INFO",  "NOTE",   "DEBUG"};

namespace mysql_harness {

namespace logging {

////////////////////////////////////////////////////////////////
// class Handler

Handler::Handler(bool format_messages, LogLevel level,
                 LogTimestampPrecision timestamp_precision)
    : format_messages_(format_messages),
      level_(level),
      precision_(timestamp_precision) {}

// Log format is:
// <date> <time> <plugin> <level> [<thread>] <message>

std::string Handler::format(const Record &record) const {
  // Bypass formatting if disabled
  if (!format_messages_) return record.message;

  time_t cur = std::chrono::system_clock::to_time_t(record.created);
  struct tm cur_localtime;
#ifdef _WIN32
  localtime_s(&cur_localtime, &cur);
#else
  localtime_r(&cur, &cur_localtime);
#endif

  const auto nsec_part = std::chrono::duration_cast<std::chrono::nanoseconds>(
      record.created.time_since_epoch() % 1s);

  std::string dt{utility::string_format(
      "%04d-%02d-%02d %02d:%02d:%02d", cur_localtime.tm_year + 1900,
      cur_localtime.tm_mon + 1, cur_localtime.tm_mday, cur_localtime.tm_hour,
      cur_localtime.tm_min, cur_localtime.tm_sec)};
  switch (precision_) {
    case LogTimestampPrecision::kMilliSec:
      dt.append(utility::string_format(
          ".%03lld",
          static_cast<long long int>(
              std::chrono::duration_cast<std::chrono::milliseconds>(nsec_part)
                  .count())));
      break;
    case LogTimestampPrecision::kMicroSec:
      dt.append(utility::string_format(
          ".%06lld",
          static_cast<long long int>(
              std::chrono::duration_cast<std::chrono::microseconds>(nsec_part)
                  .count())));
      break;
    case LogTimestampPrecision::kNanoSec:
      dt.append(utility::string_format(
          ".%09lld",
          static_cast<long long int>(
              std::chrono::duration_cast<std::chrono::nanoseconds>(nsec_part)
                  .count())));
      break;
    case LogTimestampPrecision::kSec:
    case LogTimestampPrecision::kNotSet:
    default:
      break;
  }

  // Get the thread ID in a printable format
  std::stringstream ss;
  ss << std::hex << std::this_thread::get_id();

  // Full log message containing all elements
  std::string buffer{utility::string_format(
      "%s %s %s [%s] %s", dt.c_str(), record.domain.c_str(),
      level_str[static_cast<int>(record.level)], ss.str().c_str(),
      record.message.c_str())};

  // Note: This copies the buffer into an std::string
  return buffer;
}

void Handler::handle(const Record &record) { do_log(record); }

////////////////////////////////////////////////////////////////
// class StreamHandler

StreamHandler::StreamHandler(std::ostream &out, bool format_messages,
                             LogLevel level,
                             LogTimestampPrecision timestamp_precision)
    : Handler(format_messages, level, timestamp_precision), stream_(out) {}

void StreamHandler::do_log(const Record &record) {
  std::lock_guard<std::mutex> lock(stream_mutex_);
  stream_ << format(record) << std::endl;

  has_logged(true);
}

////////////////////////////////////////////////////////////////
// class NullHandler

NullHandler::NullHandler(bool format_messages, LogLevel level,
                         LogTimestampPrecision timestamp_precision)
    : Handler(format_messages, level, timestamp_precision) {}

void NullHandler::do_log(const Record & /*record*/) {}

////////////////////////////////////////////////////////////////
// class FileHandler

FileHandler::FileHandler(const Path &path, bool format_messages, LogLevel level,
                         LogTimestampPrecision timestamp_precision)
    : StreamHandler(fstream_, format_messages, level, timestamp_precision),
      file_path_(path) {
  // create a directory if it does not exist
  {
    std::string log_path(path.str());  // log_path = /path/to/file.log
    size_t pos;
    pos = log_path.find_last_of('/');
    if (pos != std::string::npos) log_path.erase(pos);  // log_path = /path/to

    if (log_path.empty()) {
      throw std::runtime_error("filelog sink configured but the filename '" +
                               path.str() + "' is not a valid log filename");
    }

    // mkdir if it doesn't exist
    if (mysql_harness::Path(log_path).exists() == false &&
        mkdir(log_path, kStrictDirectoryPerm) != 0) {
      auto last_error =
#ifdef _WIN32
          GetLastError()
#else
          errno
#endif
          ;
      throw std::system_error(last_error, std::system_category(),
                              "Error when creating dir '" + log_path +
                                  "': " + std::to_string(last_error));
    }
  }

  reopen();  // not opened yet so it's just for open in this context
}

void FileHandler::reopen(const std::string dst) {  // namespace logging
  std::exception_ptr eptr = nullptr;

  // here we need to lock the mutex that's used while logging
  // to prevent other threads from trying to log to invalid stream
  std::lock_guard<std::mutex> lock(stream_mutex_);

  // if was open before, close first
  if (fstream_.is_open()) {
    fstream_.close();

    // With closed stream we may rename file on any platform, windows included
    if (!dst.empty()) {
      if (rename(file_path_.str().c_str(), dst.c_str())) {
        auto last_error =
#ifdef _WIN32
            GetLastError()
#else
            errno
#endif
            ;
        if (last_error != 0) {
          // Exceptions cannot be thrown directly here, but stashed until after
          // reopening the logfile again. Otherwise all logging ends up on
          // console due to closed logfile.
          mysql_harness::Path dstpath(dst);
          if (dstpath.exists()) {
            eptr = make_exception_ptr(std::system_error(
                last_error, std::system_category(),
                "File exists. Cannot rename to " + dstpath.str()));
          } else {
            if (last_error != ENOENT) {
              eptr = make_exception_ptr(
                  std::system_error(last_error, std::system_category(),
                                    "Cannot rename file in directory " +
                                        dstpath.dirname().str()));
            }
          }
        }
      }
    }

    fstream_.clear();
  }

#ifdef _WIN32
  const bool created = !file_path_.exists();
#endif
  fstream_.open(file_path_.str(), std::ofstream::app);
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

    if (file_path_.exists()) {
      throw std::system_error(
          last_error, std::system_category(),
          "File exists, but cannot open for writing " + file_path_.str());
    } else {
      throw std::system_error(
          last_error, std::system_category(),
          "Cannot create file in directory " + file_path_.dirname().str());
    }
  }

#ifdef _WIN32
  // If we are running in the context of the Windows service and we created the
  // file it will be unaccessible by anyone else which makes it kind of
  // pointless as this is a log file. Let's make sure this file is readable if
  // we just created it.
  if (created) {
    make_file_readable_for_everyone(file_path_.str());
  }
#endif

  // After reopening the logfile, is it safe to throw earlier exceptions
  if (eptr) {
    std::rethrow_exception(eptr);
  }
}  // namespace logging

void FileHandler::do_log(const Record &record) {
  std::lock_guard<std::mutex> lock(stream_mutex_);
  stream_ << format(record) << std::endl;
  // something is wrong with the logging file, let's at least log it on the
  // std error as a fallback
  if (stream_.fail()) {
    std::cerr << format(record) << std::endl;
  } else {
    has_logged(true);
  }
}

FileHandler::~FileHandler() = default;

}  // namespace logging

}  // namespace mysql_harness
