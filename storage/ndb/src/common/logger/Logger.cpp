/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>

#include "Logger.hpp"

#include <LogHandler.hpp>
#include <ConsoleLogHandler.hpp>
#include <FileLogHandler.hpp>
#include "LogHandlerList.hpp"

#if !defined NDB_WIN32
#include <SysLogHandler.hpp>
#endif

//
// PUBLIC
//
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
  m_pConsoleHandler(NULL),
  m_pFileHandler(NULL),
  m_pSyslogHandler(NULL)
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
Logger::createConsoleHandler()
{
  Guard g(m_handler_mutex);
  bool rc = true;

  if (m_pConsoleHandler == NULL)
  {
    m_pConsoleHandler = new ConsoleLogHandler(); 
    if (!addHandler(m_pConsoleHandler)) // TODO: check error code
    {
      rc = false;
      delete m_pConsoleHandler;
      m_pConsoleHandler = NULL;
    }
  }

  return rc;
}

void 
Logger::removeConsoleHandler()
{
  Guard g(m_handler_mutex);
  if (removeHandler(m_pConsoleHandler))
  {
    m_pConsoleHandler = NULL;
  }
}

bool
Logger::createFileHandler()
{
  Guard g(m_handler_mutex);
  bool rc = true;
  if (m_pFileHandler == NULL)
  {
    m_pFileHandler = new FileLogHandler(); 
    if (!addHandler(m_pFileHandler)) // TODO: check error code
    {
      rc = false;
      delete m_pFileHandler;
      m_pFileHandler = NULL;
    }
  }

  return rc;
}

void 
Logger::removeFileHandler()
{
  Guard g(m_handler_mutex);
  if (removeHandler(m_pFileHandler))
  {
    m_pFileHandler = NULL;
  }
}

bool
Logger::createSyslogHandler()
{
  Guard g(m_handler_mutex);
  bool rc = true;
  if (m_pSyslogHandler == NULL)
  {
#if defined NDB_WIN32
    m_pSyslogHandler = new ConsoleLogHandler(); 
#else
    m_pSyslogHandler = new SysLogHandler(); 
#endif
    if (!addHandler(m_pSyslogHandler)) // TODO: check error code
    {
      rc = false;
      delete m_pSyslogHandler;
      m_pSyslogHandler = NULL;
    }
  }

  return rc;
}

void 
Logger::removeSyslogHandler()
{
  Guard g(m_handler_mutex);
  if (removeHandler(m_pSyslogHandler))
  {
    m_pSyslogHandler = NULL;
  }
}

bool
Logger::addHandler(LogHandler* pHandler)
{
  Guard g(m_mutex);
  assert(pHandler != NULL);

  bool rc = pHandler->open();	
  if (rc)
  {
    m_pHandlerList->add(pHandler);
  }
  else
  {
    delete pHandler;
  }	

  return rc;
}

bool
Logger::addHandler(const BaseString &logstring, int *err, int len, char* errStr) {
  size_t i;
  Vector<BaseString> logdest;
  Vector<LogHandler *>loghandlers;
  DBUG_ENTER("Logger::addHandler");

  logstring.split(logdest, ";");

  for(i = 0; i < logdest.size(); i++) {
    DBUG_PRINT("info",("adding: %s",logdest[i].c_str()));

    Vector<BaseString> v_type_args;
    logdest[i].split(v_type_args, ":", 2);

    BaseString type(v_type_args[0]);
    BaseString params;
    if(v_type_args.size() >= 2)
      params = v_type_args[1];

    LogHandler *handler = NULL;

#ifndef NDB_WIN32
    if(type == "SYSLOG")
    {
      handler = new SysLogHandler();
    } else 
#endif
    if(type == "FILE")
      handler = new FileLogHandler();
    else if(type == "CONSOLE")
      handler = new ConsoleLogHandler();
    
    if(handler == NULL)
    {
      snprintf(errStr,len,"Could not create log destination: %s",
               logdest[i].c_str());
      DBUG_RETURN(false);
    }
    if(!handler->parseParams(params))
    {
      *err= handler->getErrorCode();
      if(handler->getErrorStr())
        strncpy(errStr, handler->getErrorStr(), len);
      DBUG_RETURN(false);
    }
    loghandlers.push_back(handler);
  }
  
  for(i = 0; i < loghandlers.size(); i++)
    addHandler(loghandlers[i]);
  
  DBUG_RETURN(true); /* @todo handle errors */
}

bool
Logger::removeHandler(LogHandler* pHandler)
{
  Guard g(m_mutex);
  int rc = false;
  if (pHandler != NULL)
  {
    rc = m_pHandlerList->remove(pHandler);
  }

  return rc;
}

void
Logger::removeAllHandlers()
{
  Guard g(m_mutex);
  m_pHandlerList->removeAll();
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

//
// PROTECTED
//

void 
Logger::log(LoggerLevel logLevel, const char* pMsg, va_list ap) const
{
  Guard g(m_mutex);
  if (m_logLevels[LL_ON] && m_logLevels[logLevel])
  {
    char buf[MAX_LOG_MESSAGE_SIZE];
    BaseString::vsnprintf(buf, sizeof(buf), pMsg, ap);
    LogHandler* pHandler = NULL;
    while ( (pHandler = m_pHandlerList->next()) != NULL)
    {
      pHandler->append(m_pCategory, logLevel, buf);
    }
  }
}

//
// PRIVATE
//

template class Vector<LogHandler*>;
