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

#include "ClusterConfiguration.hpp"

class Configuration {
public:
  Configuration();
  ~Configuration();

  /**
   * Returns false if arguments are invalid
   */
  bool init(int argc, const char** argv);

  void setupConfiguration();
  
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
  const ClusterConfiguration::ClusterData& clusterConfigurationData() const;
  const ClusterConfiguration& clusterConfiguration() const;

  const char * programName() const;
  const char * fileSystemPath() const;
  char * getConnectStringCopy() const;

  /**
   * Return Properties for own node
   */
  const Properties * getOwnProperties() const;

  /**
   * 
   */
  bool getInitialStart() const;
  bool getDaemonMode() const;
  
private:
  Uint32 _stopOnError;
  Uint32 m_restartOnErrorInsert;
  Uint32 _maxErrorLogs;
  Uint32 _lockPagesInMainMemory;
  Uint32 _timeBetweenWatchDogCheck;


  ClusterConfiguration the_clusterConfigurationData;
  const Properties * m_ownProperties;
  
  /**
   * arguments to NDB process
   */
  char * _programName;
  char * _fsPath;
  bool _initialStart;
  char * _connectString;
  bool _daemonMode;
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
