/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#ifndef MASTER_GCP_HPP
#define MASTER_GCP_HPP

#include <NodeBitmask.hpp>

#define JAM_FILE_ID 17


/**
 * 
 */
class MasterGCPConf {
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdih;
    
public:
  static constexpr Uint32 SignalLength = 10 + 2;

  enum State {
    GCP_READY            = 0,
    /**
     * GCP_PREPARE received (and replied)
     */
    GCP_PREPARE_RECEIVED = 1,

    /**
     * GCP_COMMIT received (not replied)
     */
    GCP_COMMIT_RECEIVED  = 2, // GCP_COMMIT received (and is running)

    /**
     * Replied GCP_NODEFINISH
     *   (i.e GCP_COMMIT finished)
     */
    GCP_COMMITTED = 3
  };

  enum SaveState {
    GCP_SAVE_IDLE     = 0,
    /**
     * GCP_SAVE_REQ received (running in LQH)
     */
    GCP_SAVE_REQ      = 1,

    /**
     * GCP_SAVE_CONF (or REF)
     */
    GCP_SAVE_CONF     = 2,

    /**
     * COPY_GCI_REQ (GCP) has been received and is running
     */
    GCP_SAVE_COPY_GCI = 3
  };

  struct Upgrade {
    /**
     * States uses before micro GCP
     */
    enum State {
      GCP_READY            = 0,
      GCP_PREPARE_RECEIVED = 1,
      GCP_COMMIT_RECEIVED  = 2,
      GCP_TC_FINISHED      = 3
    };
  };

private:  
  /**
   * Data replied
   */
  Uint32 gcpState;
  Uint32 senderNodeId;
  Uint32 failedNodeId;
  Uint32 newGCP_hi;
  Uint32 latestLCP;
  Uint32 oldestRestorableGCI;
  Uint32 keepGCI;
  Uint32 lcpActive_v1[NdbNodeBitmask48::Size];
  Uint32 newGCP_lo;
  Uint32 saveState;
  Uint32 saveGCI;
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
  static constexpr Uint32 SignalLength = 2;
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
  static constexpr Uint32 SignalLength = 2;
private:
  Uint32 senderNodeId;
  Uint32 failedNodeId;
};

#undef JAM_FILE_ID

#endif
