#ifndef CNTR_START_HPP
#define CNTR_START_HPP

#include <NodeBitmask.hpp>

/**
 * 
 */
class CntrStartReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;
  
  friend bool printCNTR_START_REQ(FILE*, const Uint32 *, Uint32, Uint16);
  
public:
  STATIC_CONST( SignalLength = 3 );
private:
  
  Uint32 nodeId;
  Uint32 startType;
  Uint32 lastGci;
};

class CntrStartRef {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;
  
  friend bool printCNTR_START_REF(FILE*, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 2 );

  enum ErrorCode {
    OK = 0,
    NotMaster = 1,
    StopInProgress = 2
  };
private:
  
  Uint32 errorCode;
  Uint32 masterNodeId;
};

class CntrStartConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Ndbcntr;
  friend struct UpgradeStartup;
  
  friend bool printCNTR_START_CONF(FILE*, const Uint32 *, Uint32, Uint16);

public:
  STATIC_CONST( SignalLength = 4 + 2 * NdbNodeBitmask::Size );
  
private:
  
  Uint32 startType;
  Uint32 startGci;
  Uint32 masterNodeId;
  Uint32 noStartNodes;
  Uint32 startedNodes[NdbNodeBitmask::Size];
  Uint32 startingNodes[NdbNodeBitmask::Size];
};

#endif
