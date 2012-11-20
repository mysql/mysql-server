/*
   Copyright (c) 2003-2006 MySQL AB, 2010 Sun Microsystems, Inc.
   Use is subject to license terms.

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

#ifndef DROP_TAB_HPP
#define DROP_TAB_HPP

#include "SignalData.hpp"

struct DropTabReq {
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

struct DropTabConf {
  /**
   * Sender(s)
   */
  friend class Dbtc;
  friend class Dblqh;
  friend class Dbacc;
  friend class Dbtup;
  friend class Dbtux;
  friend class Dbdih;
  friend class Suma;

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

struct DropTabRef {
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
  friend class SafeCounter;

  friend bool printDROP_TAB_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );

  enum ErrorCode {
    NoSuchTable = 1,
    DropWoPrep = 2, // Calling Drop with first calling PrepDrop
    PrepDropInProgress = 3,
    DropInProgress = 4,
    NF_FakeErrorREF = 5,
    InvalidTableState = 6
  };
  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 errorCode;
};

#endif
