/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef Logger_H
#define Logger_H

#include <ndb_global.h>
#include "portlib/ndb_compiler.h"
#include <time.h>
#include <BaseString.hpp>
#include <NdbOut.hpp>

#define MAX_LOG_MESSAGE_SIZE 1024

class LogHandler;
class LogHandlerList;

/**
 * Logger should be used whenever you need to log a message like
 * general information or debug messages. By creating/adding different
 * log handlers, a single log message can be sent to 
 * different outputs (stdout, file or syslog).
 * 
 * Each log entry is created with a log level (or severity) which is 
 * used to identity the type of the entry, e.g., if it is a debug 
 * or an error message.
 * 
 * Example of a log entry:
 *
 *  09:17:39 2002-03-13 [myLogger] INFO -- Local checkpoint started.
 *
 * HOW TO USE
 *
 * 1) Create a new instance of the Logger.
 *
 *    Logger myLogger = new Logger();
 *
 * 2) Add the log handlers that you want, i.e., where the log entries 
 *    should be written/shown.
 *
 *    myLogger->createConsoleHandler();  // Output to console/stdout
 *    myLogger->addHandler(new FileLogHandler("mylog.txt")); // use mylog.txt
 *
 *  3) Tag each log entry with a category/name.
 *
 *    myLogger->setCategory("myLogger");
 *
 * 4) Start log messages.
 *    
 *     myLogger->alert("T-9 to lift off");
 *     myLogger->info("Here comes the sun, la la"); 
 *     myLogger->debug("Why does this not work!!!, We should not be here...")
 *
 * 5) Log only debug messages.
 *
 *    myLogger->enable(Logger::LL_DEBUG);
 *
 * 6) Log only ALERTS and ERRORS.
 *
 *    myLogger->enable(Logger::LL_ERROR, Logger::LL_ALERT);
 * 
 * 7) Do not log any messages.
 *
 *    myLogger->disable(Logger::LL_ALL);
 *
 *
 * LOG LEVELS (Matches the severity levels of syslog)
 * <pre>
 *
 *  ALERT           A condition  that  should  be  corrected
 *                  immediately,  such as a corrupted system
 *                  database.
 *
 *  CRITICAL        Critical conditions, such as hard device
 *                  errors.
 *
 *  ERROR           Errors.
 *
 *  WARNING         Warning messages.
 *
 *  INFO            Informational messages.
 *
 *  DEBUG           Messages that contain  information  nor-
 *                  mally  of use only when debugging a pro-
 *                  gram.
 * </pre>
 *
 * @version #@ $Id: Logger.hpp,v 1.7 2003/09/01 10:15:53 innpeno Exp $
 */
class Logger
{
public:


  /*
    Convert time to local timezone and print in timestamp format
    to string buffer. The function always write some null terminated
    string to buffer so it can safely be printed.

      @epoch time to convert and print
      @str pointer to buffer where to print the resulting string
      @len max length of result buffer

  */
  static void format_timestamp(const time_t epoch,
                               char* str, size_t len);

  /** The log levels. NOTE: Could not use the name LogLevel since 
   * it caused conflicts with another class.
   */
  enum LoggerLevel {LL_ON, LL_DEBUG, LL_INFO, LL_WARNING, LL_ERROR,
		    LL_CRITICAL, LL_ALERT, LL_ALL};
  
  /**
   * String representation of the the log levels.
   */
  static const char* LoggerLevelNames[];

  /**
   * Default constructor.
   */
  Logger();

  /**
   * Destructor.
   */
  virtual ~Logger();
  
  /**
   * Set a category/name that each log entry will have.
   *
   * @param pCategory the category.
   */
  void setCategory(const char* pCategory);

  /**
   * Create a default handler that logs to the console/stdout.
   *
   * @return true if successful.
   */
  bool createConsoleHandler(NdbOut &out= ndbout);

  /**
   * Remove the default console handler.
   */
  void removeConsoleHandler();

  /**
   * Create a default handler that logs to the Windows event log
   * with source component set to source_name
   *
   * NOTE! Can only  be created on Windows.
   *
   * @return true if successful.
   */
#ifdef _WIN32
  bool createEventLogHandler(const char* source_name);
#endif

  /**
   * Create a default handler which writes to the specified file name.
   *
   * @return true if successful.
   */
  bool createFileHandler(char* filename);

  /**
   * Remove the default file handler.
   */
  void removeFileHandler();

  /**
   * Create a default handler that logs to the syslog.
   *
   * @return true if successful.
   */
  bool createSyslogHandler();	

  /**
   * Remove the default syslog handler.
   */
  void removeSyslogHandler();

  /**
   * Add a new log handler.
   *
   * @param pHandler a log handler.
   * @return true if successful.
   */
  bool addHandler(LogHandler* pHandler);

  /**
   * Remove a log handler.
   *
   * @param pHandler log handler to remove.
   * @return true if successful.
   */
  bool removeHandler(LogHandler* pHandler);

  /**
   * Remove all log handlers.
   */
  void removeAllHandlers();

  /**
   * Returns true if the specified log level is enabled.
   *
   * @return true if enabled.
   */
  bool isEnable(LoggerLevel logLevel) const; 

  /**
   * Enable the specified log level.
   *
   * @param logLevel the loglevel to enable.
   */
  void enable(LoggerLevel logLevel);

  /**
   * Enable log levels.
   *
   * @param fromLogLevel enable from log level.
   * @param toLogLevel enable to log level.
   */
  void enable (LoggerLevel fromLogLevel, LoggerLevel toLogLevel);

  /**
   * Disable log level.
   *
   * @param logLevel disable log level.
   */
  void disable(LoggerLevel logLevel);

  /**
   * Log an alert message.
   *
   * @param pMsg the message.
   */
  virtual void alert(const char* pMsg, ...) const
    ATTRIBUTE_FORMAT(printf, 2, 3);
  virtual void alert(BaseString &pMsg) const { alert("%s", pMsg.c_str()); }
  
  /**
   * Log a critical message.
   *
   * @param pMsg the message.
   */
  virtual void critical(const char* pMsg, ...) const
    ATTRIBUTE_FORMAT(printf, 2, 3);
  virtual void critical(BaseString &pMsg) const { critical("%s", pMsg.c_str()); }

  /**
   * Log an error message.
   *
   * @param pMsg the message.
   */
  virtual void error(const char* pMsg, ...) const
    ATTRIBUTE_FORMAT(printf, 2, 3);
  virtual void error(BaseString &pMsg) const { error("%s", pMsg.c_str()); }

  /**
   * Log a warning message.
   *
   * @param pMsg the message.
   */
  virtual void warning(const char* pMsg, ...) const
    ATTRIBUTE_FORMAT(printf, 2, 3);
  virtual void warning(BaseString &pMsg) const { warning("%s", pMsg.c_str()); }

  /**
   * Log an info message.
   *
   * @param pMsg the message.
   */
  virtual void info(const char* pMsg, ...) const
    ATTRIBUTE_FORMAT(printf, 2, 3);
  virtual void info(BaseString &pMsg) const { info("%s", pMsg.c_str()); }

  /**
   * Log a debug message.
   *
   * @param pMsg the message.
   */
  virtual void debug(const char* pMsg, ...) const
    ATTRIBUTE_FORMAT(printf, 2, 3);
  virtual void debug(BaseString &pMsg) const { debug("%s", pMsg.c_str()); }

  /*
   * Set repeat frequency, 0 means disable special repeated message handling
   */
  virtual void setRepeatFrequency(unsigned val);

protected:

  NdbMutex *m_mutex;

  void log(LoggerLevel logLevel, const char* msg, va_list ap) const
    ATTRIBUTE_FORMAT(printf, 3, 0);

private:
  /** Prohibit */
  Logger(const Logger&);
  Logger operator = (const Logger&);
  bool operator == (const Logger&);

  static constexpr Uint32 MAX_LOG_LEVELS = 8;

  bool m_logLevels[MAX_LOG_LEVELS];
  
  LogHandlerList* m_pHandlerList;
  const char* m_pCategory;

  /* Default handlers */
  NdbMutex *m_handler_mutex;
  LogHandler* m_pConsoleHandler;
  LogHandler* m_pFileHandler;
  LogHandler* m_pSyslogHandler;
};

#endif
