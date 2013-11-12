/* Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CREATE_NODEGROUP_IMPL_HPP
#define CREATE_NODEGROUP_IMPL_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 36


struct CreateNodegroupImplReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;
  friend class Tsman;
  friend class Lgman;

  /**
   * For printing
   */
  friend bool printCREATE_NODEGROUP_IMPL_REQ(FILE*, const Uint32*, Uint32, Uint16);

  STATIC_CONST( SignalLength = 6 + MAX_REPLICAS );

  enum {
    RT_PARSE    = 0x1,
    RT_PREPARE  = 0x2,
    RT_ABORT    = 0x3,
    RT_COMMIT   = 0x4,
    RT_COMPLETE = 0x5
  };

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 requestType;
  Uint32 nodegroupId;
  Uint32 gci_hi;
  Uint32 gci_lo;
  Uint32 nodes[MAX_REPLICAS]; // 0 terminated
};

struct CreateNodegroupImplRef {
  /**
   * Sender(s)
   */
  friend class Dbdict;
  friend class Tsman;
  friend class Lgman;

  /**
   * For printing
   */
  friend bool printCREATE_NODEGROUP_IMPL_REF(FILE*, const Uint32*, Uint32, Uint16);

  STATIC_CONST( SignalLength = 3 );

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;
};

struct CreateNodegroupImplConf {
  /**
   * Sender(s)
   */
  friend class Dbdict;
  friend class Tsman;
  friend class Lgman;

  /**
   * For printing
   */
  friend bool printCREATE_NODEGROUP_IMPL_CONF(FILE*, const Uint32*, Uint32, Uint16);

  STATIC_CONST( SignalLength = 4 );

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 gci_hi;
  Uint32 gci_lo;
};


#undef JAM_FILE_ID

#endif
