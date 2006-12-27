/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef GREP_IMPL_HPP
#define GREP_IMPL_HPP

#include "SignalData.hpp"
#include <GrepError.hpp>
#include <NodeBitmask.hpp>



/*****************************************************************************
 * GREP REQ   Request a Global Replication  (between SS and PS)
 *****************************************************************************/
/**
 * @class GrepReq
 * @brief
 */
class GrepReq 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;

public:
  enum Request {
    START = 0,            ///< Start Global Replication (all phases)
    SLOWSTOP = 1,       ///< Stop after finishing applying current GCI epoch
    FASTSTOP = 2,       ///< Stop after finishing applying all PS GCI epochs
    STATUS = 3,           ///< Status
    REMOVE_BUFFERS = 4,   ///< Remove buffers from PS and SS

    START_SUBSCR = 5,
    START_METALOG = 6,    ///< Start Global Replication Logging of Metadata
    START_METASCAN = 7,   ///< Start Global Replication Scanning of Metadata
    START_DATALOG = 8,    ///< Start Global Replication Logging of table data
    START_DATASCAN = 9,   ///< Start Global Replication Scanning of table data
    START_REQUESTOR = 10, ///< Start Global Replication Requestor
    START_TRANSFER = 11,  ///< Start SS-PS transfer
    START_APPLY = 12,     ///< Start applying GCI epochs in SS
    START_DELETE = 13,    ///< Start deleting buffers at PS/SS REP automatic.

    STOP_SUBSCR = 14,     ///< Remove subscription
    STOP_METALOG = 15,    ///< Stop Global Replication Logging of Metadata
    STOP_METASCAN = 16,   ///< Stop Global Replication Scanning of Metadata
    STOP_DATALOG = 17,    ///< Stop Global Replication Logging of table data
    STOP_DATASCAN = 18,   ///< Stop Global Replication Scanning of table data
    STOP_REQUESTOR = 19,  ///< Stop Global Replication Requestor
    STOP_TRANSFER = 20,   ///< Stop SS-PS transfer
    STOP_APPLY = 21,      ///< Stop applying GCI epochs in SS
    STOP_DELETE = 22,     ///< Stop deleting buffers at PS/SS REP automatically
    CREATE_SUBSCR = 23,   ///< Create subscription ID in SUMA
    DROP_TABLE = 24,      ///< Create subscription ID in SUMA
    STOP = 25,

    NO_REQUEST = 0xffffffff
  };

  STATIC_CONST( SignalLength = 2 );

  Uint32 senderRef;
  Uint32 request;
};


/*****************************************************************************
 * CREATE   Between SS and PS  (DB and REP nodes)
 *****************************************************************************/
/**
 * @class GrepSubCreateReq
 * @brief
 */
class GrepSubCreateReq 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  
  friend bool printGREP_SUB_CREATE_REQ(FILE *, 
				       const Uint32 *, 
				       Uint32, 
				       Uint16);
public:
  STATIC_CONST( SignalLength = 5 );
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 subscriptionType;
  Uint32 senderRef;
  Uint32 senderData;
  SECTION( TABLE_LIST = 0 );  
};

/**
 * @class GrepSubCreateReq
 * @brief
 */
class GrepSubCreateRef 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  
  friend bool printGREP_SUB_CREATE_REF(FILE *, 
				       const Uint32 *, 
				       Uint32, 
				       Uint16);
public:
  STATIC_CONST( SignalLength = 6 );
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 subscriptionType;
  Uint32 err;
  Uint32 senderRef;
  Uint32 senderData;
};


/**
 * @class GrepSubCreateConf
 * @brief
 */
class GrepSubCreateConf 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  
  friend bool printGREP_SUB_CREATE_CONF(FILE *, 
					const Uint32 *, 
					Uint32, 
					Uint16);
public:
  STATIC_CONST( SignalLength = 6 );
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 subscriptionType;
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 noOfNodeGroups;
};



/*****************************************************************************
 * CREATE   Internal between PS DB nodes
 *****************************************************************************/

/**
 * @class GrepCreateReq
 * @brief
 */
class GrepCreateReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class GrepParticipant;
  
  friend bool printGREP_CREATE_REQ(FILE *, 
				   const Uint32 *, 
				   Uint32, 
				   Uint16);
public:
  STATIC_CONST( SignalLength = 8 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriberData;
  Uint32 subscriberRef;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 subscriptionType;
  SECTION( TABLE_LIST = 0 );  
};


/**
 * @class GrepCreateRef
 * @brief
 */
class GrepCreateRef {
  /**
   * Sender(s)/Reciver(s)
   */

  friend class GrepParticipant;  

  friend bool printGREP_CREATE_REF(FILE *, 
				   const Uint32 *, 
				   Uint32, 
				   Uint16);
public:
  enum ErrorCode {
    NF_FakeErrorREF = GrepError::NF_FakeErrorREF
  };
  STATIC_CONST( SignalLength = 6 );
  Uint32 senderRef;
  Uint32 senderData;
  union {
    Uint32 err;
    Uint32 errorCode;
  };
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 subscriptionType;
};


/**
 * @class GrepCreateConf
 * @brief
 */
class GrepCreateConf {
  /**
   * Sender(s)/Reciver(s)
   */

  friend class GrepParticipant;
  
  friend bool printGREP_CREATE_CONF(FILE *, 
				   const Uint32 *, 
				   Uint32, 
				   Uint16);
public:
  STATIC_CONST( SignalLength = 6 );
  Uint32 senderNodeId;
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 subscriptionType;
};


/*****************************************************************************
 * START   Between SS and PS  (DB and REP nodes)
 *****************************************************************************/

/**
 * @class GrepSubStartReq
 * @brief
 */
class GrepSubStartReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  
  friend bool printGREP_SUB_START_REQ(FILE *, 
				   const Uint32 *, 
				   Uint32, 
				   Uint16);
public:
  STATIC_CONST( SignalLength = 5 );
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 part;
};

/**
 * @class GrepSubStartRef
 * @brief
 */
class GrepSubStartRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  
  friend bool printGREP_SUB_START_REF(FILE *, 
				   const Uint32 *, 
				   Uint32, 
				   Uint16);
public:
  STATIC_CONST( SignalLength = 6 );
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 err;
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 part;
};



/**
 * @class GrepSubStartConf
 * @brief
 */
class GrepSubStartConf {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  
  friend bool printGREP_SUB_START_CONF(FILE *, 
				       const Uint32 *, 
				       Uint32, 
				       Uint16);
public:
  STATIC_CONST( SignalLength = 6 );

  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 part;
  Uint32 firstGCI;
};


/*****************************************************************************
 * START  Internal between PS DB nodes
 *****************************************************************************/

/**
 * @class GrepStartReq
 * @brief
 */
class GrepStartReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class GrepParticipant;
  
  friend bool printGREP_START_REQ(FILE *, 
				   const Uint32 *, 
				   Uint32, 
				   Uint16);
public:
  STATIC_CONST( SignalLength = 4 );
 
  Uint32 senderData;
  Uint32 part;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
};


/**
 * @class GrepStartRef
 * @brief
 */
class GrepStartRef {
  /**
   * Sender(s)/Reciver(s)
   */

  friend class GrepParticipant;  

  friend bool printGREP_START_REF(FILE *, 
				  const Uint32 *, 
				  Uint32, 
				  Uint16);
public:
  enum ErrorCode {
    NF_FakeErrorREF = GrepError::NF_FakeErrorREF
  };
  STATIC_CONST( SignalLength = 6 );
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 part;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  union {
    Uint32 err;
    Uint32 errorCode;
  };
};


/**
 * @class GrepStartConf
 * @brief
 */
class GrepStartConf {
   /**
    * Sender(s)/Reciver(s)
    */
   
   friend class GrepParticipant;
   
   friend bool printGREP_START_CONF(FILE *, 
				    const Uint32 *, 
				    Uint32, 
				    Uint16);
 public:
   STATIC_CONST( SignalLength = 7 );
   
   Uint32 senderRef;
   Uint32 senderData;
   Uint32 part;
   Uint32 subscriptionId;
   Uint32 subscriptionKey;
   Uint32 firstGCI;
   Uint32 senderNodeId;
 };


/*****************************************************************************
 * SCAN (SYNC)  Between SS and PS (REP and DB nodes)
 *****************************************************************************/

/**
 * @class GrepSubSyncReq
 * @brief
 */
class GrepSubSyncReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  
  friend bool printGREP_SUB_SYNC_REQ(FILE *, 
				     const Uint32 *, 
				     Uint32, 
				     Uint16);
public:
  STATIC_CONST( SignalLength = 5 );
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 part;
};


/**
 * @class GrepSubSyncRef
 * @brief
 */
class GrepSubSyncRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  
  friend bool printGREP_SUB_SYNC_REF(FILE *, 
				     const Uint32 *, 
				     Uint32, 
				     Uint16);
public:
  STATIC_CONST( SignalLength = 6 );
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 senderRef;
  Uint32 err;
  Uint32 senderData;
  Uint32 part;
};


/**
 * @class GrepSubSyncConf
 * @brief
 */
class GrepSubSyncConf {
   /**
    * Sender(s)/Reciver(s)
   */
   friend class Grep;
   
   friend bool printGREP_SUB_SYNC_CONF(FILE *, 
				       const Uint32 *, 
				       Uint32, 
				       Uint16);
 public:
   STATIC_CONST( SignalLength = 7 );
   Uint32 subscriptionId;
   Uint32 subscriptionKey;
   Uint32 senderRef;
   Uint32 senderData;
   Uint32 part;
   Uint32 firstGCI;
   Uint32 lastGCI;
};



/*****************************************************************************
 * SCAN (SYNC)  Internal between PS DB nodes
 *****************************************************************************/

/**
 * @class GrepSyncReq
 * @brief
 */
class GrepSyncReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class GrepParticipant;
  
  friend bool printGREP_SYNC_REQ(FILE *, 
				   const Uint32 *, 
				   Uint32, 
				   Uint16);
public:
  STATIC_CONST( SignalLength = 4 );
 
  Uint32 senderData;
  Uint32 part;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
};


/**
 * @class GrepSyncRef
 * @brief
 */
class GrepSyncRef {
  /**
   * Sender(s)/Reciver(s)
   */

  friend class GrepParticipant;  

  friend bool printGREP_SYNC_REF(FILE *, 
				 const Uint32 *, 
				 Uint32, 
				 Uint16);
public:
  enum ErrorCode {
    NF_FakeErrorREF = GrepError::NF_FakeErrorREF
  };
  STATIC_CONST( SignalLength = 6 );
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 part;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  union {
    Uint32 err;
    Uint32 errorCode;
  };
};


/**
 * @class GrepSyncConf
 * @brief
 */
class GrepSyncConf 
{
  /**
   * Sender(s)/Reciver(s)
   */
  friend class GrepParticipant;
  
  friend bool printGREP_SYNC_CONF(FILE *, const Uint32 *, Uint32, Uint16);

public:
  STATIC_CONST( SignalLength = 8 );
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 part;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 senderNodeId;
  Uint32 firstGCI;
  Uint32 lastGCI;
};

/*****************************************************************************
 * ABORT - remove subscription
 *****************************************************************************/

/**
 * @class GrepSubRemoveReq
 * @brief Between PS and SS
 */
class GrepSubRemoveReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  
  friend bool printGREP_SUB_REMOVE_REQ(FILE *, const Uint32 *, 
				       Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
};


/**
 * @class GrepSubRemoveRef
 * @brief Between PS and SS
 */
class GrepSubRemoveRef {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  
  friend bool printGREP_SUB_REMOVE_REF(FILE *, const Uint32 *, 
				       Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 err;
};


/**
 * @class 
 * @brief
 */
class GrepSubRemoveConf {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class Grep;
  
  friend bool printGREP_SUB_REMOVE_CONF(FILE *, 
				      const Uint32 *, 
				      Uint32, 
				      Uint16);
public:
  STATIC_CONST( SignalLength = 4 );

  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
};


/**
 * @class 
 * @brief
 */
class GrepRemoveReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class GrepParticipant;
  
  friend bool printGREP_REMOVE_REQ(FILE *, 
				   const Uint32 *, 
				   Uint32, 
				   Uint16);
public:
  STATIC_CONST( SignalLength = 4 );
 
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
};


/**
 * @class 
 * @brief
 */
class GrepRemoveRef {
  /**
   * Sender(s)/Reciver(s)
   */

  friend class GrepParticipant;  

  friend bool printGREP_REMOVE_REF(FILE *, 
				 const Uint32 *, 
				 Uint32, 
				 Uint16);
public:
  enum ErrorCode {
    NF_FakeErrorREF = GrepError::NF_FakeErrorREF
  };
  STATIC_CONST( SignalLength = 5 );
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  union {
    Uint32 err;
    Uint32 errorCode;
  };
};


/**
 * @class 
 * @brief
 */
class GrepRemoveConf {
  /**
   * Sender(s)/Reciver(s)
   */

  friend class GrepParticipant;
  
  friend bool printGREP_REMOVE_CONF(FILE *, 
				    const Uint32 *, 
				    Uint32, 
				    Uint16);
public:
  STATIC_CONST( SignalLength = 5 );
  Uint32 senderRef;
  Uint32 senderData; 
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 senderNodeId;
};


/*****************************************************************************
 * WAIT FOR CGP
 *****************************************************************************/

/**
 * @class GrepWaitGcpReq
 * @brief
 */
class GrepWaitGcpReq {
  /**
   * Sender(s)/Reciver(s)
   */

  friend class GrepParticipant;
  
  friend bool printGREP_WAITGCP_REQ(FILE *, const Uint32 *, 
				    Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );
  
  Uint32 senderData;
  Uint32 gcp;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 senderNodeId;
};

/**
 * @class GrepWaitGcpConf
 * @brief
 */
class GrepWaitGcpConf {
  /**
   * Sender(s)/Reciver(s)
   */

  friend class GrepParticipant;
  
  friend bool printGREP_WAITGCP_CONF(FILE *, 
				     const Uint32 *, 
				     Uint32, 
				     Uint16);
public:
  STATIC_CONST( SignalLength = 4 );
  
  Uint32 senderData;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 senderNodeId;
};



class GrepCreateSubscriptionIdConf {
  friend class Grep;
  
  friend bool printGREP_CREATE_SUBSCRIPTION_ID_CONF(FILE *, const Uint32 *, 
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



class GrepStartMe {
  friend class Grep;  
  friend bool printGREP_START_ME(FILE *, const Uint32 *, 
				 Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 1 );
  Uint32 senderRef;
};




/**
 * @class GrepAddSubReq
 * @brief
 */
class GrepAddSubReq {
  /**
   * Sender(s)/Reciver(s)
   */
  friend class GrepParticipant;
  
  friend bool printGREP_ADD_SUB_REQ(FILE *, 
				    const Uint32 *, 
				    Uint32, 
				    Uint16);
public:
  STATIC_CONST( SignalLength = 7 );
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 subscriberData;
  Uint32 subscriberRef;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 subscriptionType;
};


/**
 * @class GrepAddSubRef
 * @brief
 */
class GrepAddSubRef {
  /**
   * Sender(s)/Reciver(s)
   */

  friend class GrepParticipant;  

  friend bool printGREP_CREATE_REF(FILE *, 
				   const Uint32 *, 
				   Uint32, 
				   Uint16);
public:
  STATIC_CONST( SignalLength = 5 );
  Uint32 senderData;
  Uint32 err;
  Uint32 subscriptionId;
  Uint32 subscriptionKey;
  Uint32 subscriptionType;
};


/**
 * @class GrepAddSubConf
 * @brief
 */
class GrepAddSubConf {
  /**
   * Sender(s)/Reciver(s)
   */

  friend class GrepParticipant;
  
  friend bool printGREP_CREATE_CONF(FILE *, 
				   const Uint32 *, 
				   Uint32, 
				   Uint16);
public:
  STATIC_CONST( SignalLength = 1 );
  Uint32 noOfSub;
};

#endif
