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

#ifndef PREP_DROP_TAB_HPP
#define PREP_DROP_TAB_HPP

#include "SignalData.hpp"

class PrepDropTabReq {
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * Receiver(s)
   */
  friend class Dbtc;
  friend class Dblqh;
  friend class Dbdih;

  friend bool printPREP_DROP_TAB_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );

private:
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 requestType; // @see DropTabReq::RequestType
};

class PrepDropTabConf {
  /**
   * Sender(s)
   */
  friend class Dbtc;
  friend class Dblqh;
  friend class Dbdih;

  /**
   * Receiver(s)
   */
  friend class Dbdict;

  friend bool printPREP_DROP_TAB_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 3 );

private:
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
};

class PrepDropTabRef {
  /**
   * Sender(s)
   */
  friend class Dbtc;
  friend class Dblqh;
  friend class Dbdih;

  /**
   * Receiver(s)
   */
  friend class Dbdict;

  friend bool printPREP_DROP_TAB_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );

  enum ErrorCode {
    OK = 0,
    NoSuchTable = 1,
    PrepDropInProgress = 2,
    DropInProgress = 3,
    InvalidTableState = 4,
    NF_FakeErrorREF = 5
  };
  
private:
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
  Uint32 errorCode;
};

class WaitDropTabReq {
  /**
   * Sender
   */
  friend class Dbtc;
  friend class Dbdih;

  /**
   * Receiver(s)
   */
  friend class Dblqh;

  friend bool printWAIT_DROP_TAB_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 2 );
  
  Uint32 tableId;
  Uint32 senderRef;
};

class WaitDropTabRef {
  /**
   * Sender
   */
  friend class Dblqh;

  /**
   * Receiver(s)
   */
  friend class Dbtc;
  friend class Dbdih;

  friend bool printWAIT_DROP_TAB_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 4 );
  
  enum ErrorCode {
    NoSuchTable = 1,
    IllegalTableState = 2,
    DropInProgress = 3,
    NF_FakeErrorREF = 4
  };
  
  Uint32 tableId;
  Uint32 senderRef;
  Uint32 errorCode;
  Uint32 tableStatus;
};


class WaitDropTabConf {
  /**
   * Sender
   */
  friend class Dblqh;

  /**
   * Receiver(s)
   */
  friend class Dbtc;
  friend class Dbdih;

  friend bool printWAIT_DROP_TAB_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  STATIC_CONST( SignalLength = 2 );

  Uint32 tableId;
  Uint32 senderRef;
};

#endif
