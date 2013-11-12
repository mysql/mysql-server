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

#ifndef MASTER_LCP_HPP
#define MASTER_LCP_HPP

#include <NdbOut.hpp>
#include "SignalData.hpp"

#define JAM_FILE_ID 48


/**
 * 
 */
class MasterLCPConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
    
  friend bool printMASTER_LCP_CONF(FILE *, const Uint32 *, Uint32, Uint16);  
public:
  STATIC_CONST( SignalLength = 3 );

  enum State {
    LCP_STATUS_IDLE        = 0,
    LCP_STATUS_ACTIVE      = 2,
    LCP_TAB_COMPLETED      = 8,
    LCP_TAB_SAVED          = 9
  };

  friend NdbOut& operator<<(NdbOut&, const State&);
  
private:  
  /**
   * Data replied
   */
  Uint32 senderNodeId;
  Uint32 lcpState;
  Uint32 failedNodeId;
};
/**
 * 
 */
class MasterLCPReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;

  friend bool printMASTER_LCP_REQ(FILE *, const Uint32 *, Uint32, Uint16);   
public:
  STATIC_CONST( SignalLength = 2 );
private:
  Uint32 masterRef;
  Uint32 failedNodeId;
};

class MasterLCPRef {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;

  friend bool printMASTER_LCP_REF(FILE *, const Uint32 *, Uint32, Uint16);   
public:
  STATIC_CONST( SignalLength = 2 );
private:  
  /**
   * Data replied
   */
  Uint32 senderNodeId;
  Uint32 failedNodeId;
};

#undef JAM_FILE_ID

#endif
