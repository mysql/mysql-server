/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef CREATE_FRAG_HPP
#define CREATE_FRAG_HPP

class CreateFragReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
public:
  STATIC_CONST( SignalLength = 8 );

  enum ReplicaType {
    STORED = 7,
    COMMIT_STORED = 9
  };
private:

  Uint32 userPtr;
  BlockReference userRef;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 startingNodeId;
  Uint32 copyNodeId;
  Uint32 startGci;
  Uint32 replicaType;
};

class CreateFragConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
  
public:
  STATIC_CONST( SignalLength = 5 );
private:
  
  Uint32 userPtr;
  Uint32 tableId;
  Uint32 fragId;
  Uint32 sendingNodeId;
  Uint32 startingNodeId;
};
#endif
