/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#ifndef LOGHANDLER_H
#define LOGHANDLER_H

#include <time.h>

#include "Logger.hpp"

/**
 * This class is the base class for all log handlers. A log handler is
 * responsible for formatting and writing log messages to a specific output.
 *
 * A log entry consists of three parts: a header, <body/log message and a
 * footer. <pre> 09:17:37 2002-03-13 [MgmSrv] INFO     -- Local checkpoint 13344
 * started.
 * </pre>
 *
 * Header format: TIME&DATE CATEGORY LEVEL --
 *   TIME&DATE = ctime() format.
 *   CATEGORY  = Any string.
 *   LEVEL     = ALERT to DEBUG (Log levels)
 *
 * Footer format: \n (currently only newline)
 *
 * @version #@ $Id: LogHandler.hpp,v 1.7 2003/09/01 10:15:53 innpeno Exp $
 */
class LogHandler {
 public:
  /**
   * Default constructor.
   */
  LogHandler();

  /**
   * Destructor.
   */
  virtual ~LogHandler();

  virtual const char *handler_type() { return "NONE"; }

  /**
   * Opens/initializes the log handler.
   *
   * @return true if successful.
   */
  virtual bool open() = 0;

  /**
   * Closes/free any allocated resources used by the log handler.
   *
   * @return true if successful.
   */
  virtual bool close() = 0;

  /**
   * Check if LogHandler is open
   *
   * @return true if open.
   */
  virtual bool is_open() = 0;

  /**
   * Append a log message to the output stream/file whatever.
   * append() will call writeHeader(), writeMessage() and writeFooter() for
   * a child class and in that order. Append checks for repeated messages.
   * append_impl() does not check for repeats.
   *
   * @param pCategory the category/name to tag the log entry with.
   * @param level the log level.
   * @param pMsg the log message.
   */
  virtual void append(const char *pCategory, Logger::LoggerLevel level,
                      const char *pMsg, time_t now);
  void append_impl(const char *pCategory, Logger::LoggerLevel level,
                   const char *pMsg, time_t now);

  /**
   * Returns a default formatted header. It currently has the
   * following default format: '%H:%M:%S %Y-%m-%d [CATEGORY] LOGLEVEL --'
   *
   * @param pStr the header string to format.
   * @param pCategory a category/name to tag the log entry with.
   * @param level the log level.
   * @return the header.
   */
  const char *getDefaultHeader(char *pStr, const char *pCategory,
                               Logger::LoggerLevel level, time_t now) const;

  /**
   * Returns a default formatted footer. Currently only returns a newline.
   *
   * @return the footer.
   */
  const char *getDefaultFooter() const;

  /**
   * Returns the error code.
   */
  int getErrorCode() const;

  /**
   * Sets the error code.
   *
   * @param code the error code.
   */
  void setErrorCode(int code);

  /**
   * Returns the error string.
   */
  const char *getErrorStr() const;

  /**
   * Sets the error string.
   *
   * @param str the error string.
   */
  void setErrorStr(const char *str);

  /**
   * Parse logstring parameters
   *
   * @param params list of parameters, formatted as "param=value",
   * entries separated by ","
   * @return true on success, false on failure
   */
  bool parseParams(const BaseString &params);

  /**
   * Sets a parameters. What parameters are accepted depends on the subclass.
   *
   * @param param name of parameter
   * @param value value of parameter
   */
  virtual bool setParam(const BaseString &param, const BaseString &value) = 0;

  /**
   * Checks that all necessary parameters have been set.
   *
   * @return true if all parameters are correctly set, false otherwise
   */
  virtual bool checkParams();

  /*
   * Set repeat frequency, 0 means disable special repeated message handling
   */
  virtual void setRepeatFrequency(unsigned val);

  /**
   * Sets the config BaseString to the part of the LogDestination parameter
   * needed in the config file to setup this LogHandler. i.e. passing the
   * output of getParams to parseParams should do "nothing"
   *
   * @param config where to store parameters
   */
  virtual bool getParams(BaseString &config [[maybe_unused]]) { return false; }

  virtual ndb_off_t getCurrentSize() { return -1; }
  virtual ndb_off_t getMaxSize() { return -1; }

  /** Max length of the header the log. */
  static constexpr Uint32 MAX_HEADER_LENGTH = 128;

 protected:
  /** Max length of footer in the log. */
  static constexpr Uint32 MAX_FOOTER_LENGTH = 128;

  /**
   * Write the header to the log.
   *
   * @param pCategory the category to tag the log with.
   * @param level the log level.
   */
  virtual void writeHeader(const char *pCategory, Logger::LoggerLevel level,
                           time_t now) = 0;

  /**
   * Write the message to the log.
   *
   * @param pMsg the message to log.
   */
  virtual void writeMessage(const char *pMsg) = 0;

  /**
   * Write the footer to the log.
   *
   */
  virtual void writeFooter() = 0;

 private:
  /** Prohibit */
  LogHandler(const LogHandler &);
  LogHandler *operator=(const LogHandler &);
  bool operator==(const LogHandler &);

  int m_errorCode;
  const char *m_errorStr;

  // for handling repeated messages
  unsigned m_count_repeated_messages;
  unsigned m_max_repeat_frequency;
  time_t m_last_log_time;
  char m_last_category[MAX_HEADER_LENGTH];
  char m_last_message[MAX_LOG_MESSAGE_SIZE];
  Logger::LoggerLevel m_last_level;
};

#endif
