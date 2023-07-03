/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "log_client.h"

void Logger_client::set_log_level(log_client_level level) {
  m_log_level = level;
}

void Logger_client::write(std::string data) {
  std::cerr << data << "\n";
  std::cerr.flush();
}

void Logger_client::log_client_plugin_data_exchange(const unsigned char *buffer,
                                                    unsigned int length) {
  if (m_log_level != LOG_CLIENT_LEVEL_ALL) {
    return;
  }
  std::stringstream logstream;
  char *ascii_string{nullptr};
  if (buffer && (length > 0)) {
    ascii_string = new char[(length + 1) * 2]{'\0'};
  } else {
    return;
  }
  for (unsigned int i = 0; i < length; i++) {
    sprintf(ascii_string + (2 * i), "%02X", *(buffer + i));
  }
  logstream << "Kerberos client plug-in data exchange: " << ascii_string;
  log_client_dbg(logstream.str().c_str());
  delete[] ascii_string;
  ascii_string = nullptr;
}
