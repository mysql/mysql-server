/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ConfigParamId_H
#define ConfigParamId_H

#define JAM_FILE_ID 146


  enum ConfigParamId {

    Id,
    ExecuteOnComputer,
    MaxNoOfSavedMessages,
    ShmKey,
    
    LockPagesInMainMemory,
    TimeBetweenWatchDogCheck,
    StopOnError,
    
    MaxNoOfConcurrentOperations,
    MaxNoOfConcurrentTransactions,
    MemorySpaceIndexes,
    MemorySpaceTuples,
    MemoryDiskPages,
    NoOfFreeDiskClusters,
    NoOfDiskClusters,
    
    TimeToWaitAlive,
    HeartbeatIntervalDbDb,
    HeartbeatIntervalDbApi,
    ArbitTimeout,
    
    TimeBetweenLocalCheckpoints,
    TimeBetweenGlobalCheckpoints,
    NoOfFragmentLogFiles,
    NoOfConcurrentCheckpointsDuringRestart,
    TransactionDeadlockDetectionTimeout,
    TransactionInactiveTime,
    NoOfConcurrentProcessesHandleTakeover,
    
    NoOfConcurrentCheckpointsAfterRestart,
    
    NoOfDiskPagesToDiskDuringRestartTUP,
    NoOfDiskPagesToDiskAfterRestartTUP,
    NoOfDiskPagesToDiskDuringRestartACC,
    NoOfDiskPagesToDiskAfterRestartACC,
    
    NoOfDiskClustersPerDiskFile,
    NoOfDiskFiles,

    MaxNoOfSavedEvents
  };


#undef JAM_FILE_ID

#endif // ConfigParamId_H






