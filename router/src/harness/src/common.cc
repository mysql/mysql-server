/*
Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.

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

#include <cassert>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include "common.h"
#include "harness_assert.h"

#ifdef _WIN32
#include <aclapi.h>
#include <windows.h>
#else
#include <pthread.h>
#include <sys/stat.h>
#endif

namespace mysql_harness {

std::string get_strerror(int err) {
  char msg[256];
  std::string result;

#if !defined(_GNU_SOURCE) &&                                     \
    ((defined(_POSIX_C_SOURCE) && _POSIX_C_SOURCE >= 200112L) || \
     (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 600))
  // glibc's POSIX version
  int ret = strerror_r(err, msg, sizeof(msg));
  if (ret) {
    return "errno= " + std::to_string(err) +
           " (strerror_r failed: " + std::to_string(ret) + ")";
  } else {
    result = std::string(msg);
  }
#elif defined(_WIN32)
  int ret = strerror_s(msg, sizeof(msg), err);
  if (ret) {
    return "errno= " + std::to_string(err) +
           " (strerror_s failed: " + std::to_string(ret) + ")";
  } else {
    result = std::string(msg);
  }
#elif defined(__GLIBC__) && defined(_GNU_SOURCE)
  // glibc's POSIX version, GNU version
  char *ret = strerror_r(err, msg, sizeof(msg));
  result = std::string(ret);
#else
  // POSIX version
  int ret = strerror_r(err, msg, sizeof(msg));
  if (ret) {
    return "errno= " + std::to_string(err) +
           " (strerror_r failed: " + std::to_string(ret) + ")";
  } else {
    result = std::string(msg);
  }
#endif

  return result;
}

#ifdef _WIN32
const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push, 8)
typedef struct tagTHREADNAME_INFO {
  DWORD dwType;
  LPCSTR szName;
  DWORD dwThreadID;
  DWORD dwFlags;
} THREADNAME_INFO;
#pragma pack(pop)
#endif

void rename_thread(const char thread_name[16]) {
// linux
#ifdef __linux__
  assert(strnlen(thread_name, 16) < 16);  // max allowed len for thread_name
  pthread_setname_np(pthread_self(), thread_name);

// windows
#elif defined(_WIN32)
#ifdef _DEBUG
  // In Win32 API there is no API for setting thread name, but according to
  // Microsoft documentation, there is a "secret handshake" between debuggee
  // & debugger using the special values used here.
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = thread_name;
  info.dwThreadID = GetCurrentThreadId();
  info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable : 6320 6322)
  __try {
    RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR),
                   (ULONG_PTR *)&info);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
#pragma warning(pop)
#endif  // #ifdef _DEBUG

// other
#else
  // TODO: on BSD/OSX, this should build but does not:
  // pthread_setname_np(thread_name);
  (void)thread_name;

#endif
}

static inline const std::string &truncate_string_backend(
    const std::string &input, std::string &output, size_t max_len) {
  // to keep code simple, we don't support unlikely use cases
  harness_assert(
      max_len >=
      6);  // 3 (to fit the first 3 chars) + 3 (to fit "..."), allowing:
           // "foo..."
           // ^--- arbitrarily-reasonable number, could be even 0 if we wanted

  // no truncation needed, so just return the original
  if (input.size() <= max_len) return input;

  // we truncate and overwrite last three characters with "..."
  // ("foobarbaz" becomes "foobar...")
  output.assign(input, 0, max_len);
  output[max_len - 3] = '.';
  output[max_len - 2] = '.';
  output[max_len - 1] = '.';
  return output;
}

const std::string &truncate_string(const std::string &input,
                                   size_t max_len /*= 80*/) {
  thread_local std::string output;
  return truncate_string_backend(input, output, max_len);
}

std::string truncate_string_r(const std::string &input,
                              size_t max_len /*= 80*/) {
  std::string output;
  return truncate_string_backend(input, output, max_len);
}

}  // namespace mysql_harness
