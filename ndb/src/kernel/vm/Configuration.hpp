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

#ifndef Configuration_H
#define Configuration_H

#include <mgmapi.h>
#include <ndb_types.h>

class ConfigRetriever;

class Configuration {
public:
  Configuration();
  ~Configuration();

  /**
   * Returns false if arguments are invalid
   */
  bool init(int argc, const char** argv);

  void fetch_configuration();
  void setupConfiguration();
  void closeConfiguration();
  
  bool lockPagesInMainMemory() const;
  
  int timeBetweenWatchDogCheck() const ;
  void timeBetweenWatchDogCheck(int value);
  
  int maxNoOfErrorLogs() const ;
  void maxNoOfErrorLogs(int val);

  bool stopOnError() const;
  void stopOnError(bool val);
  
  int getRestartOnErrorInsert() const;
  void setRestartOnErrorInsert(int);
  
  // Cluster configuration
  const char * programName() const;
  const char * fileSystemPath() const;
  char * getConnectStringCopy() const;

  /**
   * 
   */
  bool getInitialStart() const;
  void setInitialStart(bool val);
  bool getDaemonMode() const;

  const ndb_mgm_configuration_iterator * getOwnConfigIterator() const;

  class LogLevel * m_logLevel;
private:
  friend class Cmvmi;
  friend class Qmgr;
  ndb_mgm_configuration_iterator * getClusterConfigIterator() const;

  Uint32 _stopOnError;
  Uint32 m_restartOnErrorInsert;
  Uint32 _maxErrorLogs;
  Uint32 _lockPagesInMainMemory;
  Uint32 _timeBetweenWatchDogCheck;

  ndb_mgm_configuration * m_ownConfig;
  ndb_mgm_configuration * m_clusterConfig;

  ndb_mgm_configuration_iterator * m_clusterConfigIter;
  ndb_mgm_configuration_iterator * m_ownConfigIterator;
  
  ConfigRetriever *m_config_retriever;

  /**
   * arguments to NDB process
   */
  char * _programName;
  char * _fsPath;
  bool _initialStart;
  char * _connectString;
  bool _daemonMode;

  void calcSizeAlt(class ConfigValues * );
};

inline
const char *
Configuration::programName() const {
  return _programName;
}

inline
const char *
Configuration::fileSystemPath() const {
  return _fsPath;
}

inline
bool
Configuration::getInitialStart() const {
  return _initialStart;
}

inline
bool
Configuration::getDaemonMode() const {
  return _daemonMode;
}

#endif
