/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef LOG_CLIENT_H_
#define LOG_CLIENT_H_

#include <stdio.h>

#include <sstream>

namespace auth_ldap_sasl_client {

/**
 LDAP plugin log levels type
*/
enum ldap_log_level {
  LDAP_LOG_LEVEL_NONE = 1,
  LDAP_LOG_LEVEL_ERROR,
  LDAP_LOG_LEVEL_ERROR_WARNING,
  LDAP_LOG_LEVEL_ERROR_WARNING_INFO,
  LDAP_LOG_LEVEL_ALL
};

/**
 Log writer class.
*/
class Ldap_log_writer_error {
 public:
  /**
   Constructor.
  */
  Ldap_log_writer_error();
  /**
   Destructor.
  */
  ~Ldap_log_writer_error();
  /**
   Writes the data to the log.

   @param data [in] the data
  */
  void write(const std::string &data);
};

/**
 Class representing logger for LDAP plugins. Singleton.
*/
class Ldap_logger {
  /** type of message to be logged */
  using Message = std::initializer_list<const char *>;

 public:
  /**
   Creates the logger object.

   @param log_level [in] log level
  */
  static void create_logger(ldap_log_level log_level = LDAP_LOG_LEVEL_NONE);
  /**
   Destroys the logger object.
  */
  static void destroy_logger();
  /**
   Log a debug message.

   @param msg [in] the message
  */
  static void log_dbg_msg(Message msg);
  /**
   Log an info message.

   @param msg [in] the message
  */
  static void log_info_msg(Message msg);
  /**
   Log a warning message.

   @param msg [in] the message
  */
  static void log_warning_msg(Message msg);
  /**
   Log an error message.

   @param msg [in] the message
  */
  static void log_error_msg(Message msg);

 private:
  /**
   Private constructor to assure singleton pattern.

   @param level [in] log level
  */
  Ldap_logger(ldap_log_level level);
  /**
   Destructor.
  */
  ~Ldap_logger();

  /** Log writer */
  Ldap_log_writer_error *m_log_writer;
  /** Log level */
  ldap_log_level m_log_level;
  /** Pointer to the only log object */
  static Ldap_logger *m_logger;

  /**
   Compose the log message and write it.

   @tparam level log level
   @tparam prefix log level name

   @param msg [in] message to be logged
  */
  template <ldap_log_level level, const char *prefix>
  void log(Message msg) {
    std::stringstream log_stream;
    if (level > m_log_level || !m_log_writer) return;
    log_stream << prefix << " : ";
    for (auto &msg_element : msg) {
      if (msg_element) log_stream << msg_element;
    }
    m_logger->m_log_writer->write(log_stream.str());
  }
};

}  // namespace auth_ldap_sasl_client
/**
  \defgroup Ldap_logger_wrappers Shortcut wrappers to the logger functions
  @{
*/
#define log_dbg(...) Ldap_logger::log_dbg_msg({__VA_ARGS__})
#define log_info(...) Ldap_logger::log_info_msg({__VA_ARGS__})
#define log_warning(...) Ldap_logger::log_warning_msg({__VA_ARGS__})
#define log_error(...) Ldap_logger::log_error_msg({__VA_ARGS__})
/**@}*/

#endif
