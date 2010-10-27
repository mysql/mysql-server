/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef EVENTLOGHANDLER_H
#define EVENTLOGHANDLER_H

#include "LogHandler.hpp"

/**
 * Log messages to the Windows event log
 *
 * Example:
 *  // To make everything written to g_eventLogger also
 *   // end up in Windows event log
 *  g_eventLogger->createEventLoghandler("MySQL Cluster Management Server");
 *
 * // To log a message(normally an error) before g_eventLogger has been created
 * EventLogHandler::printf(LL_ERROR, "MySQL Cluster Management Server",
 *                         "Failed to create shutdown event, error: %d", err); 
 */
class EventLogHandler : public LogHandler
{
public:
  EventLogHandler(const char* source_name);
  virtual ~EventLogHandler();

  virtual bool open();
  virtual bool close();
  virtual bool is_open();

  virtual bool setParam(const BaseString &param, const BaseString &value);

  // Write message to event log without an open EventLogHandler
  static int printf(Logger::LoggerLevel m_level, const char* source_name,
                    const char* msg, ...) ATTRIBUTE_FORMAT(printf, 3, 4);
private:
  virtual void writeHeader(const char* pCategory, Logger::LoggerLevel level);
  virtual void writeMessage(const char* pMsg);
  virtual void writeFooter();

  EventLogHandler(const EventLogHandler&); // Not impl.
  EventLogHandler operator = (const EventLogHandler&); // Not impl.
  bool operator == (const EventLogHandler&); // Not impl.

  const char* m_source_name;
  HANDLE m_event_source;
  Logger::LoggerLevel m_level;
};
#endif
