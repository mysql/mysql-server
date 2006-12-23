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

#ifndef PREP_FAILREQREF_HPP
#define PREP_FAILREQREF_HPP

#include "SignalData.hpp"
#include <NodeBitmask.hpp>

/**
 * The Req signal is sent by Qmgr to Qmgr
 * and the Ref signal might be sent back
 *
 * NOTE that the signals are identical
 */
class PrepFailReqRef {

  /**
   * Sender(s) / Reciver(s)
   */
  friend class Qmgr;

  friend bool printPREPFAILREQREF(FILE * output, const Uint32 * theData, Uint32 len, Uint16 receiverBlockNo);
  
public:
  STATIC_CONST( SignalLength = 3 + NodeBitmask::Size );
private:
  
  Uint32 xxxBlockRef;
  Uint32 failNo;
  
  Uint32 noOfNodes;
  Uint32 theNodes[NodeBitmask::Size];
};

#endif
