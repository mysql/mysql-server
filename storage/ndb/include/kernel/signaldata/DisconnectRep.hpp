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

#ifndef DISCONNECT_REP_HPP
#define DISCONNECT_REP_HPP

#include "SignalData.hpp"

/**
 * 
 */
class DisconnectRep {
  /**
   * Receiver(s)
   */
  friend class Qmgr;
  friend class Cmvmi; // Cmvmi

  /**
   * Senders
   */
  friend class Dbtc;
  friend void reportDisconnect(void * , NodeId, Uint32); // TransporterCallback
  
  /**
   * For printing
   */
  friend bool printDISCONNECT_REP(FILE *, const Uint32 *, Uint32, Uint16);

public:
  STATIC_CONST( SignalLength = 2 );

  enum ErrCode {
    // ErrorCodes come from different sources
    // for example TransporterCallback.hpp
    // or inet errno
    // This one is selected not to conflict with any of them
    TcReportNodeFailed = 0xFF000001
  };

private:
  
  Uint32 nodeId;
  Uint32 err;
};


#endif
