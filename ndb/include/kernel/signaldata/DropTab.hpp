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

#ifndef DROP_TAB_HPP
#define DROP_TAB_HPP

#include "SignalData.hpp"

class DropTabReq {
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
  
  friend bool printDROP_TAB_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );

  enum RequestType {
    OnlineDropTab = 0,
    CreateTabDrop = 1,
    RestartDropTab = 2
  };
private:
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 requestType;
};

class DropTabConf {
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

  friend bool printDROP_TAB_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 3 );

private:
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
};

class DropTabRef {
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

  friend bool printDROP_TAB_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );

  enum ErrorCode {
    NoSuchTable = 1,
    DropWoPrep = 2, // Calling Drop with first calling PrepDrop
    PrepDropInProgress = 3,
    DropInProgress = 4,
    NF_FakeErrorREF = 5
  };
  
private:
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 errorCode;
};

#endif
