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
