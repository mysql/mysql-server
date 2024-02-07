/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

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
  static constexpr Uint32 SignalLength = 3;

  enum State {
    LCP_STATUS_IDLE = 0,
    LCP_STATUS_ACTIVE = 2,
    LCP_TAB_COMPLETED = 8,
    LCP_TAB_SAVED = 9
  };

  friend NdbOut &operator<<(NdbOut &, const State &);

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
  static constexpr Uint32 SignalLength = 2;

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
  static constexpr Uint32 SignalLength = 2;

 private:
  /**
   * Data replied
   */
  Uint32 senderNodeId;
  Uint32 failedNodeId;
};

#undef JAM_FILE_ID

#endif
