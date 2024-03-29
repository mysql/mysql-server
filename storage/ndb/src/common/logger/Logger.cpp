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

#include <ndb_global.h>

#include "Logger.hpp"

#include <time.h>

#include <LogHandler.hpp>
#include <ConsoleLogHandler.hpp>
#include <FileLogHandler.hpp>
#include "LogHandlerList.hpp"

#ifdef _WIN32
#include "EventLogHandler.hpp"
#else
#include <SysLogHandler.hpp>
#endif

#include <portlib/ndb_localtime.h>

const char* Logger::LoggerLevelNames[] = { "ON      ", 
					   "DEBUG   ",
					   "INFO    ",
					   "WARNING ",
					   "ERROR   ",
					   "CRITICAL",
					   "ALERT   ",
					   "ALL     "
					 };
Logger::Logger() : 
  m_pCategory("Logger"),
  m_pConsoleHandler(nullptr),
  m_pFileHandler(nullptr),
  m_pSyslogHandler(nullptr)
{
  m_pHandlerList = new LogHandlerList();
  m_mutex= NdbMutex_Create();
  m_handler_mutex= NdbMutex_Create();
  disable(LL_ALL);
  enable(LL_ON);
  enable(LL_INFO);
}

Logger::~Logger()
{
  removeAllHandlers();
  delete m_pHandlerList;
  NdbMutex_Destroy(m_handler_mutex);
  NdbMutex_Destroy(m_mutex);
}

void 
Logger::setCategory(const char* pCategory)
{
  Guard g(m_mutex);
  m_pCategory = pCategory;
}

bool
Logger::createConsoleHandler(NdbOut &out)
{
  Guard g(m_handler_mutex);

  if (m_pConsoleHandler)
    return true; // Ok, already exist

  LogHandler* log_handler = new ConsoleLogHandler(out);
  if (!log_handler)
    return false;

  if (!addHandler(log_handler))
  {
    delete log_handler;
    return false;
  }

  m_pConsoleHandler = log_handler;
  return true;
}

void 
Logger::removeConsoleHandler()
{
  Guard g(m_handler_mutex);
  if (removeHandler(m_pConsoleHandler))
  {
    m_pConsoleHandler = nullptr;
  }
}

#ifdef _WIN32
bool
Logger::createEventLogHandler(const char* source_name)
{
  Guard g(m_handler_mutex);

  LogHandler* log_handler = new EventLogHandler(source_name);
  if (!log_handler)
    return false;

  if (!addHandler(log_handler))
  {
    delete log_handler;
    return false;
  }

  return true;
}
#endif

bool
Logger::createFileHandler(char*filename)
{
  Guard g(m_handler_mutex);

  if (m_pFileHandler)
    return true; // Ok, already exist

  LogHandler* log_handler = new FileLogHandler(filename);
  if (!log_handler)
    return false;

  if (!addHandler(log_handler))
  {
    delete log_handler;
    return false;
  }

  m_pFileHandler = log_handler;
  return true;
}

void 
Logger::removeFileHandler()
{
  Guard g(m_handler_mutex);
  if (removeHandler(m_pFileHandler))
  {
    m_pFileHandler = nullptr;
  }
}

bool
Logger::createSyslogHandler()
{
#ifdef _WIN32
  return false;
#else
  Guard g(m_handler_mutex);

  if (m_pSyslogHandler)
    return true; // Ok, already exist

  LogHandler* log_handler = new SysLogHandler();
  if (!log_handler)
    return false;

  if (!addHandler(log_handler))
  {
    delete log_handler;
    return false;
  }

  m_pSyslogHandler = log_handler;
  return true;
#endif
}

void 
Logger::removeSyslogHandler()
{
  Guard g(m_handler_mutex);
  if (removeHandler(m_pSyslogHandler))
  {
    m_pSyslogHandler = nullptr;
  }
}

bool
Logger::addHandler(LogHandler* pHandler)
{
  Guard g(m_mutex);
  assert(pHandler != nullptr);

  if (!pHandler->is_open() &&
      !pHandler->open())
  {
    // Failed to open
    return false;
  }

  if (!m_pHandlerList->add(pHandler))
    return false;

  return true;
}


bool
Logger::removeHandler(LogHandler* pHandler)
{
  Guard g(m_mutex);
  int rc = false;
  if (pHandler != nullptr)
  {
    if (pHandler == m_pConsoleHandler)
      m_pConsoleHandler= nullptr;
    if (pHandler == m_pFileHandler)
      m_pFileHandler= nullptr;
    if (pHandler == m_pSyslogHandler)
      m_pSyslogHandler= nullptr;

    rc = m_pHandlerList->remove(pHandler);
  }

  return rc;
}

void
Logger::removeAllHandlers()
{
  Guard g(m_mutex);
  m_pHandlerList->removeAll();

  m_pConsoleHandler= nullptr;
  m_pFileHandler= nullptr;
  m_pSyslogHandler= nullptr;
}

bool
Logger::isEnable(LoggerLevel logLevel) const
{
  Guard g(m_mutex);
  if (logLevel == LL_ALL)
  {
    for (unsigned i = 1; i < MAX_LOG_LEVELS; i++)
      if (!m_logLevels[i])
	return false;
    return true;
  }
  return m_logLevels[logLevel];
}

void
Logger::enable(LoggerLevel logLevel)
{
  Guard g(m_mutex);
  if (logLevel == LL_ALL)
  {
    for (unsigned i = 0; i < MAX_LOG_LEVELS; i++)
    {
      m_logLevels[i] = true;
    }
  }
  else 
  {
    m_logLevels[logLevel] = true;
  }
}

void 
Logger::enable(LoggerLevel fromLogLevel, LoggerLevel toLogLevel)
{
  Guard g(m_mutex);
  if (fromLogLevel > toLogLevel)
  {
    LoggerLevel tmp = toLogLevel;
    toLogLevel = fromLogLevel;
    fromLogLevel = tmp;
  }

  for (int i = fromLogLevel; i <= toLogLevel; i++)
  {
    m_logLevels[i] = true;
  } 
}

void
Logger::disable(LoggerLevel logLevel)
{
  Guard g(m_mutex);
  if (logLevel == LL_ALL)
  {
    for (unsigned i = 0; i < MAX_LOG_LEVELS; i++)
    {
      m_logLevels[i] = false;
    }
  }
  else
  {
    m_logLevels[logLevel] = false;
  }
}

void 
Logger::alert(const char* pMsg, ...) const
{
  va_list ap;
  va_start(ap, pMsg);
  log(LL_ALERT, pMsg, ap);
  va_end(ap);
}

void 
Logger::critical(const char* pMsg, ...) const
{
  va_list ap;
  va_start(ap, pMsg);
  log(LL_CRITICAL, pMsg, ap);  
  va_end(ap);
}
void 
Logger::error(const char* pMsg, ...) const
{
  va_list ap;
  va_start(ap, pMsg);
  log(LL_ERROR, pMsg, ap);  
  va_end(ap);
}
void 
Logger::warning(const char* pMsg, ...) const
{
  va_list ap;
  va_start(ap, pMsg);
  log(LL_WARNING, pMsg, ap);
  va_end(ap);
}

void 
Logger::info(const char* pMsg, ...) const
{
  va_list ap;
  va_start(ap, pMsg);
  log(LL_INFO, pMsg, ap);
  va_end(ap);
}

void 
Logger::debug(const char* pMsg, ...) const
{
  va_list ap;
  va_start(ap, pMsg);
  log(LL_DEBUG, pMsg, ap);
  va_end(ap);
}

void 
Logger::log(LoggerLevel logLevel, const char* pMsg, va_list ap) const
{
  Guard g(m_mutex);
  if (m_logLevels[LL_ON] && m_logLevels[logLevel])
  {
    char buf[MAX_LOG_MESSAGE_SIZE];
    BaseString::vsnprintf(buf, sizeof(buf), pMsg, ap);
    LogHandler* pHandler = nullptr;
    while ( (pHandler = m_pHandlerList->next()) != nullptr)
    {
      time_t now = ::time((time_t*)nullptr);
      pHandler->append(m_pCategory, logLevel, buf, now);
    }
  }
}

void Logger::setRepeatFrequency(unsigned val)
{
  LogHandler* pHandler;
  while ((pHandler = m_pHandlerList->next()) != nullptr)
  {
    pHandler->setRepeatFrequency(val);
  }
}


void
Logger::format_timestamp(const time_t epoch,
                         char* str, size_t len)
{
  assert(len > 0); // Assume buffer has size

  // convert to local timezone
  tm tm_buf;
  if (ndb_localtime_r(&epoch, &tm_buf) == nullptr)
  {
    // Failed to convert to local timezone.
    // Fill with bogus time stamp value in order
    // to ensure buffer can be safely printed
    strncpy(str, "2001-01-01 00:00:00", len);
    str[len-1] = 0;
    return;
  }

  // Print the broken down time in timestamp format
  // to the string buffer
  BaseString::snprintf(str, len,
                       "%d-%.2d-%.2d %.2d:%.2d:%.2d",
                       tm_buf.tm_year + 1900,
                       tm_buf.tm_mon + 1, //month is [0,11]. +1 -> [1,12]
                       tm_buf.tm_mday,
                       tm_buf.tm_hour,
                       tm_buf.tm_min,
                       tm_buf.tm_sec);
  str[len-1] = 0;
  return;
}
