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

#include "ConsoleLogHandler.hpp"

ConsoleLogHandler::ConsoleLogHandler(NdbOut& out)
 : LogHandler(), _out(out)
{
}

ConsoleLogHandler::~ConsoleLogHandler()
{

}

bool
ConsoleLogHandler::open()
{
  return true;
}

bool
ConsoleLogHandler::close()
{
  return true;
}

bool
ConsoleLogHandler::is_open()
{
  return true;
}

//
// PROTECTED
//
void 
ConsoleLogHandler::writeHeader(const char* pCategory, Logger::LoggerLevel level,
                               time_t now)
{
  char str[MAX_HEADER_LENGTH];
  _out << getDefaultHeader(str, pCategory, level, now);
}

void 
ConsoleLogHandler::writeMessage(const char* pMsg)
{
  _out << pMsg;	
}

void 
ConsoleLogHandler::writeFooter()
{
  _out << getDefaultFooter() << flush;
}

  
bool
ConsoleLogHandler::setParam(const BaseString &param, const BaseString &value) {
  return false;
}
