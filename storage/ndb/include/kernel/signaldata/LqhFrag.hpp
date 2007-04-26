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

#ifndef LQH_FRAG_HPP
#define LQH_FRAG_HPP

#include "SignalData.hpp"

class AddFragReq {
  /**
   * Sender(s)
   */
  friend class Dbdih;

  /**
   * Receiver(s)
   */
  friend class Dbdict;
  
  friend bool printADD_FRAG_REQ(FILE *, const Uint32 *, Uint32, Uint16);

public:
  STATIC_CONST( SignalLength = 11 );
  
  enum RequestInfo {
    CreateInRunning = 0x8000000,
    TemporaryTable = 0x00000010
  };
private:
  Uint32 dihPtr;
  Uint32 senderData; // The same data as sent in DIADDTABREQ
  Uint32 fragmentId;
  Uint32 requestInfo; 
  Uint32 tableId;
  Uint32 nextLCP;
  Uint32 nodeId;
  Uint32 totalFragments;
  Uint32 startGci;
  Uint32 tablespaceId;
  Uint32 logPartId;
};

class AddFragRef {
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * Receiver(s)
   */
  friend class Dbdih;

  friend bool printADD_FRAG_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 1 );
  
private:
  Uint32 dihPtr;
};

class AddFragConf {
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * Receiver(s)
   */
  friend class Dbdih;

  friend bool printADD_FRAG_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 2 );
  
private:
  Uint32 dihPtr;
  Uint32 fragId;
};

class LqhFragReq {
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * Receiver(s)
   */
  friend class Dblqh;
  
  friend bool printLQH_FRAG_REQ(FILE *, const Uint32 *, Uint32, Uint16);

public:
  STATIC_CONST( SignalLength = 25 );
  
  enum RequestInfo {
    CreateInRunning = 0x8000000,
    TemporaryTable = 0x00000010
  };

private:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 fragmentId;
  Uint32 requestInfo; 
  Uint32 maxLoadFactor;
  Uint32 minLoadFactor;
  Uint32 kValue;
  Uint32 schemaVersion;
  Uint32 nextLCP;
  Uint32 noOfCharsets;
  Uint32 startGci;
  Uint32 tableType;             // DictTabInfo::TableType
  Uint32 primaryTableId;        // table of index or RNIL
  Uint32 tablespace_id;       // RNIL for MM table
  Uint16 tableId;
  Uint16 localKeyLength;
  Uint16 lh3DistrBits;
  Uint16 lh3PageBits;
  Uint16 noOfAttributes;
  Uint16 noOfNullAttributes;
  Uint16 noOfPagesToPreAllocate;
  Uint16 keyLength;
  Uint16 noOfKeyAttr;
  Uint8 checksumIndicator;
  Uint8 GCPIndicator;
  Uint32 logPartId;
  Uint32 maxRowsLow;
  Uint32 maxRowsHigh;
  Uint32 minRowsLow;
  Uint32 minRowsHigh;
  Uint32 forceVarPartFlag;
};

class LqhFragConf {
  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * Receiver(s)
   */
  friend class Dbdict;

  friend bool printLQH_FRAG_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 2 );

private:
  Uint32 senderData;
  Uint32 lqhFragPtr;
};

class LqhFragRef {
  /**
   * Sender(s)
   */
  friend class Dblqh;

  /**
   * Receiver(s)
   */
  friend class Dbdict;

  friend bool printLQH_FRAG_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 2 );

private:
  Uint32 senderData;
  Uint32 errorCode;
};

class LqhAddAttrReq {
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * Receiver(s)
   */
  friend class Dblqh;

  friend bool printLQH_ADD_ATTR_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( HeaderLength = 4 );
  STATIC_CONST( EntryLength = 3 );
  STATIC_CONST( MAX_ATTRIBUTES = 6 );
  struct Entry {
    Uint32 attrId;              // for index, includes primary attr id << 16
    Uint32 attrDescriptor;      // 2 words type info
    Uint32 extTypeInfo;
  };
private:
  Uint32 lqhFragPtr;
  Uint32 noOfAttributes;
  Uint32 senderData;
  Uint32 senderAttrPtr;
  Entry attributes[MAX_ATTRIBUTES];
};

class LqhAddAttrRef {
  /**
   * Sender(s)
   */
  friend class Dblqh;
  
  /**
   * Receiver(s)
   */
  friend class Dbdict;

  friend bool printLQH_ADD_ATTR_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 2 );

private:
  Uint32 senderData;
  Uint32 errorCode;
};

class LqhAddAttrConf {
  /**
   * Sender(s)
   */
  friend class Dblqh;
  
  /**
   * Receiver(s)
   */
  friend class Dbdict;

  friend bool printLQH_ADD_ATTR_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 3 );

private:
  Uint32 senderData;
  Uint32 senderAttrPtr;
  Uint32 fragId;
};

#endif
