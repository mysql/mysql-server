/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef CONSOLELOGHANDLER_H
#define CONSOLELOGHANDLER_H

#include <time.h>

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
  ~ConsoleLogHandler() override;
  
  bool open() override;
  bool close() override;

  bool is_open() override;

  bool setParam(const BaseString &param, const BaseString &value) override;
  
protected:	
  void writeHeader(const char* pCategory, Logger::LoggerLevel level,
                   time_t now) override;
  void writeMessage(const char* pMsg) override;
  void writeFooter() override;
  NdbOut& _out;

private:
  /** Prohibit*/
  ConsoleLogHandler(const ConsoleLogHandler&);
  ConsoleLogHandler operator = (const ConsoleLogHandler&);
  bool operator == (const ConsoleLogHandler&);

};
#endif
