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


class SubCreateReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  friend class SumaParticipant;
  
  friend bool printSUB_CREATE_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );
  
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
  
  Uint32 subscriberRef;
  Uint32 subscriberData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 subscriptionType;
  union {
    Uint32 tableId; // Used when doing SingelTableScan
  };
  SECTION( ATTRIBUTE_LIST = 0); // Used when doing SingelTableScan  
  SECTION( TABLE_LIST = 1 );  
  
};

class SubCreateRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  friend class SumaParticipant;
  
  friend bool printSUB_CREATE_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 6 );

  Uint32 subscriberRef;
  Uint32 subscriberData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 subscriptionType;
  Uint32 err;

  SECTION( ATTRIBUTE_LIST = 0); // Used when doing SingelTableScan
  union {
    Uint32 tableId; // Used when doing SingelTableScan
  };
};

class SubCreateConf {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  friend class SumaParticipant;
  
  friend bool printSUB_CREATE_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 3 );
  
  Uint32 subscriberData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
};

class SubscriptionData {
public:
  enum Part {
    MetaData = 1,
    TableData = 2
  };
};

class SubStartReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Suma;
  
  friend bool printSUB_START_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
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

class SubStartRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Suma;
  
  friend bool printSUB_START_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  enum ErrorCode {
    Undefined = 0,
    NF_FakeErrorREF = 11,
    Busy = 701,
    Temporary = 0x1 << 16
  };
  bool isTemporary() const;
  void setTemporary();
  ErrorCode setTemporary(ErrorCode ec);

  STATIC_CONST( SignalLength = 7 );
  STATIC_CONST( SignalLength2 = SignalLength+1 );
  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
  union { // do not change the order here!
    Uint32 err;
    Uint32 errorCode;
  };
  // with SignalLength2
  Uint32 subscriberRef;
};
inline bool SubStartRef::isTemporary() const
{ return (errorCode &  SubStartRef::Temporary) > 0; }
inline void SubStartRef::setTemporary()
{ errorCode |=  SubStartRef::Temporary; }
inline SubStartRef::ErrorCode SubStartRef::setTemporary(ErrorCode ec)
{ return (SubStartRef::ErrorCode) 
    (errorCode = ((Uint32) ec | (Uint32)SubStartRef::Temporary)); }

class SubStartConf {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  
  friend bool printSUB_START_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
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

class SubStopReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Suma;
  
  friend bool printSUB_STOP_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 7 );
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
  Uint32 subscriberRef;
};

class SubStopRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Suma;
  
  friend bool printSUB_STOP_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  enum ErrorCode {
    Undefined = 0,
    NF_FakeErrorREF = 11,
    Busy = 701,
    Temporary = 0x1 << 16
  };
  bool isTemporary() const;
  void setTemporary();
  ErrorCode setTemporary(ErrorCode ec);

  STATIC_CONST( SignalLength = 8 );
  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
  Uint32 subscriberRef;
  union {
    Uint32 err;
    Uint32 errorCode;
  };
};
inline bool SubStopRef::isTemporary() const
{ return (errorCode &  SubStopRef::Temporary) > 0; }
inline void SubStopRef::setTemporary()
{ errorCode |=  SubStopRef::Temporary; }
inline SubStopRef::ErrorCode SubStopRef::setTemporary(ErrorCode ec)
{ return (SubStopRef::ErrorCode) 
    (errorCode = ((Uint32) ec | (Uint32)SubStopRef::Temporary)); }

class SubStopConf {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  
  friend bool printSUB_STOP_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 7 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
  Uint32 subscriberRef;
};

class SubSyncReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Suma;
  friend class Grep;
  
  friend bool printSUB_SYNC_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );
  
public:
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 subscriberData;
  Uint32 part; // SubscriptionData::Part
};

class SubSyncRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Suma;
  friend class Grep;
  
  friend bool printSUB_SYNC_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  enum ErrorCode {
    Undefined = 0,
    Temporary = 0x1 << 16
  };
  STATIC_CONST( SignalLength = 5 );
  
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part; // SubscriptionData::Part
  Uint32 subscriberData;
  union {
    Uint32 errorCode;
    Uint32 err;
  };
};

class SubSyncConf {

  /**
   * Sender(s)/Reciver(s)
   */
  friend class Suma;
  friend class Grep;
  
  friend bool printSUB_SYNC_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );
  
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
};

class SubMetaData {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class SumaParticipant;
  friend class Grep;
  
  friend bool printSUB_META_DATA(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 3 );
  SECTION( DICT_TAB_INFO = 0 );
  
  Uint32 gci;
  union { // Haven't decide what to call it
    Uint32 senderData;
    Uint32 subscriberData;
  };
  union {
    Uint32 tableId;
  };
};

class SubTableData {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class SumaParticipant;
  friend class Grep;
  
  friend bool printSUB_TABLE_DATA(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 7 );
  
  enum LogType {
    SCAN = 1, 
    LOG  = 2,
    REMOVE_FLAGS = 0xff,
    GCINOTCONSISTENT = 0x1 << 16
  };
  
  void setGCINotConsistent() { logType |= (Uint32)GCINOTCONSISTENT; };
  bool isGCIConsistent()
  { return (logType & (Uint32)GCINOTCONSISTENT) == 0 ? true : false; };

  union { // Haven't decide what to call it
    Uint32 senderData;
    Uint32 subscriberData;
  };
  Uint32 gci;
  Uint32 tableId;
  Uint32 operation;
  Uint32 noOfAttributes;
  Uint32 dataSize; 
  Uint32 logType;
};

class SubSyncContinueReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class SumaParticipant;
  friend class Grep;
  friend class Trix;
  
  friend bool printSUB_SYNC_CONTINUE_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 2 );

  union { // Haven't decide what to call it
    Uint32 senderData;
    Uint32 subscriberData;
  };
  Uint32 noOfRowsSent;
};

class SubSyncContinueRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class SumaParticipant;
  friend class Grep;
  friend class Trix;
  
  friend bool printSUB_SYNC_CONTINUE_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 2 );

  Uint32 subscriptionId;
  Uint32 subscriptionKey;
};

class SubSyncContinueConf {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class SumaParticipant;
  friend class Grep;
  friend class Trix;
  
  friend bool printSUB_SYNC_CONTINUE_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 2 );

  Uint32 subscriptionId;
  Uint32 subscriptionKey;
};

class SubGcpCompleteRep {

  /**
   * Sender(s)/Reciver(s)
   */
  friend class Dbdih;
  friend class SumaParticipant;
  friend class Grep;
  friend class Trix;
  
  friend bool printSUB_GCP_COMPLETE_REP(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 3 );
  
  Uint32 gci;
  Uint32 senderRef;
  union { // Haven't decide what to call it
    Uint32 senderData;
    Uint32 subscriberData;
  };
};

class SubGcpCompleteAcc {
  /**
   * Sender(s)/Reciver(s)
   */
public:
  STATIC_CONST( SignalLength = SubGcpCompleteRep::SignalLength );

  SubGcpCompleteRep rep;
};

class SubRemoveReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  friend class SumaParticipant;
  
  friend bool printSUB_REMOVE_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );
  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
};

class SubRemoveRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  friend class SumaParticipant;
  
  friend bool printSUB_REMOVE_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );
  enum ErrorCode {
    Undefined = 0,
    NF_FakeErrorREF = 11,
    Busy = 701,
    Temporary = 0x1 << 16
  };
  bool isTemporary() const;
  void setTemporary();
  ErrorCode setTemporary(ErrorCode ec);

  Uint32 senderRef;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  union {
    Uint32 err;
    Uint32 errorCode;
  };
  union { // Haven't decide what to call it
    Uint32 senderData;
    Uint32 subscriberData;
  };
};
inline bool SubRemoveRef::isTemporary() const
{ return (err &  SubRemoveRef::Temporary) > 0; }
inline void SubRemoveRef::setTemporary()
{ err |=  SubRemoveRef::Temporary; }
inline SubRemoveRef::ErrorCode SubRemoveRef::setTemporary(ErrorCode ec)
{ return (SubRemoveRef::ErrorCode) 
    (errorCode = ((Uint32) ec | (Uint32)SubRemoveRef::Temporary)); }

class SubRemoveConf {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  friend class SumaParticipant;
  
  friend bool printSUB_REMOVE_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );
  
  Uint32 senderRef;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 err;
  union { // Haven't decide what to call it
    Uint32 senderData;
    Uint32 subscriberData;
  };

};


class CreateSubscriptionIdReq {
  friend class Grep;
  friend class SumaParticipant;
  
  friend bool printCREATE_SUBSCRIPTION_ID_REQ(FILE *, const Uint32 *, 
					       Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 3 );
  
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  union { // Haven't decide what to call it
    Uint32 senderData;
    Uint32 subscriberData;
  };
};


class CreateSubscriptionIdConf {
  friend class Grep;
  friend class SumaParticipant;
  
  friend bool printCREATE_SUBSCRIPTION_ID_CONF(FILE *, const Uint32 *, 
					       Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 3 );
  
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  union { // Haven't decide what to call it
    Uint32 senderData;
    Uint32 subscriberData;
  };
};


class CreateSubscriptionIdRef {
  friend class Grep;
  friend class SumaParticipant;
  
  friend bool printCREATE_SUBSCRIPTION_ID_REF(FILE *, const Uint32 *, 
					       Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );
  
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  union { // Haven't decide what to call it
    Uint32 senderData;
    Uint32 subscriberData;
  };
  Uint32 err;
};

class SumaStartMe {
public:
  STATIC_CONST( SignalLength = 1 );
  Uint32 unused;
};

class SumaHandoverReq {
public:
  STATIC_CONST( SignalLength = 1 );
  Uint32 gci;
};

class SumaHandoverConf {
public:
  STATIC_CONST( SignalLength = 1 );
  Uint32 gci;
};
#endif
