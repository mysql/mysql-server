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

#ifndef NDB_STTOR_HPP
#define NDB_STTOR_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 142


class NdbSttor {
  /**
   * Sender(s)
   */
  friend class NdbCntr;
  
  /**
   * Reciver(s)
   */
  friend class Ndbcntr;
  friend class Dbdict;
  friend class Dbdih;
  friend class Dblqh;
  friend class Dbtc;
  friend class ClusterMgr;
  friend class Trix;
  friend class Backup;
  friend class Suma;
  friend class LocalProxy;

  friend bool printNDB_STTOR(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 5;
  static constexpr Uint32 DataLength = 16;
private:

  Uint32 senderRef;
  Uint32 nodeId;
  Uint32 internalStartPhase;
  Uint32 typeOfStart;
  Uint32 masterNodeId;
  Uint32 unused;
  Uint32 config[DataLength];
};

DECLARE_SIGNAL_SCOPE(GSN_STTOR, Local);
DECLARE_SIGNAL_SCOPE(GSN_NDB_STTOR, Local);

class NdbSttorry {
  /**
   * Receiver(s)
   */
  friend class NdbCntr;

  /**
   * Sender(s)
   */
  friend class Ndbcntr;
  friend class Dbdict;
  friend class Dbdih;
  friend class Dblqh;
  friend class Dbtc;
  friend class ClusterMgr;
  friend class Trix;
  friend class Backup;
  friend class Suma;
  friend class LocalProxy;

  friend bool printNDB_STTORRY(FILE *, const Uint32 *, Uint32, Uint16);
public:
  static constexpr Uint32 SignalLength = 1;
private:

  Uint32 senderRef;
};

DECLARE_SIGNAL_SCOPE(GSN_STTORRY, Local);
DECLARE_SIGNAL_SCOPE(GSN_NDB_STTORRY, Local);

#undef JAM_FILE_ID

#endif
