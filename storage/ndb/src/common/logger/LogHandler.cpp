/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "LogHandler.hpp"

#include <NdbTick.h>
#include <time.h>

#include "util/cstrbuf.h"

//
// PUBLIC
//
LogHandler::LogHandler() : m_errorCode(0), m_errorStr(nullptr) {
  m_max_repeat_frequency = 3;  // repeat messages maximum every 3 seconds
  m_count_repeated_messages = 0;
  m_last_category[0] = 0;
  m_last_message[0] = 0;
  m_last_log_time = 0;
  m_last_level = Logger::LL_UNDEFINED_LEVEL;
}

LogHandler::~LogHandler() {}

void LogHandler::append(const char *pCategory, Logger::LoggerLevel level,
                        const char *pMsg, time_t now) {
  if (m_max_repeat_frequency == 0 || level != m_last_level ||
      strcmp(pCategory, m_last_category) || strcmp(pMsg, m_last_message)) {
    if (m_count_repeated_messages > 0)  // print that message
      append_impl(m_last_category, m_last_level, m_last_message, now);

    m_last_level = level;
    if (cstrbuf_copy(m_last_category, pCategory) == 1) {
      // truncated category
    }
    if (cstrbuf_copy(m_last_message, pMsg) == 1) {
      // truncated message
    }
  } else  // repeated message
  {
    if (now < (time_t)(m_last_log_time + m_max_repeat_frequency)) {
      m_count_repeated_messages++;
      return;
    }
  }

  append_impl(pCategory, level, pMsg, now);
  m_last_log_time = now;
}

void LogHandler::append_impl(const char *pCategory, Logger::LoggerLevel level,
                             const char *pMsg, time_t now) {
  writeHeader(pCategory, level, now);
  if (m_count_repeated_messages <= 1)
    writeMessage(pMsg);
  else {
    BaseString str(pMsg);
    str.appfmt(" - Repeated %d times", m_count_repeated_messages);
    writeMessage(str.c_str());
  }
  m_count_repeated_messages = 0;
  writeFooter();
}

const char *LogHandler::getDefaultHeader(char *pStr, const char *pCategory,
                                         Logger::LoggerLevel level,
                                         time_t now) const {
  char timestamp[64];
  Logger::format_timestamp(now, timestamp, sizeof(timestamp));

  BaseString::snprintf(pStr, MAX_HEADER_LENGTH, "%s [%s] %s -- ", timestamp,
                       pCategory, Logger::LoggerLevelNames[level]);
  return pStr;
}

const char *LogHandler::getDefaultFooter() const { return "\n"; }

int LogHandler::getErrorCode() const { return m_errorCode; }

void LogHandler::setErrorCode(int code) { m_errorCode = code; }

const char *LogHandler::getErrorStr() const { return m_errorStr; }

void LogHandler::setErrorStr(const char *str) { m_errorStr = str; }

bool LogHandler::parseParams(const BaseString &_params) {
  Vector<BaseString> v_args;

  bool ret = true;

  _params.split(v_args, ",");
  for (unsigned i = 0; i < v_args.size(); i++) {
    Vector<BaseString> v_param_value;
    if (v_args[i].split(v_param_value, "=", 2) != 2) {
      ret = false;
      setErrorStr("Can't find key=value pair.");
    } else {
      v_param_value[0].trim(" \t");
      if (!setParam(v_param_value[0], v_param_value[1])) {
        ret = false;
      }
    }
  }

  if (!checkParams()) ret = false;
  return ret;
}

bool LogHandler::checkParams() { return true; }

void LogHandler::setRepeatFrequency(unsigned val) {
  m_max_repeat_frequency = val;
}

//
// PRIVATE
//
