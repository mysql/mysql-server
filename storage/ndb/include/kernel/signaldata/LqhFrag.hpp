/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

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
  STATIC_CONST( SignalLength = 12 );
  
  enum RequestInfo {
    CreateInRunning = 0x8000000,
    TemporaryTable  = 0x00000010
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
  Uint32 changeMask;
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
  STATIC_CONST( SignalLength = 2 );
  
private:
  Uint32 dihPtr;
  Uint32 fragId;
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
  friend class DblqhProxy;
  
  friend bool printLQH_FRAG_REQ(FILE *, const Uint32 *, Uint32, Uint16);

public:
  STATIC_CONST( SignalLength = 22 );
  
  enum RequestInfo {
    CreateInRunning = 0x8000000,
    TemporaryTable = 0x00000010
  };

private:
  Uint32 senderData;
  Uint32 senderRef;
  Uint32 tableId;
  Uint32 tableVersion;

  Uint32 nextLCP;
  Uint32 startGci;
  union {
    Uint32 fragmentId;
    Uint32 fragId;
  };
  Uint32 requestInfo; 

  Uint32 localKeyLength;
  Uint32 lh3DistrBits;
  Uint32 lh3PageBits;
  Uint32 keyLength;
  Uint32 maxLoadFactor;
  Uint32 minLoadFactor;
  Uint32 kValue;

  Uint32 logPartId;
  Uint32 tablespace_id;       // RNIL for MM table
  Uint32 maxRowsLow;
  Uint32 maxRowsHigh;
  Uint32 minRowsLow;
  Uint32 minRowsHigh;
  Uint32 changeMask;
};

class LqhFragConf {
  /**
   * Sender(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;

  /**
   * Receiver(s)
   */
  friend class Dbdict;

  friend bool printLQH_FRAG_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 5 );

private:
  Uint32 senderData;
  Uint32 lqhFragPtr;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 changeMask;
};

class LqhFragRef {
  /**
   * Sender(s)
   */
  friend class Dblqh;
  friend class DblqhProxy;

  /**
   * Receiver(s)
   */
  friend class Dbdict;

  friend bool printLQH_FRAG_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 6 );

private:
  Uint32 senderData;
  Uint32 errorCode;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 requestInfo;
  Uint32 changeMask;
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
  friend class DblqhProxy;

  friend bool printLQH_ADD_ATTR_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( HeaderLength = 4 );
  STATIC_CONST( EntryLength = 3 );
  STATIC_CONST( MAX_ATTRIBUTES = 6 );
  STATIC_CONST( DEFAULT_VALUE_SECTION_NUM = 0 );
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
  friend class DblqhProxy;
  
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
  friend class DblqhProxy;
  
  /**
   * Receiver(s)
   */
  friend class Dbdict;

  friend bool printLQH_ADD_ATTR_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 2 );

private:
  Uint32 senderData;
  Uint32 senderAttrPtr;
};

struct DropFragReq
{
  STATIC_CONST( SignalLength = 5 );
  enum RequestInfo
  {
    AlterTableAbort = 0x1
  };
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 requestInfo;
};

struct DropFragRef
{
  STATIC_CONST( SignalLength = 5 );
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 errCode;
};

struct DropFragConf
{
  STATIC_CONST( SignalLength = 4 );
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 fragId;
};

#endif
