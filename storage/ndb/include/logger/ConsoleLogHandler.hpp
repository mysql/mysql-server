/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CONSOLELOGHANDLER_H
#define CONSOLELOGHANDLER_H

#include "LogHandler.hpp"
#include <NdbOut.hpp>

/**
 * Logs messages to the console/stdout.
 *
 * @see LogHandler
 * @version #@ $Id: ConsoleLogHandler.hpp,v 1.2 2003/09/01 10:15:53 innpeno Exp $
 */
class ConsoleLogHandler : public LogHandler
{
public:
  /**
   * Default constructor.
   */
  ConsoleLogHandler(NdbOut &out= ndbout);
  /**
   * Destructor.
   */
  virtual ~ConsoleLogHandler();
  
  virtual bool open();
  virtual bool close();

  virtual bool is_open();

  virtual bool setParam(const BaseString &param, const BaseString &value);
  
protected:	
  virtual void writeHeader(const char* pCategory, Logger::LoggerLevel level,
                           time_t now);
  virtual void writeMessage(const char* pMsg);
  virtual void writeFooter();
  NdbOut& _out;

private:
  /** Prohibit*/
  ConsoleLogHandler(const ConsoleLogHandler&);
  ConsoleLogHandler operator = (const ConsoleLogHandler&);
  bool operator == (const ConsoleLogHandler&);

};
#endif
