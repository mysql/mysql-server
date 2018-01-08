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
