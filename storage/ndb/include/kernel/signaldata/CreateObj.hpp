/*
   Copyright (C) 2005, 2006 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#ifndef CREATE_OBJ_HPP
#define CREATE_OBJ_HPP

#include "DictObjOp.hpp"
#include "SignalData.hpp"

/**
 * CreateObj
 *
 * Implemenatation of CreateObj
 */
struct CreateObjReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printCREATE_OBJ_REQ(FILE*, const Uint32*, Uint32, Uint16);
  
public:
  STATIC_CONST( SignalLength = 10 );
  STATIC_CONST( GSN = GSN_CREATE_OBJ_REQ );
  
private:
  Uint32 op_key;
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 requestInfo;

  Uint32 clientRef;
  Uint32 clientData;

  Uint32 objId;
  Uint32 objType;
  Uint32 objVersion;
  Uint32 gci;

  SECTION( DICT_OBJ_INFO = 0 );
};

struct CreateObjRef {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;
  friend class SafeCounter;
  
  /**
   * For printing
   */
  friend bool printCREATE_OBJ_REF(FILE *, const Uint32 *, Uint32, Uint16);
  
  STATIC_CONST( SignalLength = 6 );
  STATIC_CONST( GSN = GSN_CREATE_OBJ_REF );

  enum ErrorCode {
    NF_FakeErrorREF = 255
  };


  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
  Uint32 errorLine; 
  Uint32 errorKey;
  Uint32 errorStatus;
};

struct CreateObjConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;
  
  /**
   * For printing
   */
  friend bool printCREATE_OBJ_CONF(FILE *, const Uint32 *, Uint32, Uint16);
  
public:
  STATIC_CONST( SignalLength = 2 );

private:
  Uint32 senderRef;
  Uint32 senderData;
};

#endif
