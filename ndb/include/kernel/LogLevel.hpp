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

#ifndef _LOG_LEVEL_HPP
#define _LOG_LEVEL_HPP

#include <ndb_global.h>
#include <mgmapi_config_parameters.h>

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
  
  static const Uint32 MIN_LOGLEVEL_ID = CFG_LOGLEVEL_STARTUP;
  
  enum EventCategory {
    /**
     * Events during all kind of startups
     */
    llStartUp = CFG_LOGLEVEL_STARTUP - MIN_LOGLEVEL_ID,
    
    /**
     * Events during shutdown
     */
    llShutdown = CFG_LOGLEVEL_SHUTDOWN - MIN_LOGLEVEL_ID,

    /**
     * Transaction statistics
     *   Job level
     *   TCP/IP speed
     */
    llStatistic = CFG_LOGLEVEL_STATISTICS - MIN_LOGLEVEL_ID,
    
    /**
     * Checkpoints
     */
    llCheckpoint = CFG_LOGLEVEL_CHECKPOINT - MIN_LOGLEVEL_ID,
    
    /**
     * Events during node restart
     */
    llNodeRestart = CFG_LOGLEVEL_NODERESTART - MIN_LOGLEVEL_ID,

    /**
     * Events related to connection / communication
     */
    llConnection = CFG_LOGLEVEL_CONNECTION - MIN_LOGLEVEL_ID,

    /**
     * Assorted event w.r.t unexpected happenings
     */
    llError = CFG_LOGLEVEL_ERROR - MIN_LOGLEVEL_ID,

    /**
     * Assorted event w.r.t warning
     */
    llWarning = CFG_LOGLEVEL_WARNING - MIN_LOGLEVEL_ID,

    /**
     * Assorted event w.r.t information
     */
    llInfo = CFG_LOGLEVEL_INFO - MIN_LOGLEVEL_ID,

    /**
     * Events related to global replication
     */
    llGrep = CFG_LOGLEVEL_GREP - MIN_LOGLEVEL_ID
  };

  struct LogLevelCategoryName {
    const char* name;
  };

  /**
   * Log/event level category names. Remember to update the names whenever
   * a new category is added.
   */
  static const LogLevelCategoryName LOGLEVEL_CATEGORY_NAME[];

  /**
   * No of categories
   */
#define _LOGLEVEL_CATEGORIES 10
  static const Uint32 LOGLEVEL_CATEGORIES = _LOGLEVEL_CATEGORIES;

  void clear();

  /**
   * Note level is valid as 0-15
   */
  void setLogLevel(EventCategory ec, Uint32 level = 7);
  
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
  for(Uint32 i = 0; i<LOGLEVEL_CATEGORIES; i++){
    logLevelData[i] = org.logLevelData[i];
  }
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
void
LogLevel::setLogLevel(EventCategory ec, Uint32 level){
  assert(ec >= 0 && (Uint32) ec < LOGLEVEL_CATEGORIES);
  logLevelData[ec] = level;
}

inline
Uint32
LogLevel::getLogLevel(EventCategory ec) const{
  assert(ec >= 0 && (Uint32) ec < LOGLEVEL_CATEGORIES);

  return logLevelData[ec];
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

#endif
