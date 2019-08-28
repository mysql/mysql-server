/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef EVENTLOGGER_H
#define EVENTLOGGER_H

#include <logger/Logger.hpp>
#include <kernel/kernel_types.h>
#include <kernel/LogLevel.hpp>
#include <kernel/signaldata/EventReport.hpp>

class EventLoggerBase {
public:
  virtual ~EventLoggerBase();

  /**
   * LogLevel settings
   */
  LogLevel m_logLevel;
  
  /**
   * This matrix defines which event should be printed when
   *
   * threshold - is in range [0-15]
   * severity  - DEBUG to ALERT (Type of log message)
   */  
  typedef void (* EventTextFunction)(char *,size_t,const Uint32*, Uint32 len);

  struct EventRepLogLevelMatrix {
    Ndb_logevent_type       eventType;
    LogLevel::EventCategory eventCategory;
    Uint32                  threshold;
    Logger::LoggerLevel     severity;
    EventTextFunction       textF;
  };

  static const EventRepLogLevelMatrix matrix[];
  static const Uint32 matrixSize;
  static int event_lookup(int eventType,
			  LogLevel::EventCategory &cat,
			  Uint32 &threshold, 
			  Logger::LoggerLevel &severity,
			  EventTextFunction &textF);
};

/**
 * The EventLogger is primarily used for logging NDB events 
 * in the Management Server. It inherits all logging functionality of Logger.
 *
 * HOW TO USE
 *
 * 1) Create an EventLogger
 * 
 *   EventLogger myEventLogger = new EventLogger();
 * 
 * 2) Log NDB events and other log messages.
 *
 *   myEventLogger->info("Changing log levels.");
 *   
 *   EventReport* report = (EventReport*)&theSignalData[0];
 *   myEventLogger->log(eventReport->getEventType(), theSignalData, aNodeId);
 * 
 *
 * The following NDB event categories and log levels are enabled as default:
 *
 *  EVENT-CATEGORY LOG-LEVEL
 *
 *  Startup         4
 *  Shutdown        1
 *  Statistic       2 
 *  Checkpoint      5
 *  NodeRestart     8
 *  Connection      2
 *  Error          15 
 *  Info           10 
 *
 * @see Logger
 * @version #@ $Id: EventLogger.hpp,v 1.3 2003/09/01 10:15:52 innpeno Exp $
 */
class EventLogger : public EventLoggerBase, public Logger
{
public:
  /**
   * Default constructor. Enables default log levels and 
   * sets the log category to 'EventLogger'.
   */
  EventLogger();

  /**
   * Destructor.
   */
  virtual ~EventLogger();

  /**
   * Closes the eventlog.
   */
  void close();

  /**
   * Logs the NDB event.
   *
   * @param eventType the type of event.
   * @param theData the event data.
   * @param nodeId the node id of event origin.
   */
  virtual void log(int, const Uint32*, Uint32 len, NodeId = 0,const class LogLevel * = 0);

  
  /**
   * Returns the event text for the specified event report type.
   *
   * @param textF print function for the event
   * @param theData the event data.
   * @param nodeId a node id.
   * @return the event report text.
   */
  static const char* getText(char * dst, size_t dst_len,
			     EventTextFunction textF,
			     const Uint32* theData, Uint32 len, 
			     NodeId nodeId = 0);

private:
  /** Prohibit */
  EventLogger(const EventLogger&);
  EventLogger operator = (const EventLogger&);
  bool operator == (const EventLogger&);

  STATIC_CONST(MAX_TEXT_LENGTH = 384);
};

extern void getRestartAction(Uint32 action, BaseString &str);
#endif
