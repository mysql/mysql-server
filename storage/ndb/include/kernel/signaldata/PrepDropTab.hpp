/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef PREP_DROP_TAB_HPP
#define PREP_DROP_TAB_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 161


class PrepDropTabReq {
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * Receiver(s)
   */
  friend class Dbspj;
  friend class Dbtc;
  friend class Dblqh;
  friend class DblqhProxy;
  friend class Dbdih;
  friend class DbgdmProxy;

  friend bool printPREP_DROP_TAB_REQ(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 4;

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
  friend class Dbspj;
  friend class Dbtc;
  friend class Dblqh;
  friend class DblqhProxy;
  friend class Dbdih;
  friend class DbgdmProxy;

  /**
   * Receiver(s)
   */
  friend class Dbdict;

  friend bool printPREP_DROP_TAB_CONF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 3;

private:
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 tableId;
};

class PrepDropTabRef {
  /**
   * Sender(s)
   */
  friend class Dbspj;
  friend class Dbtc;
  friend class Dblqh;
  friend class DblqhProxy;
  friend class Dbdih;
  friend class DbgdmProxy;

  /**
   * Receiver(s)
   */
  friend class Dbdict;

  friend bool printPREP_DROP_TAB_REF(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 4;

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


#undef JAM_FILE_ID

#endif
