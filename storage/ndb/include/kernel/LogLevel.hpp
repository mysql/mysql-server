/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _LOG_LEVEL_HPP
#define _LOG_LEVEL_HPP

#include <ndb_global.h>
#include <mgmapi_config_parameters.h>

#define JAM_FILE_ID 3


/**
 * 
 */
class LogLevel {
  friend class Config;
public:
  /**
   * Constructor
   */
  LogLevel();
  
  /**
   * Howto add a new event category:
   * 1. Add the new event category to EventCategory below
   * 2. Update #define _LOGLEVEL_CATEGORIES (found below) with the number of 
   *    items in EventCategory 
   * 3. Update LogLevelCategoryName in LogLevel.cpp
   * 4. Add the event in EventLogger
   */


  /**
   * Copy operator
   */
  LogLevel & operator= (const LogLevel &);

  enum EventCategory {
    llInvalid = -1,
    llStartUp = CFG_LOGLEVEL_STARTUP - CFG_MIN_LOGLEVEL,
    llShutdown = CFG_LOGLEVEL_SHUTDOWN - CFG_MIN_LOGLEVEL,
    llStatistic = CFG_LOGLEVEL_STATISTICS - CFG_MIN_LOGLEVEL,
    llCheckpoint = CFG_LOGLEVEL_CHECKPOINT - CFG_MIN_LOGLEVEL,
    llNodeRestart = CFG_LOGLEVEL_NODERESTART - CFG_MIN_LOGLEVEL,
    llConnection = CFG_LOGLEVEL_CONNECTION - CFG_MIN_LOGLEVEL,
    llInfo = CFG_LOGLEVEL_INFO - CFG_MIN_LOGLEVEL,
    llWarning = CFG_LOGLEVEL_WARNING - CFG_MIN_LOGLEVEL,
    llError = CFG_LOGLEVEL_ERROR - CFG_MIN_LOGLEVEL,
    llCongestion = CFG_LOGLEVEL_CONGESTION - CFG_MIN_LOGLEVEL,
    llDebug = CFG_LOGLEVEL_DEBUG - CFG_MIN_LOGLEVEL
    ,llBackup = CFG_LOGLEVEL_BACKUP - CFG_MIN_LOGLEVEL
    ,llSchema = CFG_LOGLEVEL_SCHEMA - CFG_MIN_LOGLEVEL
  };

  /**
   * No of categories
   */
#define _LOGLEVEL_CATEGORIES (CFG_MAX_LOGLEVEL - CFG_MIN_LOGLEVEL + 1)
  STATIC_CONST( LOGLEVEL_CATEGORIES = _LOGLEVEL_CATEGORIES );
  
  void clear();
  
  /**
   * Note level is valid as 0-15
   */
  int setLogLevel(EventCategory ec, Uint32 level = 7);
  
  /**
   * Get the loglevel (0-15) for a category
   */
  Uint32 getLogLevel(EventCategory ec) const;
  
  /**
   * Set this= max(this, ll) per category
   */
  LogLevel& set_max(const LogLevel& ll);
  
  bool operator==(const LogLevel& l) const { 
    return memcmp(this, &l, sizeof(* this)) == 0;
  }

private:
  /**
   * The actual data
   */
  Uint8 logLevelData[LOGLEVEL_CATEGORIES];
};

inline
LogLevel::LogLevel(){
  clear();
}

inline
LogLevel & 
LogLevel::operator= (const LogLevel & org){
  memcpy(logLevelData, org.logLevelData, sizeof(logLevelData));
  return * this;
}

inline
void
LogLevel::clear(){
  for(Uint32 i = 0; i<LOGLEVEL_CATEGORIES; i++){
    logLevelData[i] = 0;
  }
}

inline
int
LogLevel::setLogLevel(EventCategory ec, Uint32 level){
  if (ec >= 0 && (Uint32) ec < LOGLEVEL_CATEGORIES)
  {
    logLevelData[ec] = (Uint8)level;
    return 0;
  }
  return 1;
}

inline
Uint32
LogLevel::getLogLevel(EventCategory ec) const{
  assert(ec >= 0 && (Uint32) ec < LOGLEVEL_CATEGORIES);

  return (Uint32)logLevelData[ec];
}

inline
LogLevel & 
LogLevel::set_max(const LogLevel & org){
  for(Uint32 i = 0; i<LOGLEVEL_CATEGORIES; i++){
    if(logLevelData[i] < org.logLevelData[i])
      logLevelData[i] = org.logLevelData[i];
  }
  return * this;
}


#undef JAM_FILE_ID

#endif
