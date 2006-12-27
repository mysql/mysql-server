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

#ifndef DROP_OBJ_HPP
#define DROP_OBJ_HPP

#include "DictObjOp.hpp"
#include "SignalData.hpp"

struct DropObjReq 
{
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * Receiver(s)
   */
  friend class Dbtc;
  friend class Dblqh;
  friend class Dbacc;
  friend class Dbtup;
  friend class Dbtux;
  friend class Dbdih;
  
  friend bool printDROP_OBJ_REQ(FILE *, const Uint32 *, Uint32, Uint16);
  STATIC_CONST( SignalLength = 9 );

  Uint32 op_key;
  Uint32 objId;
  Uint32 objType;
  Uint32 objVersion;

  Uint32 senderRef;
  Uint32 senderData;

  Uint32 requestInfo;

  Uint32 clientRef;
  Uint32 clientData;
};

class DropObjConf {
  /**
   * Sender(s)
   */
  friend class Dbtc;
  friend class Dblqh;
  friend class Dbacc;
  friend class Dbtup;
  friend class Dbtux;
  friend class Dbdih;

  /**
   * Receiver(s)
   */
  friend class Dbdict;

  friend bool printDROP_OBJ_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 3 );

private:
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 objId;
};

class DropObjRef {
  /**
   * Sender(s)
   */
  friend class Dbtc;
  friend class Dblqh;
  friend class Dbacc;
  friend class Dbtup;
  friend class Dbtux;
  friend class Dbdih;

  /**
   * Receiver(s)
   */
  friend class Dbdict;

  friend bool printDROP_OBJ_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );

  enum ErrorCode {
    NoSuchObj = 1,
    DropWoPrep = 2, // Calling Drop with first calling PrepDrop
    PrepDropInProgress = 3,
    DropInProgress = 4,
    NF_FakeErrorREF = 5
  };
  
private:
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 objId;
  Uint32 errorCode;
};

#endif
