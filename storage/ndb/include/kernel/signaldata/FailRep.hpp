/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef FAIL_REP_HPP
#define FAIL_REP_HPP

#include "SignalData.hpp"

/**
 * 
 */
class FailRep {
  /**
   * Sender(s) & Reciver(s)
   */
  friend class Qmgr;
  
  /**
   * For printing
   */
  friend bool printFAIL_REP(FILE *, const Uint32 *, Uint32, Uint16);

public:
  STATIC_CONST( SignalLength = 2 );

  enum FailCause {
    ZOWN_FAILURE=0,
    ZOTHER_NODE_WHEN_WE_START=1,
    ZIN_PREP_FAIL_REQ=2,
    ZSTART_IN_REGREQ=3,
    ZHEARTBEAT_FAILURE=4,
    ZLINK_FAILURE=5,
    ZOTHERNODE_FAILED_DURING_START=6
  };

private:
  
  Uint32 failNodeId;
  Uint32 failCause;
};


#endif
