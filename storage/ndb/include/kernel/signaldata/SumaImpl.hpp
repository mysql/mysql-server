/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef SUMA_IMPL_HPP
#define SUMA_IMPL_HPP

#include <NodeBitmask.hpp>
#include "SignalData.hpp"

#define JAM_FILE_ID 79

struct SubCreateReq {
  friend bool printSUB_CREATE_REQ(FILE *, const Uint32 *, Uint32, Uint16);
  static constexpr Uint32 SignalLength = 7;

  enum SubscriptionType {
    SingleTableScan = 1,         //
    DatabaseSnapshot = 2,        // All tables/all data (including new ones)
    TableEvent = 3,              //
    SelectiveTableSnapshot = 4,  // User defines tables
    RemoveFlags = 0xff,
    GetFlags = 0xff << 16,
    RestartFlag = 0x2 << 16,
    ReportAll = 0x4 << 16,
    ReportSubscribe = 0x8 << 16,
    NoReportDDL = 0x10 << 16,
    NR_Sub_Dropped = 0x1 << 24  // sub is dropped but needs to be copied
  };

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 subscriptionType;
  Uint32 tableId;
  Uint32 schemaTransId;
};

struct SubCreateRef {
  friend bool printSUB_CREATE_REF(FILE *, const Uint32 *, Uint32, Uint16);
  static constexpr Uint32 SignalLength = 3;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;

  enum ErrorCode {
    SubscriptionAlreadyExist = 1415,
    OutOfSubscriptionRecords = 1422,
    OutOfTableRecords = 1423,
    TableDropped = 1417,
    NF_FakeErrorREF = 11,
    NotStarted = 1428
  };
};

struct SubCreateConf {
  friend bool printSUB_CREATE_CONF(FILE *, const Uint32 *, Uint32, Uint16);
  static constexpr Uint32 SignalLength = 2;

  Uint32 senderRef;
  Uint32 senderData;
};

struct SubscriptionData {
  enum Part { MetaData = 1, TableData = 2 };
};

struct SubStartReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Suma;

  friend bool printSUB_START_REQ(FILE *, const Uint32 *, Uint32, Uint16);
  static constexpr Uint32 SignalLengthWithoutRequestInfo = 7;
  static constexpr Uint32 SignalLength = 8;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
  Uint32 subscriberRef;
  Uint32 requestInfo;

  // For requestInfo bitwise options
  enum RequestInfo {
    FILTER_ANYVALUE_MYSQL_NO_LOGGING = 1 << 0,
    FILTER_ANYVALUE_MYSQL_NO_REPLICA_UPDATES = 1 << 1,
  };
};

struct SubStartRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Suma;

  friend bool printSUB_START_REF(FILE *, const Uint32 *, Uint32, Uint16);
  enum ErrorCode {
    Undefined = 1,
    NF_FakeErrorREF = 11,
    Busy = 701,
    PartiallyConnected = 1421,
    NoSuchSubscription = 1407,
    Locked = 1411,
    Dropped = 1418,
    Defining = 1418,
    OutOfSubscriberRecords = 1412,
    OutOfSubOpRecords = 1424,
    NotMaster = 702,  // For API/DICT communication
    BusyWithNR = 1405,
    NodeDied = 1427,
    NotStarted = 1428,
    SubscriberNodeIdUndefined = 1429
  };

  static constexpr Uint32 SignalLength = 7;
  static constexpr Uint32 SignalLength2 = SignalLength + 1;
  static constexpr Uint32 SL_MasterNode = 9;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
  // do not change the order here!
  Uint32 errorCode;
  // with SignalLength2
  union {
    Uint32 subscriberRef;
    Uint32 m_masterNodeId;
  };
};

struct SubStartConf {
  /**
   * Sender(s)/Reciver(s)
   */

  friend bool printSUB_START_CONF(FILE *, const Uint32 *, Uint32, Uint16);
  static constexpr Uint32 SignalLength = 9;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 firstGCI;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
  Uint32 bucketCount;
  Uint32 nodegroup;
};

struct SubStopReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Suma;

  enum RequestInfo { RI_ABORT_START = 0x1 };

  friend bool printSUB_STOP_REQ(FILE *, const Uint32 *, Uint32, Uint16);
  static constexpr Uint32 SignalLength = 8;
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
  Uint32 subscriberRef;
  Uint32 requestInfo;
};

struct SubStopRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Suma;

  friend bool printSUB_STOP_REF(FILE *, const Uint32 *, Uint32, Uint16);
  enum ErrorCode {
    Undefined = 1,
    NF_FakeErrorREF = 11,
    Busy = 701,
    NoSuchSubscription = 1407,
    Locked = 1411,
    Defining = 1425,
    OutOfSubOpRecords = 1424,
    NoSuchSubscriber = 1426,
    NotMaster = 702,
    BusyWithNR = 1405,
    NotStarted = 1428
  };

  static constexpr Uint32 SignalLength = 8;
  static constexpr Uint32 SL_MasterNode = 9;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
  Uint32 subscriberRef;
  Uint32 errorCode;
  Uint32 m_masterNodeId;
};

struct SubStopConf {
  /**
   * Sender(s)/Reciver(s)
   */

  friend bool printSUB_STOP_CONF(FILE *, const Uint32 *, Uint32, Uint16);
  static constexpr Uint32 SignalLengthWithGci = 9;
  static constexpr Uint32 SignalLength = 9;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part;  // SubscriptionData::Part
  Uint32 subscriberData;
  Uint32 subscriberRef;
  //
  Uint32 gci_hi;
  Uint32 gci_lo;
};

struct SubSyncReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Suma;

  friend bool printSUB_SYNC_REQ(FILE *, const Uint32 *, Uint32, Uint16);
  static constexpr Uint32 SignalLength = 9;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 part;  // SubscriptionData::Part
  Uint32 requestInfo;
  Uint32 fragCount;
  Uint32 fragId;  // ZNIL if not used
  Uint32 batchSize;

  enum {
    LM_Exclusive = 0x1,
    ReorgDelete = 0x2,
    NoDisk = 0x4,
    TupOrder = 0x8,
    LM_CommittedRead = 0x10,
    RangeScan = 0x20,
    StatScan = 0x40
  };

  SECTION(ATTRIBUTE_LIST = 0);  // Used when doing SingelTableScan
  SECTION(TABLE_LIST = 1);
  SECTION(TUX_BOUND_INFO = 1);  // If range scan
};

struct SubSyncRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Suma;

  friend bool printSUB_SYNC_REF(FILE *, const Uint32 *, Uint32, Uint16);
  enum ErrorCode { Undefined = 1 };
  static constexpr Uint32 SignalLength = 3;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
  Uint32 masterNodeId;
};

struct SubSyncConf {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Suma;

  friend bool printSUB_SYNC_CONF(FILE *, const Uint32 *, Uint32, Uint16);
  static constexpr Uint32 SignalLength = 2;

  Uint32 senderRef;
  Uint32 senderData;
};

struct SubTableData {
  friend bool printSUB_TABLE_DATA(FILE *, const Uint32 *, Uint32, Uint16);
  static constexpr Uint32 SignalLength = 8;
  static constexpr Uint32 SignalLengthWithTransId = 10;
  SECTION(DICT_TAB_INFO = 0);
  SECTION(ATTR_INFO = 0);
  SECTION(AFTER_VALUES = 1);
  SECTION(BEFORE_VALUES = 2);

  enum Flags { SCAN = 1, LOG = 2, REMOVE_FLAGS = 0xff };

  Uint32 senderData;
  Uint32 gci_hi;
  Uint32 tableId;
  Uint32 requestInfo;
  Uint32 flags;
  union {
    Uint32 changeMask;
    Uint32 anyValue;
    Uint32 takeOver;
  };
  Uint32 totalLen;
  Uint32 gci_lo;
  Uint32 transId1;
  Uint32 transId2;

  static void setOperation(Uint32 &ri, Uint32 val) {
    ri = (ri & 0xFFFFFF00) | val;
  }
  static void setReqNodeId(Uint32 &ri, Uint32 val) {
    ri = (ri & 0xFFFF00FF) | (val << 8);
  }
  static void setNdbdNodeId(Uint32 &ri, Uint32 val) {
    ri = (ri & 0xFF00FFFF) | (val << 16);
  }

  static Uint32 getOperation(const Uint32 &ri) { return (ri & 0xFF); }

  static Uint32 getReqNodeId(const Uint32 &ri) { return (ri >> 8) & 0xFF; }

  static Uint32 getNdbdNodeId(const Uint32 &ri) { return (ri >> 16) & 0xFF; }
};

struct SubSyncContinueReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Trix;

  friend bool printSUB_SYNC_CONTINUE_REQ(FILE *, const Uint32 *, Uint32,
                                         Uint16);
  static constexpr Uint32 SignalLength = 3;

  Uint32 subscriberData;
  Uint32 noOfRowsSent;
  Uint32 senderData;
};

struct SubSyncContinueRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Trix;

  friend bool printSUB_SYNC_CONTINUE_REF(FILE *, const Uint32 *, Uint32,
                                         Uint16);
  static constexpr Uint32 SignalLength = 3;

  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 senderData;
};

struct SubSyncContinueConf {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Trix;

  friend bool printSUB_SYNC_CONTINUE_CONF(FILE *, const Uint32 *, Uint32,
                                          Uint16);
  static constexpr Uint32 SignalLength = 3;

  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 senderData;
};

struct SubGcpCompleteRep {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Dbdih;
  friend class Trix;

  friend bool printSUB_GCP_COMPLETE_REP(FILE *, const Uint32 *, Uint32, Uint16);
  static constexpr Uint32 SignalLength = 5;
  static constexpr Uint32 ON_DISK = 1;
  static constexpr Uint32 IN_MEMORY = 2;
  static constexpr Uint32 MISSING_DATA = 4;
  static constexpr Uint32 ADD_CNT = 8;   // Uses hi 16-bit for delta
  static constexpr Uint32 SUB_CNT = 16;  // Uses hi 16-bit for delta
  static constexpr Uint32 SUB_DATA_STREAMS_IN_SIGNAL =
      32;  // Whether sub datat stream identifiers are appended to signal
  // If the number of sub data streams increase in future, we may need to put
  // the identifiers in a separate section.

  Uint32 gci_hi;
  Uint32 senderRef;
  Uint32 gcp_complete_rep_count;
  Uint32 gci_lo;
  Uint32 flags;

  /**
   * If SUB_DATA_STREAMS_IN_SIGNAL flag is set,
   * gcp_complete_rep_count will indicate the number of 16-bit data
   * stream identifiers appended.  A word is packed with two stream
   * identifiers.  If and odd number of identifiers are indicated,
   * the high 16-bit of last word are not used, but should be zero
   * filled.
   */
  Uint32 sub_data_streams[1];
};

struct SubGcpCompleteAck {
  /**
   * Sender(s)/Reciver(s)
   */
  static constexpr Uint32 SignalLength = SubGcpCompleteRep::SignalLength;

  SubGcpCompleteRep rep;
};

struct SubRemoveReq {
  friend bool printSUB_REMOVE_REQ(FILE *, const Uint32 *, Uint32, Uint16);
  static constexpr Uint32 SignalLength = 4;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
};

struct SubRemoveRef {
  friend bool printSUB_REMOVE_REF(FILE *, const Uint32 *, Uint32, Uint16);
  static constexpr Uint32 SignalLength = 5;
  enum ErrorCode {
    Undefined = 1,
    NF_FakeErrorREF = 11,
    Busy = 701,
    NoSuchSubscription = 1407,
    Locked = 1411,
    Defining = 1418,
    AlreadyDropped = 1419,
    NotStarted = 1428
  };

  Uint32 senderRef;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 errorCode;
  Uint32 senderData;
};

struct SubRemoveConf {
  friend bool printSUB_REMOVE_CONF(FILE *, const Uint32 *, Uint32, Uint16);
  static constexpr Uint32 SignalLength = 5;

  Uint32 senderRef;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 errorCode;
  Uint32 senderData;
};

struct CreateSubscriptionIdReq {
  friend bool printCREATE_SUBSCRIPTION_ID_REQ(FILE *, const Uint32 *, Uint32,
                                              Uint16);
  static constexpr Uint32 SignalLength = 2;

  Uint32 senderRef;
  Uint32 senderData;
};

struct CreateSubscriptionIdConf {
  friend bool printCREATE_SUBSCRIPTION_ID_CONF(FILE *, const Uint32 *, Uint32,
                                               Uint16);
  static constexpr Uint32 SignalLength = 4;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
};

struct CreateSubscriptionIdRef {
  friend bool printCREATE_SUBSCRIPTION_ID_REF(FILE *, const Uint32 *, Uint32,
                                              Uint16);
  static constexpr Uint32 SignalLength = 3;

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
};

struct SumaStartMeReq {
  static constexpr Uint32 SignalLength = 1;
  Uint32 unused;
};

struct SumaStartMeRef {
  static constexpr Uint32 SignalLength = 1;
  Uint32 errorCode;
  enum { Busy = 0x1, NotStarted = 0x2 };
};

struct SumaStartMeConf {
  static constexpr Uint32 SignalLength = 1;
  Uint32 unused;
};

struct SumaHandoverReq {
  static constexpr Uint32 SignalLength = 4;
  Uint32 gci;
  Uint32 nodeId;
  Uint32 theBucketMask[1];
  Uint32 requestType;

  enum RequestType { RT_START_NODE = 0, RT_STOP_NODE = 1 };
};

struct SumaHandoverConf {
  static constexpr Uint32 SignalLength = 4;
  Uint32 gci;
  Uint32 nodeId;
  Uint32 theBucketMask[1];
  Uint32 requestType;
};

struct SumaContinueB {
  enum {
    RESEND_BUCKET = 1,
    RELEASE_GCI = 2,
    OUT_OF_BUFFER_RELEASE = 3,
    API_FAIL_GCI_LIST = 4,
    API_FAIL_SUBSCRIBER_LIST = 5,
    API_FAIL_SUBSCRIPTION = 6,
    SUB_STOP_REQ = 7,
    RETRY_DICT_LOCK = 8,
    HANDOVER_WAIT_TIMEOUT = 9,
    WAIT_SCAN_TAB_REQ = 10,
    WAIT_GET_FRAGMENT = 11,
    SEND_SUB_GCP_COMPLETE_REP = 12,
    REPORT_SUBSCRIPTION_SET = 13
  };
};

#undef JAM_FILE_ID

#endif
