/*
   Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

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
  virtual void writeHeader(const char* pCategory, Logger::LoggerLevel level,
                           time_t now);
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
