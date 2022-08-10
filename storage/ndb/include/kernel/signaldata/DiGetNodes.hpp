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

#ifndef DIGETNODES_HPP
#define DIGETNODES_HPP

#include <NodeBitmask.hpp>
#include <ndb_limits.h>

#define JAM_FILE_ID 90


/**
 * 
 */
struct DiGetNodesConf {
  /**
   * Receiver(s)
   */
  friend class Dbtc;
  friend class Dbspj;
  friend class Backup;
  friend class Suma;

  /**
   * Sender(s)
   */
  friend class Dbdih;

  static constexpr Uint32 SignalLength = 4 + MAX_REPLICAS;
  static constexpr Uint32 REORG_MOVING = 0x80000000;

  Uint32 zero;
  Uint32 fragId;
  Uint32 reqinfo;
  Uint32 instanceKey;
  Uint32 nodes[MAX_REPLICAS + (3 + MAX_REPLICAS)]; //+1
};
/**
 * 
 */
class DiGetNodesReq {
  /**
   * Sender(s)
   */
  friend class Dbtc;
  friend class Dbspj;
  friend class Backup;
  friend class Suma;
  /**
   * Receiver(s)
   */
  friend class Dbdih;
public:
  static constexpr Uint32 SignalLength = 6 + (sizeof(void*) / sizeof(Uint32));
  static constexpr Uint32 MAX_DIGETNODESREQS = 16;
private:
  Uint32 tableId;
  Uint32 hashValue;
  Uint32 distr_key_indicator;
  Uint32 scan_indicator;
  Uint32 get_next_fragid_indicator;
  Uint32 anyNode;
  union {
    void * jamBufferPtr;
    Uint32 jamBufferStorage[2];
  };
};


#undef JAM_FILE_ID

#endif
