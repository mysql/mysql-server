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

#ifndef REP_IMPL_HPP
#define REP_IMPL_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>
#include <ndb_limits.h>
#include <debugger/GrepError.hpp>

/**
 * RecordType
 * sz = no of elems in enum
 * @todo support for meta_log must be added
 */
enum RecordType 
{
  DATA_SCAN = 0, 
  DATA_LOG = 1, 
  META_SCAN = 2, 
  //  META_LOG = 3,  //removed META_LOG. not supported
  RecordTypeSize = 3 // =4 if meta log is supported
};

/**
 * Wait GCP
 */
class RepWaitGcpReq 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  friend class GrepParticipant;
  friend bool printREP_WAITGCP_REQ(FILE *, const Uint32 *, Uint32, Uint16);

public:
  STATIC_CONST( SignalLength = 5 );
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 gcp;
  Uint32 senderNodeId;
};

class RepWaitGcpConf 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  friend class GrepParticipant;
  
  friend bool printREP_WAITGCP_CONF(FILE *, const Uint32 *, Uint32, Uint16);

public:
  STATIC_CONST( SignalLength = 5 );
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 senderNodeId;
};

class RepWaitGcpRef 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  friend class GrepParticipant;

  friend bool printREP_WAITGCP_REF(FILE *, const Uint32 *, Uint32, Uint16);

public:
  STATIC_CONST( SignalLength = 6 );
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 senderNodeId;
  GrepError::GE_Code err;
};

class RepGetGciReq 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  friend class Grep;
  
  friend bool printREP_GET_GCI_REQ(FILE *, const Uint32 *, Uint32, Uint16);
				   
public:
  STATIC_CONST( SignalLength = 3 );
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 nodeGrp;
};

class RepGetGciConf 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_GET_GCI_CONF(FILE *, const Uint32 *, Uint32, Uint16);
				   
public:
  STATIC_CONST( SignalLength = 7 );
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 nodeGrp;
  Uint32 firstPSGCI;
  Uint32 lastPSGCI;
  Uint32 firstSSGCI;
  Uint32 lastSSGCI;
};

class RepGetGciRef 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_GET_GCI_REF(FILE *, const Uint32 *, Uint32, Uint16);
				   
public:
  STATIC_CONST( SignalLength = 8);
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 nodeGrp;
  Uint32 firstPSGCI;
  Uint32 lastPSGCI;
  Uint32 firstSSGCI;
  Uint32 lastSSGCI;
  GrepError::GE_Code err;
};

class RepGetGciBufferReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;

  friend bool printREP_GET_GCIBUFFER_REQ(FILE *, const Uint32 *, 
					 Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );
  Uint32 senderRef;
  Uint32 senderData;  
  Uint32 firstGCI;
  Uint32 lastGCI;
  Uint32 nodeGrp;
};


class RepGetGciBufferConf {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_GET_GCIBUFFER_CONF(FILE *, const Uint32 *, 
					  Uint32, Uint16);

public:
  STATIC_CONST( SignalLength = 8 );
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 firstPSGCI;
  Uint32 lastPSGCI;
  Uint32 firstSSGCI;
  Uint32 lastSSGCI;
  Uint32 currentGCIBuffer;
  Uint32 nodeGrp;
};

class RepGetGciBufferRef 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_GET_GCIBUFFER_REF(FILE *, const Uint32 *, 
					 Uint32, Uint16);
					 
public:
  STATIC_CONST( SignalLength = 9 );
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 firstPSGCI;
  Uint32 lastPSGCI;
  Uint32 firstSSGCI;
  Uint32 lastSSGCI;
  Uint32 currentGCIBuffer;
  Uint32 nodeGrp;
  GrepError::GE_Code err;
};

class RepInsertGciBufferReq 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_INSERT_GCIBUFFER_REQ(FILE *, const Uint32 *, 
					    Uint32, Uint16);
					    
public:
  STATIC_CONST( SignalLength = 5 );
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 gci;
  Uint32 nodeGrp;
  Uint32 force;
};

class RepInsertGciBufferRef 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_INSERT_GCIBUFFER_REF(FILE *, const Uint32 *, 
					    Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 7 );
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 gci;
  Uint32 nodeGrp;
  Uint32 tableId;
  Uint32 force;
  GrepError::GE_Code err;
};

class RepInsertGciBufferConf 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_INSERT_GCIBUFFER_CONF(FILE *, const Uint32 *, 
					     Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 gci;
  Uint32 nodeGrp;
  Uint32 force;
};


class RepClearPSGciBufferReq 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_CLEAR_PS_GCIBUFFER_REQ(FILE *, const Uint32 *, 
					      Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 firstGCI;
  Uint32 lastGCI;
  Uint32 nodeGrp;
};

class RepClearPSGciBufferRef 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_CLEAR_PS_GCIBUFFER_REF(FILE *, const Uint32 *, 
					      Uint32, Uint16);
  
public:
  STATIC_CONST( SignalLength = 7 );  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 firstGCI;
  Uint32 lastGCI;
  Uint32 currentGCI;
  Uint32 nodeGrp;
  GrepError::GE_Code err;
};

class RepClearPSGciBufferConf 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_CLEAR_PS_GCIBUFFER_CONF(FILE *, const Uint32 *, 
					       Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 firstGCI;
  Uint32 lastGCI;
  Uint32 nodeGrp;
};

class RepClearSSGciBufferReq 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_CLEAR_SS_GCIBUFFER_REQ(FILE *, const Uint32 *, 
					      Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 firstGCI;
  Uint32 lastGCI;
  Uint32 nodeGrp;
};

class RepClearSSGciBufferRef 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_CLEAR_SS_GCIBUFFER_REF(FILE *, const Uint32 *, 
					      Uint32, Uint16);
					      
public:
  STATIC_CONST( SignalLength = 7 );  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 firstGCI;
  Uint32 lastGCI;
  Uint32 currentGCI;
  Uint32 nodeGrp;
  GrepError::GE_Code err;
};

class RepClearSSGciBufferConf 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_CLEAR_SS_GCIBUFFER_CONF(FILE *, const Uint32 *, 
					       Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );  
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 firstGCI;
  Uint32 lastGCI;
  Uint32 nodeGrp;
};


class RepDataPage 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_DATA_PAGE(FILE *, const Uint32 *, Uint32, Uint16);
				 
public:
  STATIC_CONST( SignalLength = 4 );
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 nodeGrp;
  Uint32 gci;  
};


class RepGciBufferAccRep 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_GCIBUFFER_ACC_REP(FILE *, const Uint32 *, 
					 Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 nodeGrp;
  Uint32 gci;  
  Uint32 totalSentBytes;  
};

class RepDropTableReq 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_DROP_TABLE_REQ(FILE *, const Uint32 *, 
				      Uint32, Uint16);
  
public:
  STATIC_CONST( SignalLength = 4 );
  Uint32 tableId;
  //  char   tableName[MAX_TAB_NAME_SIZE]; 
};

class RepDropTableRef 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_DROP_TABLE_REF(FILE *, const Uint32 *, 
				      Uint32, Uint16);
				      
public:
  STATIC_CONST( SignalLength = 4 );
  Uint32 tableId;
  // char   tableName[MAX_TAB_NAME_SIZE]; 
};

class RepDropTableConf 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  
  friend bool printREP_DROP_TABLE_CONF(FILE *, const Uint32 *, Uint32, Uint16);
				      
public:
  STATIC_CONST( SignalLength = 4 );
  Uint32 tableId;
  //char   tableName[MAX_TAB_NAME_SIZE]; 
};

class RepDisconnectRep 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Rep;
  friend class Grep;
  
  friend bool printREP_DISCONNECT_REP(FILE *, const Uint32 *, Uint32, Uint16);
				      
public:
  enum NodeType {
    DB = 0,
    REP = 1
  };
  STATIC_CONST( SignalLength = 7 );
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 nodeId;
  Uint32 nodeType;
  Uint32 subId;
  Uint32 subKey;
  Uint32 err;
};

#endif
