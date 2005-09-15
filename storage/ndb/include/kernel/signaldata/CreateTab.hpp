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

#ifndef CREATE_TAB_HPP
#define CREATE_TAB_HPP

#include "SignalData.hpp"

/**
 * CreateTab
 *
 * Implemenatation of CreateTable
 */
class CreateTabReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printCREATE_TAB_REQ(FILE*, const Uint32*, Uint32, Uint16);
  
public:
  STATIC_CONST( SignalLength = 8 );
  
  enum RequestType {
    CreateTablePrepare = 0, // Prepare create table
    CreateTableCommit = 1,  // Commit create table
    CreateTableDrop = 2     // Prepare failed, drop instead
  };
private:
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 clientRef;
  Uint32 clientData;

  Uint32 tableId;
  Uint32 tableVersion;
  Uint32 gci;
  Uint32 requestType;

  SECTION( DICT_TAB_INFO = 0 );
  SECTION( FRAGMENTATION = 1 );
};

struct CreateTabRef {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;
  friend class SafeCounter;
  
  /**
   * For printing
   */
  friend bool printCREATE_TAB_REF(FILE *, const Uint32 *, Uint32, Uint16);
  
  STATIC_CONST( SignalLength = 6 );
  STATIC_CONST( GSN = GSN_CREATE_TAB_REF );

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

class CreateTabConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;
  friend class Suma;
  
  /**
   * For printing
   */
  friend bool printCREATE_TAB_CONF(FILE *, const Uint32 *, Uint32, Uint16);
  
public:
  STATIC_CONST( SignalLength = 2 );

private:
  Uint32 senderRef;
  Uint32 senderData;
};

#endif
