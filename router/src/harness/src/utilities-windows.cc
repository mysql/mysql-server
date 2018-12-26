/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "utilities.h"

#include <Shlwapi.h>
#include <Windows.h>
#include <Winsock2.h>

#include <regex>
#include <string>

namespace mysql_harness {

namespace utility {

bool matches_glob(const std::string &word, const std::string &pattern) {
  return PathMatchSpec(word.c_str(), pattern.c_str());
}

void sleep_seconds(unsigned int seconds) { Sleep(1000 * seconds); }

std::string get_message_error(int errcode) {
  if (errcode == SOCKET_ERROR || errcode == 0) {
    errcode = WSAGetLastError();
  }

  LPTSTR lpMsgBuf;

  if (0 != FormatMessage(
               FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS,
               NULL, errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
               (LPTSTR)&lpMsgBuf, 0, NULL)) {
    std::string msgerr = "SystemError: ";
    msgerr += lpMsgBuf;
    LocalFree(lpMsgBuf);
    return msgerr;
  } else {
    return "SystemError: " + std::to_string(errcode);
  }
}

bool regex_pattern_matches(const std::string &s, const std::string &pattern) {
  std::regex regex(pattern, std::regex::extended);
  return std::regex_match(s, regex);
}

}  // namespace utility

}  // namespace mysql_harness
