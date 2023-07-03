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

#ifndef LOG_H_
#define LOG_H_

#include <stdio.h>
#include <iostream>
#include <sstream>

struct log_client_type {
  typedef enum {
    LOG_CLIENT_DBG,
    LOG_CLIENT_INFO,
    LOG_CLIENT_WARNING,
    LOG_CLIENT_ERROR
  } log_type;
};

enum log_client_level {
  LOG_CLIENT_LEVEL_NONE = 1,
  LOG_CLIENT_LEVEL_ERROR,
  LOG_CLIENT_LEVEL_ERROR_WARNING,
  LOG_CLIENT_LEVEL_ERROR_WARNING_INFO,
  LOG_CLIENT_LEVEL_ALL
};

class Logger_client {
 public:
  template <log_client_type::log_type type>
  void log(std::string msg);
  void set_log_level(log_client_level level);
  void write(std::string data);
  void log_client_plugin_data_exchange(const unsigned char *buffer,
                                       unsigned int length);

 private:
  log_client_level m_log_level{LOG_CLIENT_LEVEL_NONE};
};

template <log_client_type::log_type type>
void Logger_client::log(std::string msg) {
  std::stringstream log_stream;
  switch (type) {
    case log_client_type::LOG_CLIENT_DBG:
      if (LOG_CLIENT_LEVEL_ALL > m_log_level) {
        return;
      }
      log_stream << "[DBG] ";
      break;
    case log_client_type::LOG_CLIENT_INFO:
      if (LOG_CLIENT_LEVEL_ERROR_WARNING_INFO > m_log_level) {
        return;
      }
      log_stream << "[Note] ";
      break;
    case log_client_type::LOG_CLIENT_WARNING:
      if (LOG_CLIENT_LEVEL_ERROR_WARNING > m_log_level) {
        return;
      }
      log_stream << "[Warning] ";
      break;
    case log_client_type::LOG_CLIENT_ERROR:
      if (LOG_CLIENT_LEVEL_NONE >= m_log_level) {
        return;
      }
      log_stream << "[Error] ";
      break;
  };

  /*
    We can write debug messages also in error log file if logging level is set
    to debug. For MySQL client this will come from environment variable
  */
  log_stream << ": " << msg;
  write(log_stream.str());
}

extern Logger_client *g_logger_client;

#define log_client_dbg g_logger_client->log<log_client_type::LOG_CLIENT_DBG>
#define log_client_info g_logger_client->log<log_client_type::LOG_CLIENT_INFO>
#define log_client_warning g_logger->log<log_client_type::LOG_CLIENT_WARNING>
#define log_client_error g_logger_client->log<log_client_type::LOG_CLIENT_ERROR>

#endif
