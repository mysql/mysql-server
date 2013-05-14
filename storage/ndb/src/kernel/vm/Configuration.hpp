/* Copyright (c) 2003-2007 MySQL AB


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */


#ifndef Configuration_H
#define Configuration_H

#include <util/BaseString.hpp>
#include <mgmapi.h>
#include <ndb_types.h>
#include <NdbMutex.h>
#include <NdbThread.h>

enum ThreadTypes
{
  WatchDogThread = 1,
  SocketServerThread = 2,
  SocketClientThread = 3,
  NdbfsThread = 4,
  MainThread = 5,
  NotInUse = 6
};

#define MAX_NDB_THREADS 256
#define NO_LOCK_CPU 65535

struct ThreadInfo
{
  NDB_TID_TYPE threadId;
  NDB_THAND_TYPE threadHandle;
  enum ThreadTypes type;
};

class Configuration;

struct ThreadContainer
{
  Configuration *conf;
  enum ThreadTypes type;
  Uint32 index;
};

class ConfigRetriever;

class Configuration {
public:
  Configuration();
  ~Configuration();

  /**
   * Returns false if arguments are invalid
   */
  bool init(int argc, char** argv);

  void fetch_configuration();
  void setupConfiguration();
  void closeConfiguration(bool end_session= true);
  
  Uint32 lockPagesInMainMemory() const;

  int schedulerExecutionTimer() const;
  void schedulerExecutionTimer(int value);

  int schedulerSpinTimer() const;
  void schedulerSpinTimer(int value);

  bool realtimeScheduler() const;
  void realtimeScheduler(bool realtime_on);

  Uint32 executeLockCPU() const;
  void executeLockCPU(Uint32 value);

  Uint32 maintLockCPU() const;
  void maintLockCPU(Uint32 value);

  void setAllRealtimeScheduler();
  void setAllLockCPU(bool exec_thread);
  int setLockCPU(NDB_TID_TYPE threadId,
                 enum ThreadTypes type,
                 bool exec_thread,
                 bool init);
  int setRealtimeScheduler(NDB_THAND_TYPE threadHandle,
                           enum ThreadTypes type,
                           bool real_time,
                           bool init);
  Uint32 addThreadId(enum ThreadTypes type);
  void removeThreadId(Uint32 index);
  void yield_main(Uint32 thread_index, bool start);
  void initThreadArray();

  int timeBetweenWatchDogCheck() const ;
  void timeBetweenWatchDogCheck(int value);
  
  int maxNoOfErrorLogs() const ;
  void maxNoOfErrorLogs(int val);

  bool stopOnError() const;
  void stopOnError(bool val);
  
  int getRestartOnErrorInsert() const;
  void setRestartOnErrorInsert(int);

  Uint32 getMaxStartFailRetries() const;
  
  Uint32 getStartFailRetryDelaySecs() const;
  
  // Cluster configuration
  const char * programName() const;
  const char * fileSystemPath() const;
  const char * backupFilePath() const;
  const char * getConnectString() const;
  char * getConnectStringCopy() const;

  /**
   * 
   */
  bool getInitialStart() const;
  void setInitialStart(bool val);
  bool getDaemonMode() const;
  bool getForegroundMode() const;

  const ndb_mgm_configuration_iterator * getOwnConfigIterator() const;

  Uint32 get_mgmd_port() const {return m_mgmd_port;};
  const char *get_mgmd_host() const {return m_mgmd_host.c_str();};
  ConfigRetriever* get_config_retriever() { return m_config_retriever; };

  class LogLevel * m_logLevel;
private:
  friend class Cmvmi;
  friend class Qmgr;
  friend class Dbdih;
  friend int reportShutdown(class Configuration *config, int error, int restart);

  ndb_mgm_configuration_iterator * getClusterConfigIterator() const;

  Uint32 _stopOnError;
  Uint32 m_restartOnErrorInsert;
  Uint32 _maxErrorLogs;
  Uint32 _lockPagesInMainMemory;
  Uint32 _timeBetweenWatchDogCheck;
  Uint32 _schedulerExecutionTimer;
  Uint32 _schedulerSpinTimer;
  Uint32 _realtimeScheduler;
  Uint32 _executeLockCPU;
  Uint32 _maintLockCPU;
  Uint32 _timeBetweenWatchDogCheckInitial;
  Uint32 _maxStartFailRetries;
  Uint32 _startFailDelaySecs;

  Vector<struct ThreadInfo> threadInfo;
  NdbMutex *threadIdMutex;

  ndb_mgm_configuration * m_ownConfig;
  ndb_mgm_configuration * m_clusterConfig;

  ndb_mgm_configuration_iterator * m_clusterConfigIter;
  ndb_mgm_configuration_iterator * m_ownConfigIterator;
  
  ConfigRetriever *m_config_retriever;

  Vector<BaseString> m_mgmds;

  /**
   * arguments to NDB process
   */
  char * _programName;
  char * _fsPath;
  char * _backupPath;
  bool _initialStart;
  char * _connectString;
  Uint32 m_mgmd_port;
  BaseString m_mgmd_host;
  bool _daemonMode; // if not, angel in foreground
  bool _foregroundMode; // no angel, raw ndbd in foreground

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
const char *
Configuration::backupFilePath() const {
  return _backupPath;
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

inline
bool
Configuration::getForegroundMode() const {
  return _foregroundMode;
}

#endif
