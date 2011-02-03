/*
   Copyright (C) 2003, 2005, 2006, 2008 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#ifndef NDB_STTOR_HPP
#define NDB_STTOR_HPP

#include "SignalData.hpp"

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
  STATIC_CONST( SignalLength = 5 );
  STATIC_CONST( DataLength = 16 );
private:

  Uint32 senderRef;
  Uint32 nodeId;
  Uint32 internalStartPhase;
  Uint32 typeOfStart;
  Uint32 masterNodeId;
  Uint32 unused;
  Uint32 config[DataLength];
};

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
  STATIC_CONST( SignalLength = 1 );
private:

  Uint32 senderRef;
};

#endif
