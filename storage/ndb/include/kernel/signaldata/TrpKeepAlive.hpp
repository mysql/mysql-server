/*
   Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef NDB_TRP_KEEP_ALIVE_HPP
#define NDB_TRP_KEEP_ALIVE_HPP

#define JAM_FILE_ID 541

#include "ndb_types.h"

#include <stdio.h>

class TrpKeepAlive {
  /**
   * Sender(s) & Reciver(s)
   */
  friend class Qmgr;

  /**
   * For printing
   */
  friend bool printTRP_KEEP_ALIVE(FILE *, const Uint32 *, Uint32, Uint16);

 public:
  static constexpr Uint32 SignalLength = 2;

 private:
  Uint32 senderRef;
  Uint32 keepalive_seqnum;
};

#undef JAM_FILE_ID

#endif
