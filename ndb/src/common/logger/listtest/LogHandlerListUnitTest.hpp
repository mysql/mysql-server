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

#ifndef LOGHANDLERLISTUNITTEST_H
#define LOGHANDLERLISTUNITTEST_H

#include "LogHandlerList.hpp"

/**
 * Unit test of LogHandlerList.
 *
 * @version #@ $Id: LogHandlerListUnitTest.hpp,v 1.1 2002/03/13 17:59:15 eyualex Exp $
 */
class LogHandlerListUnitTest
{
public:
  
  static bool testAdd(const char* msg);
  static bool testRemove(const char* msg);
  static bool testTraverseNext(const char* msg);

  void error(const char* msg);
  
  LogHandlerListUnitTest();
  ~LogHandlerListUnitTest();
};
#endif
