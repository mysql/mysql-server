/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef NDB_UPGRADE_STARTUP
#define NDB_UPGRADE_STARTUP

class Ndbcntr;

struct UpgradeStartup {

  static void installEXEC(SimulatedBlock*);

  STATIC_CONST( GSN_CM_APPCHG = 131 );
  STATIC_CONST( GSN_CNTR_MASTERCONF = 148 );
  STATIC_CONST( GSN_CNTR_MASTERREF = 149 );
  STATIC_CONST( GSN_CNTR_MASTERREQ = 150 );

  static void sendCmAppChg(Ndbcntr&, Signal *, Uint32 startLevel);
  static void execCM_APPCHG(SimulatedBlock& block, Signal*);
  static void sendCntrMasterReq(Ndbcntr& cntr, Signal* signal, Uint32 n);
  static void execCNTR_MASTER_REPLY(SimulatedBlock & block, Signal* signal);
  
  struct CntrMasterReq {
    STATIC_CONST( SignalLength = 4 + NdbNodeBitmask::Size );
    
    Uint32 userBlockRef;
    Uint32 userNodeId;
    Uint32 typeOfStart;
    Uint32 noRestartNodes;
    Uint32 theNodes[NdbNodeBitmask::Size];
  };

  struct CntrMasterConf {
    STATIC_CONST( SignalLength = 1 + NdbNodeBitmask::Size );
    
    Uint32 noStartNodes;
    Uint32 theNodes[NdbNodeBitmask::Size];
  };
};

#endif
