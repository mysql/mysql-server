/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "LogHandler.hpp"

#include <NdbTick.h>

//
// PUBLIC
//
LogHandler::LogHandler() : 
  m_pDateTimeFormat("%d-%.2d-%.2d %.2d:%.2d:%.2d"),
  m_errorCode(0)
{  
}

LogHandler::~LogHandler()
{  
}

void 
LogHandler::append(const char* pCategory, Logger::LoggerLevel level,
		   const char* pMsg)
{	
  writeHeader(pCategory, level);
  writeMessage(pMsg);
  writeFooter();
}	

const char* 
LogHandler::getDefaultHeader(char* pStr, const char* pCategory, 
			     Logger::LoggerLevel level) const
{
  char time[MAX_DATE_TIME_HEADER_LENGTH];
  BaseString::snprintf(pStr, MAX_HEADER_LENGTH, "%s [%s] %s -- ", 
	     getTimeAsString((char*)time),
	     pCategory,
	     Logger::LoggerLevelNames[level]);
 
  return pStr;
}


const char* 
LogHandler::getDefaultFooter() const
{
  return "\n";
}

const char* 
LogHandler::getDateTimeFormat() const
{
  return m_pDateTimeFormat;	
}

void 
LogHandler::setDateTimeFormat(const char* pFormat)
{
  m_pDateTimeFormat = (char*)pFormat;
}

char* 
LogHandler::getTimeAsString(char* pStr) const 
{
  struct tm* tm_now;
  time_t now;
  now = ::time((time_t*)NULL);
#ifdef NDB_WIN32
  tm_now = localtime(&now);
#else
  tm_now = ::localtime(&now); //uses the "current" timezone
#endif

  BaseString::snprintf(pStr, MAX_DATE_TIME_HEADER_LENGTH, 
	     m_pDateTimeFormat, 
	     tm_now->tm_year + 1900, 
	     tm_now->tm_mon + 1, //month is [0,11]. +1 -> [1,12]
	     tm_now->tm_mday,
	     tm_now->tm_hour,
	     tm_now->tm_min,
	     tm_now->tm_sec);
  
  return pStr;
}

int 
LogHandler::getErrorCode() const
{
  return m_errorCode;
}

void 
LogHandler::setErrorCode(int code)
{
  m_errorCode = code;
}

bool
LogHandler::parseParams(const BaseString &_params) {
  Vector<BaseString> v_args;

  bool ret = true;

  _params.split(v_args, ",");
  for(size_t i=0; i < v_args.size(); i++) {
    Vector<BaseString> v_param_value;
    if(v_args[i].split(v_param_value, "=", 2) != 2)
      ret = false;
    else if (!setParam(v_param_value[0], v_param_value[1]))
      ret = false;
  }

  if(!checkParams())
    ret = false;
  return ret;
}

bool
LogHandler::checkParams() {
  return true;
}

//
// PRIVATE
//
