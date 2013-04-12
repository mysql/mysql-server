/* Copyright (C) 2008 MySQL AB, 2008 Sun Microsystems, Inc.
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef CREATE_NODEGROUP_HPP
#define CREATE_NODEGROUP_HPP

#include "SignalData.hpp"

struct CreateNodegroupReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class NdbDictInterface;
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printCREATE_NODEGROUP_REQ(FILE*, const Uint32*, Uint32, Uint16);

  STATIC_CONST( SignalLength = 10 );

  union {
    Uint32 senderData;
    Uint32 clientData;
  };
  union {
    Uint32 senderRef;
    Uint32 clientRef;
  };
  Uint32 requestInfo;
  Uint32 transId;
  Uint32 transKey;
  Uint32 nodegroupId; // RNIL == unspecified
  Uint32 nodes[4]; // 0 terminated
};

struct CreateNodegroupRef {
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * Sender(s) / Reciver(s)
   */
  friend class NdbDictInterface;

  /**
   * For printing
   */
  friend bool printCREATE_NODEGROUP_REF(FILE*, const Uint32*, Uint32, Uint16);

  STATIC_CONST( SignalLength = 7 );

  enum ErrorCode {
    NoError = 0,
    Busy = 701,
    NotMaster = 702,
    NoMoreObjectRecords = 710,
    InvalidFormat = 740,
    SingleUser = 299,
    InvalidNoOfNodesInNodegroup = 320,
    InvalidNodegroupId = 321,
    NodeAlreadyInNodegroup = 322,
    NodegroupInUse = 323,
    NoNodeAlive = 324
  };

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 masterNodeId;
  Uint32 errorCode;
  Uint32 errorLine;
  Uint32 errorNodeId;
  Uint32 transId;
};

struct CreateNodegroupConf {
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * Sender(s) / Reciver(s)
   */
  friend class NdbDictInterface;

  /**
   * For printing
   */
  friend bool printCREATE_NODEGROUP_CONF(FILE*, const Uint32*, Uint32, Uint16);

  STATIC_CONST( SignalLength = 4 );

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 nodegroupId;
  Uint32 transId;
};

#endif
