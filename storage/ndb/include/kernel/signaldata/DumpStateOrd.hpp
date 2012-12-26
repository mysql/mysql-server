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

#ifndef DUMP_STATE_ORD_HPP
#define DUMP_STATE_ORD_HPP

#include "SignalData.hpp"

/**
 * DumpStateOrd is sent by the mgmtsrvr to CMVMI.
 * CMVMI the redirect the signal to all blocks.
 *
 * The implementation of the DumpStateOrd should dump state information
 * (typically using the infoEvent-function)
 */
class DumpStateOrd {
  /**
   * Sender/Reciver
   */
  friend class Cmvmi;

  /**
   * Sender(s)
   */
  friend class MgmtSrvr;
  
  /**
   * Reciver(s)
   */
  friend class Dbacc;
  friend class Dblqh;
  friend class Dbtup;
  friend class Dbtc;
  friend class Ndbcntr;
  friend class Qmgr;
  friend class Dbdih;
  friend class Dbdict;
  friend class Ndbfs;

public:
  enum DumpStateType {
    /* any dumps above this value should go to one block only */
    OneBlockOnly = 100000,

    _BackupMin   = 100000,
    BackupStatus = 100000,
    _BackupMax   = 100999,

    _TCMin       = 101000,
    _TCMax       = 101999,

    _LQHMin = 102000,
    LQHLogFileInitStatus = 102000,
    _LQHMax = 102999,

    // 1 QMGR Dump information about phase 1 variables
    // 13 CMVMI Dump signal counter
    // 13 NDBCNTR Dump start phase information
    // 13 NDBCNTR_REF  Dump start phase information
    CommitAckMarkersSize = 14, // TC+LQH Dump free size in commitAckMarkerP
    CommitAckMarkersDump = 15, // TC+LQH Dump info in commitAckMarkerPool
    DihDumpNodeRestartInfo = 16, // 16 DIH Dump node restart info
    DihDumpNodeStatusInfo = 17,// 17 DIH Dump node status info
    DihPrintFragmentation = 18,// 18 DIH Print fragmentation
    DihPrintOneFragmentation = 19,// 18 DIH Print info about one fragmentation
    // 19 NDBFS Fipple with O_SYNC, O_CREATE etc.
    // 20-24 BACKUP
    NdbcntrTestStopOnError = 25,
    NdbcntrStopNodes = 70,
    // 100-105 TUP and ACC  
    // 200-240 UTIL
    // 300-305 TRIX
    QmgrErr935 = 935,
    NdbfsDumpFileStat = 400,
    NdbfsDumpAllFiles = 401,
    NdbfsDumpOpenFiles = 402,
    NdbfsDumpIdleFiles = 403,
    CmvmiSchedulerExecutionTimer = 502,
    CmvmiRealtimeScheduler = 503,
    CmvmiExecuteLockCPU = 504,
    CmvmiMaintLockCPU = 505,
    CmvmiSchedulerSpinTimer = 506,
    // 1222-1225 DICT
    DictDumpLockQueue = 1228,
    LqhDumpAllDefinedTabs = 1332,
    LqhDumpNoLogPages = 1333,
    LqhDumpOneScanRec = 2300,
    LqhDumpAllScanRec = 2301,
    LqhDumpAllActiveScanRec = 2302,
    LqhDumpLcpState = 2303,
    LqhErrorInsert5042 = 2315,
    LqhDumpPoolLevels = 2353,

    AccDumpOneScanRec = 2400,
    AccDumpAllScanRec = 2401,
    AccDumpAllActiveScanRec = 2402,
    AccDumpOneOperationRec = 2403,
    AccDumpNumOpRecs = 2404,
    AccDumpFreeOpRecs = 2405,
    AccDumpNotFreeOpRecs = 2406,
    DumpPageMemory = 1000, // Acc & TUP
    TcDumpAllScanFragRec = 2500,
    TcDumpOneScanFragRec = 2501,
    TcDumpAllScanRec = 2502,
    TcDumpAllActiveScanRec = 2503,
    TcDumpOneScanRec = 2504,
    TcDumpOneApiConnectRec = 2505,
    TcDumpAllApiConnectRec = 2506,
    TcSetTransactionTimeout = 2507,
    TcSetApplTransactionTimeout = 2508,
    TcStartDumpIndexOpCount = 2512,
    TcDumpIndexOpCount = 2513,
    TcDumpPoolLevels = 2555,
    CmvmiDumpConnections = 2600,
    CmvmiDumpLongSignalMemory = 2601,
    CmvmiSetRestartOnErrorInsert = 2602,
    CmvmiTestLongSigWithDelay = 2603,
    CmvmiDumpSubscriptions = 2604, /* note: done to respective outfile
                                      to be able to debug if events
                                      for some reason does not end up
                                      in clusterlog */
    CmvmiTestLongSig = 2605,  /* Long signal testing trigger */
    DumpEventLog = 2606,
    LCPContinue = 5900,
    // 7000 DIH
    // 7001 DIH
    // 7002 DIH
    // 7003 DIH
    // 7004 DIH
    // 7005 DIH
    // 7006 DIH
    // 7006 DIH
    // 7007 DIH
    // 7008 DIH
    // 7009 DIH
    // 7010 DIH
    // 7011 DIH
    // 7012 DIH
    DihDumpLCPState= 7013,
    DihDumpLCPMasterTakeOver = 7014,    
    // 7015 DIH
    DihAllAllowNodeStart = 7016,
    DihMinTimeBetweenLCP = 7017,
    DihMaxTimeBetweenLCP = 7018,
    // Checks whether add frag failure was cleaned up.
    // Should NOT be used while commands involving addFragReq
    // are being performed
    DihAddFragFailCleanedUp = 7019,
    // 7020
    // 7021
    DihDumpPageRecInfo = 7032,
    EnableUndoDelayDataWrite = 7080, // DIH+ACC+TUP
    DihSetTimeBetweenGcp = 7090,
    DihStartLcpImmediately = 7099,
    // 8000 Suma
    // 12000 Tux
    TuxLogToFile = 12001,
    TuxSetLogFlags = 12002,
    TuxMetaDataJunk = 12009,
    
    DumpTsman = 9800, 
    DumpLgman = 10000,
    DumpPgman = 11000,
    DumpBackup = 13000,
    DumpBackupSetCompressed = 13001,
    DumpBackupSetCompressedLCP = 13002,
    BackupErrorInsert = 13003,

    DumpDbinfo = 14000,
    DbinfoListTables = 14001,
    DbinfoListColumns = 14002,
    DbinfoScanTable = 14003,

    SchemaResourceSnapshot = 4000, // Save resource consumption
    SchemaResourceCheckLeak = 4001, // check same as snapshot

    TcResourceSnapshot = 2553,
    TcResourceCheckLeak = 2554
  };
public:
  
  Uint32 args[25];          // Generic argument
};

#endif
