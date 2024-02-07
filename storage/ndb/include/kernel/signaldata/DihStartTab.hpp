/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#ifndef DIH_STARTTAB__HPP
#define DIH_STARTTAB__HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 96

class DihStartTabReq {
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * Receiver(s)
   */
  friend class Dbdih;

 public:
  static constexpr Uint32 HeaderLength = 3;

 private:
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 noOfTables;

  struct {
    Uint32 tableId;
    Uint32 schemaVersion;
  } tables[10];
};

class DihStartTabConf {
  /**
   * Sender(s)
   */
  friend class Dbdih;

  /**
   * Receiver(s)
   */
  friend class Dbdict;

 public:
  static constexpr Uint32 SignalLength = 2;

 private:
  Uint32 senderRef;
  Uint32 senderData;
};

#undef JAM_FILE_ID

#endif
