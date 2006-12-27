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

#ifndef MASTER_GCP_HPP
#define MASTER_GCP_HPP

#include <NodeBitmask.hpp>

/**
 * 
 */
class MasterGCPConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
    
public:
  STATIC_CONST( SignalLength = 8 + NdbNodeBitmask::Size );

  enum State {
    GCP_READY            = 0,
    GCP_PREPARE_RECEIVED = 1,
    GCP_COMMIT_RECEIVED  = 2,
    GCP_TC_FINISHED      = 3
  };
private:  
  /**
   * Data replied
   */
  Uint32 gcpState;
  Uint32 senderNodeId;
  Uint32 failedNodeId;
  Uint32 newGCP;
  Uint32 latestLCP;
  Uint32 oldestRestorableGCI;
  Uint32 keepGCI;
  Uint32 lcpActive[NdbNodeBitmask::Size];
};
/**
 * 
 */
class MasterGCPReq {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
    
public:
  STATIC_CONST( SignalLength = 2 );
private:
  Uint32 masterRef;
  Uint32 failedNodeId;
};

/**
 * 
 */
class MasterGCPRef {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
    
public:
  STATIC_CONST( SignalLength = 2 );
private:
  Uint32 senderNodeId;
  Uint32 failedNodeId;
};
#endif
