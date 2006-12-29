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

#ifndef LOGGERUNITTEST_H
#define LOGGERUNITTEST_H

#include "Logger.hpp"

/**
  * Unit test of Logger.
  *
  * @version #@ $Id: LoggerUnitTest.hpp,v 1.1 2002/03/13 17:55:31 eyualex Exp $
  */
class LoggerUnitTest
{
public:
  
  static bool testAll(const char* msg);
  static bool testOff(const char* msg);	
  static bool testAlert(const char* msg);
  static bool testCritical(const char* msg);
  static bool testError(const char* msg);
  static bool testWarning(const char* msg);
  static bool testInfo(const char* msg);
  static bool testDebug(const char* msg);
  static bool testInfoCritical(const char* msg);

  static bool logTo(Logger::LoggerLevel level, const char* msg);
  static bool logTo(Logger::LoggerLevel from, Logger::LoggerLevel to, const char* msg);
  
  void error(const char* msg);
  
  LoggerUnitTest();
  ~LoggerUnitTest();
};
#endif
