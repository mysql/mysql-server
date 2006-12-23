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

#include "ConsoleLogHandler.hpp"

#include <NdbOut.hpp>

ConsoleLogHandler::ConsoleLogHandler() : LogHandler()
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

//
// PROTECTED
//
void 
ConsoleLogHandler::writeHeader(const char* pCategory, Logger::LoggerLevel level)
{
  char str[LogHandler::MAX_HEADER_LENGTH];
  ndbout << getDefaultHeader(str, pCategory, level);	
}

void 
ConsoleLogHandler::writeMessage(const char* pMsg)
{
  ndbout << pMsg;	
}

void 
ConsoleLogHandler::writeFooter()
{
  ndbout << getDefaultFooter() << flush;
}

  
bool
ConsoleLogHandler::setParam(const BaseString &param, const BaseString &value) {
  return false;
}
