/*
   Copyright (c) 2005, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef DROP_OBJ_HPP
#define DROP_OBJ_HPP

#include "DictObjOp.hpp"
#include "SignalData.hpp"

#define JAM_FILE_ID 25

struct DropObjReq {
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
  static constexpr Uint32 SignalLength = 9;

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
  static constexpr Uint32 SignalLength = 3;

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
  static constexpr Uint32 SignalLength = 4;

  enum ErrorCode {
    NoSuchObj = 1,
    DropWoPrep = 2,  // Calling Drop with first calling PrepDrop
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

#undef JAM_FILE_ID

#endif
