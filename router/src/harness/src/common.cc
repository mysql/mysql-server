/*
Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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

#include "my_thread.h"

#ifndef _WIN32
#include <pthread.h>
#include <sys/stat.h>
#endif

namespace mysql_harness {

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
