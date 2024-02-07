/*
   Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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

#ifndef BUILD_FK_IMPL_HPP
#define BUILD_FK_IMPL_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 207

struct BuildFKImplReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printBUILD_FK_IMPL_REQ(FILE *, const Uint32 *, Uint32, Uint16);

  static constexpr Uint32 SignalLength = 8;
  static constexpr Uint32 PARENT_COLUMNS_SEC = 0;
  static constexpr Uint32 CHILD_COLUMNS_SEC = 1;

  enum {
    RT_PARSE = 0x1,
    RT_PREPARE = 0x2,
    RT_ABORT = 0x3,
    RT_COMMIT = 0x4,
    RT_COMPLETE = 0x5
  };

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 transId;
  Uint32 requestType;
  Uint32 fkId;
  Uint32 fkVersion;
  Uint32 parentTableId;  // could be unique index...
  Uint32 childTableId;
};

struct BuildFKImplRef {
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printBUILD_FK_IMPL_REF(FILE *, const Uint32 *, Uint32, Uint16);

  static constexpr Uint32 SignalLength = 3;

  enum ErrorCode { NoError = 0 };

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;
};

struct BuildFKImplConf {
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printBUILD_FK_IMPL_CONF(FILE *, const Uint32 *, Uint32, Uint16);

  static constexpr Uint32 SignalLength = 4;

  Uint32 senderData;
  Uint32 senderRef;
};

#undef JAM_FILE_ID

#endif
