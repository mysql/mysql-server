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

#ifndef SUMA_IMPL_HPP
#define SUMA_IMPL_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>


struct SubCreateReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct Grep;
  friend struct SumaParticipant;
  
  friend bool printSUB_CREATE_REQ(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 6 );
  
  enum SubscriptionType {
    SingleTableScan  = 1,  // 
    DatabaseSnapshot = 2, // All tables/all data (including new ones)
    TableEvent  = 3,       //
    SelectiveTableSnapshot  = 4,  // User defines tables
    RemoveFlags  = 0xff,
    GetFlags     = 0xff << 16,
    AddTableFlag = 0x1 << 16,
    RestartFlag  = 0x2 << 16
  };
  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 subscriptionType;
  Uint32 tableId;
};

struct SubCreateRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct Grep;
  friend struct SumaParticipant;
  
  friend bool printSUB_CREATE_REF(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 3 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
};

struct SubCreateConf {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct Grep;
  friend struct SumaParticipant;
  
  friend bool printSUB_CREATE_CONF(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 2 );
  
  Uint32 senderRef;
  Uint32 senderData;
};

struct SubscriptionData {
  enum Part {
    MetaData = 1,
    TableData = 2
  };
};

struct SubStartReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct Suma;
  
  friend bool printSUB_START_REQ(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 6 );
  STATIC_CONST( SignalLength2 = SignalLength+1 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
  Uint32 subscriberRef;
};

struct SubStartRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct Suma;
  
  friend bool printSUB_START_REF(FILE *, const Uint32 *, Uint32, Uint16);
  enum ErrorCode {
    Undefined = 1,
    NF_FakeErrorREF = 11,
    Busy = 701
  };

  STATIC_CONST( SignalLength = 7 );
  STATIC_CONST( SignalLength2 = SignalLength+1 );
  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
  // do not change the order here!
  Uint32 errorCode;
  // with SignalLength2
  Uint32 subscriberRef;
};

struct SubStartConf {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct Grep;
  
  friend bool printSUB_START_CONF(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 7 );
  STATIC_CONST( SignalLength2 = SignalLength+1 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 firstGCI;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
  // with SignalLength2
  Uint32 subscriberRef;
};

struct SubStopReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct Suma;
  
  friend bool printSUB_STOP_REQ(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 7 );
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
  Uint32 subscriberRef;
};

struct SubStopRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct Suma;
  
  friend bool printSUB_STOP_REF(FILE *, const Uint32 *, Uint32, Uint16);
  enum ErrorCode {
    Undefined = 1,
    NF_FakeErrorREF = 11,
    Busy = 701
  };

  STATIC_CONST( SignalLength = 8 );
  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
  Uint32 subscriberRef;
  Uint32 errorCode;
};

struct SubStopConf {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct Grep;
  
  friend bool printSUB_STOP_CONF(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 7 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
  Uint32 subscriberRef;
};

struct SubSyncReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct Suma;
  friend struct Grep;
  
  friend bool printSUB_SYNC_REQ(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 5 );
  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part; // SubscriptionData::Part

  SECTION( ATTRIBUTE_LIST = 0); // Used when doing SingelTableScan  
  SECTION( TABLE_LIST = 1 );
};

struct SubSyncRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct Suma;
  friend struct Grep;
  
  friend bool printSUB_SYNC_REF(FILE *, const Uint32 *, Uint32, Uint16);
  enum ErrorCode {
    Undefined = 1
  };
  STATIC_CONST( SignalLength = 3 );
  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
};

struct SubSyncConf {

  /**
   * Sender(s)/Reciver(s)
   */
  friend struct Suma;
  friend struct Grep;
  
  friend bool printSUB_SYNC_CONF(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 2 );
  
  Uint32 senderRef;
  Uint32 senderData;
};

struct SubMetaData {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct SumaParticipant;
  friend struct Grep;
  
  friend bool printSUB_META_DATA(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 3 );
  SECTION( DICT_TAB_INFO = 0 );
  
  Uint32 gci;
  Uint32 senderData;
  Uint32 tableId;
};

struct SubTableData {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct SumaParticipant;
  friend struct Grep;
  
  friend bool printSUB_TABLE_DATA(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 5 );
  
  enum LogType {
    SCAN = 1, 
    LOG  = 2,
    REMOVE_FLAGS = 0xff
  };
  
  Uint32 senderData;
  Uint32 gci;
  Uint32 tableId;
  Uint8  operation;
  Uint8  req_nodeid;
  Uint8  not_used2;
  Uint8  not_used3;
  Uint32 logType;
};

struct SubSyncContinueReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct SumaParticipant;
  friend struct Grep;
  friend struct Trix;
  
  friend bool printSUB_SYNC_CONTINUE_REQ(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 2 );

  Uint32 subscriberData;
  Uint32 noOfRowsSent;
};

struct SubSyncContinueRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct SumaParticipant;
  friend struct Grep;
  friend struct Trix;
  
  friend bool printSUB_SYNC_CONTINUE_REF(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 2 );

  Uint32 subscriptionId;
  Uint32 subscriptionKey;
};

struct SubSyncContinueConf {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct SumaParticipant;
  friend struct Grep;
  friend struct Trix;
  
  friend bool printSUB_SYNC_CONTINUE_CONF(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 2 );

  Uint32 subscriptionId;
  Uint32 subscriptionKey;
};

struct SubGcpCompleteRep {

  /**
   * Sender(s)/Reciver(s)
   */
  friend struct Dbdih;
  friend struct SumaParticipant;
  friend struct Grep;
  friend struct Trix;
  
  friend bool printSUB_GCP_COMPLETE_REP(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 3 );
  
  Uint32 gci;
  Uint32 senderRef;
  Uint32 gcp_complete_rep_count;
};

struct SubGcpCompleteAck {
  /**
   * Sender(s)/Reciver(s)
   */
  STATIC_CONST( SignalLength = SubGcpCompleteRep::SignalLength );

  SubGcpCompleteRep rep;
};

struct SubRemoveReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct Grep;
  friend struct SumaParticipant;
  
  friend bool printSUB_REMOVE_REQ(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 4 );
  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
};

struct SubRemoveRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct Grep;
  friend struct SumaParticipant;
  
  friend bool printSUB_REMOVE_REF(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 5 );
  enum ErrorCode {
    Undefined = 1,
    NF_FakeErrorREF = 11,
    Busy = 701
  };

  Uint32 senderRef;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 errorCode;
  Uint32 senderData;
};

struct SubRemoveConf {
  /**
   * Sender(s)/Reciver(s)
   */
  friend struct Grep;
  friend struct SumaParticipant;
  
  friend bool printSUB_REMOVE_CONF(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 5 );
  
  Uint32 senderRef;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 errorCode;
  Uint32 senderData;
};


struct CreateSubscriptionIdReq {
  friend struct Grep;
  friend struct SumaParticipant;
  
  friend bool printCREATE_SUBSCRIPTION_ID_REQ(FILE *, const Uint32 *, 
					       Uint32, Uint16);
  STATIC_CONST( SignalLength = 2 );
  
  Uint32 senderRef;
  Uint32 senderData;
};


struct CreateSubscriptionIdConf {
  friend struct Grep;
  friend struct SumaParticipant;
  
  friend bool printCREATE_SUBSCRIPTION_ID_CONF(FILE *, const Uint32 *, 
					       Uint32, Uint16);
  STATIC_CONST( SignalLength = 4 );
  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
};


struct CreateSubscriptionIdRef {
  friend struct Grep;
  friend struct SumaParticipant;
  
  friend bool printCREATE_SUBSCRIPTION_ID_REF(FILE *, const Uint32 *, 
					       Uint32, Uint16);
  STATIC_CONST( SignalLength = 3 );
  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
};

struct SumaStartMeReq {
  STATIC_CONST( SignalLength = 1 );
  Uint32 unused;
};

struct SumaStartMeRef {
  STATIC_CONST( SignalLength = 1 );
  Uint32 errorCode;
  enum {
    Busy = 0x1
  };
};

struct SumaStartMeConf {
  STATIC_CONST( SignalLength = 1 );
  Uint32 unused;
};

struct SumaHandoverReq {
  STATIC_CONST( SignalLength = 3 );
  Uint32 gci;
  Uint32 nodeId;
  Uint32 theBucketMask[1];
};

struct SumaHandoverConf {
  STATIC_CONST( SignalLength = 3 );
  Uint32 gci;
  Uint32 nodeId;
  Uint32 theBucketMask[1];
};

struct SumaContinueB
{
  enum 
  {
    RESEND_BUCKET = 1
    ,RELEASE_GCI = 2
    ,OUT_OF_BUFFER_RELEASE = 3
  };
};

#endif
