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
    // 1 QMGR Dump information about phase 1 variables
    // 13 CMVMI Dump signal counter
    // 13 NDBCNTR Dump start phase information
    // 13 NDBCNTR_REF  Dump start phase information
    CommitAckMarkersSize = 14, // TC+LQH Dump free size in commitAckMarkerP
    CommitAckMarkersDump = 15, // TC+LQH Dump info in commitAckMarkerPool
    DihDumpNodeRestartInfo = 16, // 16 DIH Dump node restart info
    DihDumpNodeStatusInfo = 17,// 17 DIH Dump node status info
    DihPrintFragmentation = 18,// 18 DIH Print fragmentation
    // 19 NDBFS Fipple with O_SYNC, O_CREATE etc.
    // 20-24 BACKUP
    NdbcntrTestStopOnError = 25,
    // 100-105 TUP and ACC  
    // 200-240 UTIL
    // 300-305 TRIX
    NdbfsDumpFileStat = 400,
    NdbfsDumpAllFiles = 401,
    NdbfsDumpOpenFiles = 402,
    NdbfsDumpIdleFiles = 403,
    // 1222-1225 DICT
    LqhDumpAllDefinedTabs = 1332,
    LqhDumpNoLogPages = 1333,
    LqhDumpOneScanRec = 2300,
    LqhDumpAllScanRec = 2301,
    LqhDumpAllActiveScanRec = 2302,
    LqhDumpLcpState = 2303,
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
    StartTcTimer = 2509,
    StopTcTimer = 2510,
    StartPeriodicTcTimer = 2511,
    TcStartDumpIndexOpCount = 2512,
    TcDumpIndexOpCount = 2513,
    CmvmiDumpConnections = 2600,
    CmvmiDumpLongSignalMemory = 2601,
    CmvmiSetRestartOnErrorInsert = 2602,
    CmvmiTestLongSigWithDelay = 2603,
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
    EnableUndoDelayDataWrite = 7080, // DIH+ACC+TUP
    DihStartLcpImmediately = 7099,
    // 8000 Suma
    // 12000 Tux
    TuxLogToFile = 12001,
    TuxSetLogFlags = 12002,
    TuxMetaDataJunk = 12009
  };
public:
  
  Uint32 args[25];          // Generic argument
};

#endif
