/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NF_COMPLETE_REP_HPP
#define NF_COMPLETE_REP_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 150


/**
 * NFCompleteRep - Node Fail Complete Report
 *
 * This signal is sent by a block(or a node)
 * when it has finished cleaning up after a node failure.
 *
 * It's also sent from Qmgr to the clusterMgr in API
 * to tell the API that it can now abort all transactions still waiting for response
 * from the failed NDB node
 *
 */
struct NFCompleteRep {

  friend bool printNF_COMPLETE_REP(FILE *, const Uint32 *, Uint32, Uint16);
  
  STATIC_CONST( SignalLength = 5 );

  /**
   * Which block has completed...
   *
   * NOTE: 0 means the node has completed
   */
  Uint32 blockNo;
  
  /**
   * Which node has completed...
   */
  Uint32 nodeId;
  
  /**
   * Which node has failed
   */
  Uint32 failedNodeId;

  /**
   * Is this the original message or a delayed variant.
   */
  Uint32 unused; // originalMessage

  Uint32 from;
};


#undef JAM_FILE_ID

#endif
