/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef Configuration_H
#define Configuration_H

#include <ndb_global.h>

#include <util/BaseString.hpp>
#include <mgmapi.h>
#include <kernel_types.h>
#include <NdbMutex.h>
#include <NdbThread.h>
#include <util/SparseBitmask.hpp>
#include <util/UtilBuffer.hpp>
#include "mt_thr_config.hpp"

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
#define NO_LOCK_CPU 0x10000

struct ThreadInfo
{
  enum ThreadTypes type;
  struct NdbThread* pThread;
};

class ConfigRetriever;

class Configuration {
public:
  Configuration();
  ~Configuration();

  bool init(int _no_start, int _initial,
            int _initialstart);


  void fetch_configuration(const char* _connect_string, int force_nodeid,
                           const char* _bind_adress,
                           NodeId allocated_nodeid);
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
  int setLockCPU(NdbThread*, enum ThreadTypes type);
  int setRealtimeScheduler(NdbThread*,
                           enum ThreadTypes type,
                           bool real_time,
                           bool init);
  Uint32 addThread(struct NdbThread*, enum ThreadTypes type);
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
  
  // Cluster configuration
  const char * fileSystemPath() const;
  const char * backupFilePath() const;

  bool getInitialStart() const { return _initialStart; }

  const ndb_mgm_configuration_iterator * getOwnConfigIterator() const;

  ConfigRetriever* get_config_retriever() { return m_config_retriever; };

  class LogLevel * m_logLevel;
  ndb_mgm_configuration_iterator * getClusterConfigIterator() const;

  ndb_mgm_configuration* getClusterConfig() const { return m_clusterConfig; }
  Uint32 get_config_generation() const; 

  THRConfigApplier m_thr_config;
private:
  friend class Cmvmi;
  friend class Qmgr;

  Uint32 _stopOnError;
  Uint32 m_restartOnErrorInsert;
  Uint32 _maxErrorLogs;
  Uint32 _lockPagesInMainMemory;
  Uint32 _timeBetweenWatchDogCheck;
  Uint32 _schedulerExecutionTimer;
  Uint32 _schedulerSpinTimer;
  Uint32 _realtimeScheduler;
  Uint32 _timeBetweenWatchDogCheckInitial;

  Vector<struct ThreadInfo> threadInfo;
  NdbMutex *threadIdMutex;

  ndb_mgm_configuration * m_ownConfig;
  ndb_mgm_configuration * m_clusterConfig;
  UtilBuffer m_clusterConfigPacked;

  ndb_mgm_configuration_iterator * m_clusterConfigIter;
  ndb_mgm_configuration_iterator * m_ownConfigIterator;
  
  ConfigRetriever *m_config_retriever;

  /**
   * arguments to NDB process
   */
  char * _fsPath;
  char * _backupPath;
  bool _initialStart;

  void calcSizeAlt(class ConfigValues * );
};

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

#endif
