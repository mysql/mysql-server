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

#ifndef TUP_FRAG_HPP
#define TUP_FRAG_HPP

#include "SignalData.hpp"

/*
 * Add fragment and add attribute signals between LQH and TUP,TUX.
 * NOTE: return signals from TUP,TUX to LQH must have same format.
 */

// TUP: add fragment

class TupFragReq {
  friend class Dblqh;
  friend class Dbtup;
public:
  STATIC_CONST( SignalLength = 14 );
private:
  Uint32 userPtr;
  Uint32 userRef;
  Uint32 reqInfo;
  Uint32 tableId;
  Uint32 noOfAttr;
  Uint32 fragId;
  Uint32 todo[8];
};

class TupFragConf {
  friend class Dblqh;
  friend class Dbtup;
public:
  STATIC_CONST( SignalLength = 4 );
private:
  Uint32 userPtr;
  Uint32 tupConnectPtr;
  Uint32 fragPtr;
  Uint32 fragId;
};

class TupFragRef {
  friend class Dblqh;
  friend class Dbtup;
public:
  STATIC_CONST( SignalLength = 2 );
private:
  Uint32 userPtr;
  Uint32 errorCode;
};

// TUX: add fragment

class TuxFragReq {
  friend class Dblqh;
  friend class Dbtux;
public:
  STATIC_CONST( SignalLength = 14 );
private:
  Uint32 userPtr;
  Uint32 userRef;
  Uint32 reqInfo;
  Uint32 tableId;
  Uint32 noOfAttr;
  Uint32 fragId;
  Uint32 fragOff;
  Uint32 tableType;
  Uint32 primaryTableId;
  Uint32 tupIndexFragPtrI;
  Uint32 tupTableFragPtrI[2];
  Uint32 accTableFragPtrI[2];
};

class TuxFragConf {
  friend class Dblqh;
  friend class Dbtux;
public:
  STATIC_CONST( SignalLength = 4 );
private:
  Uint32 userPtr;
  Uint32 tuxConnectPtr;
  Uint32 fragPtr;
  Uint32 fragId;
};

class TuxFragRef {
  friend class Dblqh;
  friend class Dbtux;
public:
  STATIC_CONST( SignalLength = 2 );
  enum ErrorCode {
    NoError = 0,
    InvalidRequest = 800,
    NoFreeFragment = 604,
    NoFreeAttributes = 827
  };
private:
  Uint32 userPtr;
  Uint32 errorCode;
};

// TUP: add attribute

class TupAddAttrReq {
  friend class Dblqh;
  friend class Dbtux;
public:
  STATIC_CONST( SignalLength = 5 );
private:
  Uint32 tupConnectPtr;
  Uint32 notused1;
  Uint32 attrId;
  Uint32 attrDescriptor;
  Uint32 extTypeInfo;
};

class TupAddAttrConf {
  friend class Dblqh;
  friend class Dbtup;
public:
  STATIC_CONST( SignalLength = 2 );
private:
  Uint32 userPtr;
  Uint32 lastAttr; // bool: got last attr and closed frag op
};

class TupAddAttrRef {
  friend class Dblqh;
  friend class Dbtup;
public:
  STATIC_CONST( SignalLength = 2 );
  enum ErrorCode {
    NoError = 0,
    InvalidCharset = 743
  };
private:
  Uint32 userPtr;
  Uint32 errorCode;
};

// TUX: add attribute

class TuxAddAttrReq {
  friend class Dblqh;
  friend class Dbtux;
public:
  STATIC_CONST( SignalLength = 6 );
private:
  Uint32 tuxConnectPtr;
  Uint32 notused1;
  Uint32 attrId;
  Uint32 attrDescriptor;
  Uint32 extTypeInfo;
  Uint32 primaryAttrId;
};

class TuxAddAttrConf {
  friend class Dblqh;
  friend class Dbtux;
public:
  STATIC_CONST( SignalLength = 2 );
private:
  Uint32 userPtr;
  Uint32 lastAttr; // bool: got last attr and closed frag op
};

class TuxAddAttrRef {
  friend class Dblqh;
  friend class Dbtux;
public:
  STATIC_CONST( SignalLength = 2 );
  enum ErrorCode {
    NoError = 0,
    InvalidAttributeType = 742,
    InvalidCharset = 743,
    InvalidNodeSize = 832
  };
private:
  Uint32 userPtr;
  Uint32 errorCode;
};

#endif
