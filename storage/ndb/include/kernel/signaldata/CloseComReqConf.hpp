/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef CLOSE_COMREQCONF_HPP
#define CLOSE_COMREQCONF_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

#define JAM_FILE_ID 174


/**
 * The Req signal is sent by Qmgr to Cmvmi
 * and the Conf signal is sent back
 *
 * NOTE that the signals are identical
 */
struct CloseComReqConf {

  /**
   * Sender(s) / Reciver(s)
   */
  friend class Qmgr;
  friend class Trpman;
  friend class TrpmanProxy;

  /**
   * For printing
   */
  friend bool printCLOSECOMREQCONF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);

  STATIC_CONST( SignalLength = 4 + NodeBitmask::Size );

  enum RequestType {
    RT_API_FAILURE   = 0,
    RT_NODE_FAILURE  = 1,
    RT_NO_REPLY      = 2
  };

  Uint32 xxxBlockRef;
  Uint32 requestType;
  Uint32 failNo;
  
  Uint32 noOfNodes;
  Uint32 theNodes[NodeBitmask::Size];
};


#undef JAM_FILE_ID

#endif
